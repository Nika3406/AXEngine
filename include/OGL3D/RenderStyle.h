#pragma once

#include <string>

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct StylizedLightingSettings {
    // ENGINE DATA CONTAINER ONLY.
    // Do not tune game/level visuals here. Put real level values in scripts/Environment.h.
    Vec3f sunDirection   = {0.0f, -1.0f, 0.0f};
    Vec3f sunColor       = {1.0f, 1.0f, 1.0f};
    float sunIntensity   = 1.0f;

    Vec3f ambientSky     = {0.0f, 0.0f, 0.0f};
    Vec3f ambientGround  = {0.0f, 0.0f, 0.0f};

    Vec3f shadowColor    = {0.0f, 0.0f, 0.0f};
    float shadowStrength = 0.0f;
    float minLight       = 0.0f;

    bool realtimeShadows = false;
    float shadowBias = 0.0015f;
    float shadowMapSoftness = 1.0f;

    // Directional shadow camera size in world units. Smaller = sharper/contact shadows,
    // larger = covers more world. Environment.h owns the actual values.
    float shadowOrthoRange = 80.0f;

    float shadowSoftness = 0.0f;

    Vec3f rimColor       = {1.0f, 1.0f, 1.0f};
    float rimIntensity   = 0.0f;
    float rimPower       = 3.0f;

    Vec3f fogColor       = {0.0f, 0.0f, 0.0f};
    float fogDensity     = 0.0f;

    float celStrength    = 0.0f;
    float celBandSoftness = 0.0f;
};

struct ColorGradeSettings {
    // ENGINE DATA CONTAINER ONLY.
    // Real values are authored in scripts/Environment.h.
    float exposure   = 1.0f;
    float saturation = 1.0f;
    float contrast   = 1.0f;
    float gamma      = 2.2f;
};

struct SkySettings {
    enum class Mode {
        Color,
        Texture
    };

    bool enabled = false;
    Mode mode = Mode::Color;
    Vec3f color = {0.0f, 0.0f, 0.0f};
    std::string panoramaTexturePath;

    float intensity = 0.0f;
    float rotation = 0.0f;
    float saturation = 1.0f;
    float contrast   = 1.0f;
};

// Engine-side feature flags. These are not level tuning values; they are only
// the switchboard that render.cpp uploads to shaders. Environment.h decides
// what these should be for each game/level profile.
struct RenderFeatureSettings {
    bool stylizedLighting = false;
    bool fog = false;
    bool celShading = false;
    bool rimLighting = false;
    bool materialEmission = true;
    bool bloom = false;
};

// Engine-side bloom container. Environment.h owns the actual values.
struct BloomSettings {
    bool enabled = false;
    float threshold = 0.0f;
    float strength = 0.0f;
    float radius = 1.0f;
};

struct RenderStyle {
    // ---------------------------------------------------------------------
    // Legacy/profile flags kept so scripts/Environment.h can own the values
    // without needing render.cpp to know about EnvironmentRenderSettings.
    // ---------------------------------------------------------------------
    bool stylizedLightingEnabled = false;
    bool skyEnabled = false;
    bool fogEnabled = false;
    bool celShadingEnabled = false;
    bool rimLightingEnabled = false;
    bool bloomEnabled = false;

    StylizedLightingSettings lighting;
    ColorGradeSettings colorGrade;
    SkySettings sky;

    // New renderer-facing containers expected by render.cpp.
    RenderFeatureSettings features;
    BloomSettings bloom;

    // Keep renderer-facing flags synchronized from the legacy flags that
    // scripts/Environment.cpp fills from Environment.h.
    void syncFeaturesFromLegacyFlags() {
        features.stylizedLighting = stylizedLightingEnabled;
        features.fog = fogEnabled;
        features.celShading = celShadingEnabled;
        features.rimLighting = rimLightingEnabled;
        features.materialEmission = true;
        features.bloom = bloomEnabled;

        sky.enabled = skyEnabled;
        bloom.enabled = bloomEnabled;
    }

    // Backward-compatible constructors. They intentionally return neutral
    // container values. Do not use these as game art-direction presets.
    static RenderStyle DefaultStylized();
    static RenderStyle ShadowColossusLike();
    static RenderStyle AsuraLike();
    static RenderStyle DarkCelestialBrutalism();
};
