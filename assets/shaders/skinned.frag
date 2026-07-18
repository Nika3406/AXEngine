#version 330 core

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUV;
in vec4 vLightSpacePos;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 BrightColor;

uniform sampler2D uBaseColorTexture;
uniform int uHasBaseColorTexture;

uniform vec3 uColor;
uniform float uAlpha;
uniform int uAlphaMode;
uniform float uAlphaCutoff;

// Camera / lighting
uniform vec3 uCameraPos;

uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunIntensity;

uniform vec3 uAmbientSky;
uniform vec3 uAmbientGround;

// Rim
uniform vec3 uRimColor;
uniform float uRimIntensity;
uniform float uRimPower;

// Fog
uniform vec3 uFogColor;
uniform float uFogDensity;

// Stylization
uniform float uCelStrength;
uniform float uShadowSoftness;

// Color grading
uniform float uExposure;
uniform float uSaturation;
uniform float uContrast;
uniform float uGamma;

// Shadow mapping
uniform sampler2D uShadowMap;
uniform int uUseShadows;
uniform float uShadowBias;
uniform float uShadowMapSoftness;
uniform mat4 uLightSpaceMatrix;
uniform float uShadowTexelWorldSize;

uniform vec3 uShadowColor;
uniform float uShadowStrength;
uniform float uMinLight;

// VFX: only enabled by C++ for sword primitives whose material is white/light.
uniform int uSwordWhiteEmission;
uniform vec3 uSwordEmissionColor;
uniform float uSwordEmissionStrength;

// Bloom control. Bloom is no longer brightness-based.
// C++ sets this per primitive/material.
uniform float uBloomMask;
uniform vec3 uBloomTint;
uniform float uBloomStrength;
uniform int uExplicitBloomMaterial;

// Render profile feature toggles. These let AXEngine remain multipurpose:
// block/world prototypes can disable cinematic fog/rim/cel/bloom, while
// action profiles can turn them back on.
uniform int uUseStylizedLighting;
uniform int uUseFog;
uniform int uUseCelShading;
uniform int uUseRimLighting;
uniform int uUseMaterialEmission;
uniform int uUseBloom;
uniform int uUseGeometryNormals;

vec3 stableNormal(vec3 meshNormal) {
    vec3 N = normalize(meshNormal);

    if (uUseGeometryNormals != 0) {
        // Static graybox/architecture imports often have ugly or inconsistent
        // vertex normals on heavily triangulated walls. Use a screen-space
        // geometric normal for static scene surfaces so lighting does not reveal
        // the triangulation as faint diagonal bands.
        vec3 gx = dFdx(vWorldPos);
        vec3 gy = dFdy(vWorldPos);
        vec3 gN = normalize(cross(gx, gy));
        if (dot(gN, N) < 0.0) {
            gN = -gN;
        }
        N = gN;
    }

    return N;
}

vec3 applySaturation(vec3 color, float sat) {
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luma), color, sat);
}

vec3 applyContrast(vec3 color, float contrastValue) {
    // Contrast around middle-gray instead of 0.5.  After correct sRGB texture
    // decoding, many baked albedo textures land in the 0.05-0.25 range.
    // Pivoting contrast at 0.5 crushes those details to black.  A middle-gray
    // pivot keeps dark wall/metal texture readable while still allowing a
    // stylized high-contrast look.
    vec3 pivot = vec3(0.18);
    return (color - pivot) * contrastValue + pivot;
}

vec3 applyToneMapAndGrade(vec3 hdrColor) {
    float exposure = max(uExposure, 0.001);
    float saturation = (uSaturation <= 0.001) ? 1.0 : uSaturation;
    float contrast = (uContrast <= 0.001) ? 1.0 : uContrast;
    float gammaValue = (uGamma <= 0.001) ? 2.2 : uGamma;

    vec3 color = hdrColor * exposure;

    // Filmic-ish Reinhard. This keeps the gray-box readable while allowing
    // red/cyan emissive accents to feel bright without turning the whole frame white.
    color = color / (color + vec3(1.0));

    // Important: do the display/gamma lift BEFORE contrast.
    // Applying contrast in linear space crushed very dark baked textures like
    // Building3Baked into pure black before gamma ever had a chance to lift them.
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / gammaValue));

    color = applySaturation(color, saturation);
    color = applyContrast(color, contrast);
    color = clamp(color, 0.0, 1.0);

    return color;
}

float sampleShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
    if (uUseShadows == 0) {
        return 0.0;
    }

    vec3 N = normalize(normal);
    vec3 L = normalize(lightDir);
    float ndotl = max(dot(N, L), 0.0);

    // Stable receiver offset + simple PCF. The previous receiver-plane gradient
    // correction was too sensitive on large UV-baked architectural meshes and
    // could show up as diagonal/triangle shadow lines. This version favors clean
    // contact shadows over mathematically aggressive per-tap correction.
    float texelWorld = max(uShadowTexelWorldSize, 0.01);
    float receiverNormalOffset = texelWorld * mix(0.35, 0.90, 1.0 - ndotl);

    vec4 lightSpacePos = uLightSpaceMatrix * vec4(worldPos + N * receiverNormalOffset, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    int radius = int(clamp(uShadowMapSoftness, 0.0, 2.0));

    float baseBias = max(uShadowBias, 0.00012);
    float bias = baseBias + (1.0 - ndotl) * 0.00018;
    float transition = max(bias * 2.5, 0.00012);

    float shadow = 0.0;
    int samples = 0;

    for (int x = -radius; x <= radius; ++x) {
        for (int y = -radius; y <= radius; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float closestDepth = texture(uShadowMap, projCoords.xy + offset).r;
            float delta = projCoords.z - closestDepth;
            shadow += smoothstep(bias, bias + transition, delta);
            samples++;
        }
    }

    float averaged = shadow / float(max(samples, 1));
    return clamp(averaged, 0.0, 1.0);
}

float softCel(float ndotl) {
    // Three-weight feel: ambient base, mid band, hot edge.
    // Still soft enough for Blender gray-boxes and simple GLB materials.
    float mid = smoothstep(0.22 - uShadowSoftness, 0.22 + uShadowSoftness, ndotl);
    float high = smoothstep(0.72 - uShadowSoftness, 0.72 + uShadowSoftness, ndotl);
    float banded = 0.38 + mid * 0.38 + high * 0.24;
    return mix(ndotl, banded, clamp(uCelStrength, 0.0, 1.0));
}

vec3 brutalistMaterialGrade(vec3 baseColor) {
    // Keep gray/concrete/stone quiet and monumental. Colored materials remain colored.
    float luma = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));
    float chroma = length(baseColor - vec3(luma));

    vec3 coldStone = mix(vec3(luma), vec3(luma) * vec3(0.84, 0.87, 0.94), 0.45);
    float grayish = 1.0 - smoothstep(0.035, 0.20, chroma);

    return mix(baseColor, coldStone, grayish * 0.35);
}

vec3 materialAccentEmission(vec3 baseColor, vec3 texColor) {
    // Blender-friendly rule: make material/color red, cyan, or gold and the shader
    // treats it as controlled energy. No new material system required yet.
    vec3 c = clamp(baseColor * texColor, 0.0, 4.0);

    float redDominance = c.r - max(c.g, c.b) * 1.25;
    float redEnergy = smoothstep(0.08, 0.55, redDominance) * smoothstep(0.22, 0.85, c.r);

    float cyanDominance = min(c.g, c.b) - c.r * 1.15;
    float cyanEnergy = smoothstep(0.08, 0.50, cyanDominance) * smoothstep(0.25, 0.90, max(c.g, c.b));

    float goldDominance = min(c.r, c.g) - c.b * 1.25;
    float goldEnergy = smoothstep(0.08, 0.45, goldDominance) * smoothstep(0.35, 0.95, max(c.r, c.g));

    vec3 redGlow = vec3(1.0, 0.05, 0.015) * redEnergy * 3.8;
    vec3 cyanGlow = vec3(0.08, 0.95, 1.0) * cyanEnergy * 2.2;
    vec3 goldGlow = vec3(1.0, 0.70, 0.18) * goldEnergy * 1.8;

    return redGlow + cyanGlow + goldGlow;
}

vec3 swordWhiteEmission(vec3 baseColor, vec3 texColor) {
    if (uSwordWhiteEmission == 0 || uSwordEmissionStrength <= 0.001) {
        return vec3(0.0);
    }

    // Detect the white/light part of the sword material. This allows a sword
    // texture to have only its white regions glow instead of lighting the whole mesh.
    vec3 c = clamp(baseColor * texColor, 0.0, 4.0);
    float maxC = max(max(c.r, c.g), c.b);
    float minC = min(min(c.r, c.g), c.b);
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float chroma = maxC - minC;

    float brightMask = smoothstep(0.62, 0.92, luma);
    float whiteMask = 1.0 - smoothstep(0.05, 0.24, chroma);
    float mask = brightMask * whiteMask;

    // Fresnel makes the blade edge/readability stronger without making the
    // flat face look like a solid white rectangle.
    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.2);
    float edgeBoost = mix(0.70, 1.45, fresnel);

    return uSwordEmissionColor * mask * edgeBoost * uSwordEmissionStrength;
}

void main() {
    vec4 tex = vec4(1.0);
    if (uHasBaseColorTexture != 0) {
        tex = texture(uBaseColorTexture, vUV);
    }

    // glTF rule: baseColorTexture alpha only matters for MASK/BLEND materials.
    // OPAQUE baked textures may contain transparent/black padding around UV islands;
    // using that alpha on opaque architecture causes accidental holes/dark edges.
    float alpha = uAlpha;
    if (uAlphaMode != 0) {
        alpha *= tex.a;
    }

    // Alpha mode: 0 opaque, 1 mask, 2 blend.
    if (uAlphaMode == 1 && alpha < uAlphaCutoff) {
        discard;
    }
    if (uAlphaMode == 2 && alpha < 0.02) {
        discard;
    }

    vec3 rawBaseColor = uColor * tex.rgb;
    vec3 baseColor = (uUseStylizedLighting != 0)
        ? brutalistMaterialGrade(rawBaseColor)
        : rawBaseColor;

    vec3 N = stableNormal(vWorldNormal);
    vec3 L = normalize(-uSunDirection);
    vec3 V = normalize(uCameraPos - vWorldPos);

    float ndotl = max(dot(N, L), 0.0);
    float litBand = (uUseCelShading != 0) ? softCel(ndotl) : ndotl;

    // Hemisphere ambient: cold sky above, dirty dark bounce below.
    float up = N.y * 0.5 + 0.5;
    vec3 ambient = mix(uAmbientGround, uAmbientSky, up);

    float realtimeShadow = sampleShadow(vWorldPos, N, L);
    float shadowFactor = 1.0 - realtimeShadow * clamp(uShadowStrength, 0.0, 1.0);

    // Keep ambient/fill light out of the realtime shadow test. The previous shader
    // tinted ambient with the shadow map, which made tiny self-shadow errors visible
    // as gray zebra/triangle lines. Shadows should primarily remove direct sun.
    float directLight = litBand;
    float fillLight = uMinLight;

    vec3 hdr =
        baseColor * ambient +
        baseColor * uSunColor * (directLight * shadowFactor + fillLight * 0.35) * uSunIntensity;

    // Directional half-rim for silhouettes against the fog/sky.
    if (uUseRimLighting != 0) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        rim = pow(rim, max(uRimPower, 0.001));
        float upperRim = smoothstep(-0.25, 0.85, N.y);
        hdr += uRimColor * rim * uRimIntensity * mix(0.65, 1.25, upperRim);
    }

    // Red/cyan/gold colors are only emissive when C++ marks the material as
    // explicit emission/bloom.  This prevents normal red trim or white walls
    // from being treated like VFX just because of their color.
    vec3 accentEmission = (uUseMaterialEmission != 0 && uExplicitBloomMaterial != 0)
        ? materialAccentEmission(uColor, tex.rgb)
        : vec3(0.0);

    // Sword-only white material glow. C++ enables this only for sword primitives,
    // while the shader masks it to the actual white/light part of the material.
    vec3 swordEmission = (uUseMaterialEmission != 0)
        ? swordWhiteEmission(uColor, tex.rgb)
        : vec3(0.0);

    // Explicit material bloom is controlled by Blender material names such as:
    // BLOOM_*, EMISSIVE_*, or GLOW_*.
    // This is separate from brightness so skies/white walls do not bloom.
    vec3 explicitMaterialEmission = vec3(0.0);
    if (uUseMaterialEmission != 0 && uExplicitBloomMaterial != 0 && uBloomStrength > 0.001) {
        explicitMaterialEmission = uBloomTint * uBloomStrength;
    }

    vec3 emissionForBloom = accentEmission + swordEmission + explicitMaterialEmission;
    hdr += emissionForBloom;

    // Subtle warm glancing light from the sun side to sell the concept-art sky.
    float sunFacing = pow(max(dot(reflect(-L, N), V), 0.0), 16.0);
    hdr += uSunColor * sunFacing * 0.14;

    // Fog before tone mapping. Heavy atmospheric scale hides low-detail distance.
    float dist = length(uCameraPos - vWorldPos);

    if (uUseFog != 0 && uFogDensity > 0.000001) {
        float linearFog = smoothstep(75.0, 380.0, dist);
        float expFog = 1.0 - exp(-dist * max(uFogDensity, 0.0));

        // Extra low-lying haze around the ground plane and mega-structure bases.
        float heightHaze = 1.0 - smoothstep(-8.0, 28.0, vWorldPos.y);
        float fogAmount = max(max(linearFog, expFog), heightHaze * 0.16);
        fogAmount = clamp(fogAmount, 0.0, 0.94);

        hdr = mix(hdr, uFogColor, fogAmount);
    }

    vec3 finalColor = applyToneMapAndGrade(hdr);

    FragColor = vec4(finalColor, alpha);

    // Bloom target: material-controlled emission only.
    // No brightness threshold here: bright sky/clouds/walls must not bloom unless
    // C++ explicitly enables uBloomMask for that material/primitive.
    BrightColor = (uUseBloom != 0)
        ? vec4(emissionForBloom * clamp(uBloomMask, 0.0, 1.0), alpha)
        : vec4(0.0, 0.0, 0.0, alpha);
}
