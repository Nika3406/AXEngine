#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>

#include "OGL3D/render.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "ecs/RenderComponent.h"
#include "ecs/TransformComponent.h"
#include "ecs/SkinnedRenderComponent.h"
#include "ecs/StaticSceneRenderComponent.h"
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static std::string axLowerCopy(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

static bool axStartsWithIgnoreCase(const std::string& value, const std::string& prefix) {
    std::string v = axLowerCopy(value);
    std::string p = axLowerCopy(prefix);
    return v.rfind(p, 0) == 0;
}

static bool axMaterialNameHasExplicitEmission(const std::string& name) {
    return axStartsWithIgnoreCase(name, "BLOOM_") ||
    axStartsWithIgnoreCase(name, "EMISSIVE_") ||
    axStartsWithIgnoreCase(name, "GLOW_") ||
    axStartsWithIgnoreCase(name, "AX_BLOOM_") ||
    axStartsWithIgnoreCase(name, "AX_EMIT_") ||
    axStartsWithIgnoreCase(name, "VFX_");
}

static bool axMaterialNameHasExplicitBloom(const std::string& name) {
    return axStartsWithIgnoreCase(name, "BLOOM_") ||
    axStartsWithIgnoreCase(name, "AX_BLOOM_") ||
    axStartsWithIgnoreCase(name, "VFX_");
}

static void resetMaterialBloomUniforms(unsigned int program) {
    if (!program) return;

    int locBloomMask = glGetUniformLocation(program, "uBloomMask");
    int locBloomTint = glGetUniformLocation(program, "uBloomTint");
    int locBloomStrength = glGetUniformLocation(program, "uBloomStrength");
    int locExplicitBloomMaterial = glGetUniformLocation(program, "uExplicitBloomMaterial");
    int locSwordWhiteEmission = glGetUniformLocation(program, "uSwordWhiteEmission");
    int locSwordEmissionStrength = glGetUniformLocation(program, "uSwordEmissionStrength");
    int locSwordEmissionColor = glGetUniformLocation(program, "uSwordEmissionColor");

    if (locBloomMask >= 0) glUniform1f(locBloomMask, 0.0f);
    if (locBloomTint >= 0) glUniform3f(locBloomTint, 0.0f, 0.0f, 0.0f);
    if (locBloomStrength >= 0) glUniform1f(locBloomStrength, 0.0f);
    if (locExplicitBloomMaterial >= 0) glUniform1i(locExplicitBloomMaterial, 0);

    if (locSwordWhiteEmission >= 0) glUniform1i(locSwordWhiteEmission, 0);
    if (locSwordEmissionStrength >= 0) glUniform1f(locSwordEmissionStrength, 0.0f);
    if (locSwordEmissionColor >= 0) glUniform3f(locSwordEmissionColor, 0.0f, 0.0f, 0.0f);
}

static void resetTextureMaterialUniforms(unsigned int program) {
    if (!program) return;

    int locHasBase = glGetUniformLocation(program, "uHasBaseColorTexture");
    int locBaseTex = glGetUniformLocation(program, "uBaseColorTexture");
    int locAlpha = glGetUniformLocation(program, "uAlpha");
    int locAlphaMode = glGetUniformLocation(program, "uAlphaMode");
    int locCutoff = glGetUniformLocation(program, "uAlphaCutoff");

    if (locBaseTex >= 0) glUniform1i(locBaseTex, 0);
    if (locHasBase >= 0) glUniform1i(locHasBase, 0);
    if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
    if (locAlphaMode >= 0) glUniform1i(locAlphaMode, 0);
    if (locCutoff >= 0) glUniform1f(locCutoff, 0.5f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ---------------------------------------------------------------------------
// Column-major matrix math
// ---------------------------------------------------------------------------

static void mat4Identity(float* m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4Mul(const float* A, const float* B, float* out) {
    float tmp[16] = {};
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                tmp[col*4+row] += A[k*4+row] * B[col*4+k];
    memcpy(out, tmp, 64);
}

static bool mat4Inverse(const float* m, float* invOut) {
    float inv[16];

    inv[0] = m[5]  * m[10] * m[15] - m[5]  * m[11] * m[14] - m[9]  * m[6]  * m[15] + m[9]  * m[7]  * m[14] + m[13] * m[6]  * m[11] - m[13] * m[7]  * m[10];
    inv[4] = -m[4]  * m[10] * m[15] + m[4]  * m[11] * m[14] + m[8]  * m[6]  * m[15] - m[8]  * m[7]  * m[14] - m[12] * m[6]  * m[11] + m[12] * m[7]  * m[10];
    inv[8] = m[4]  * m[9] * m[15] - m[4]  * m[11] * m[13] - m[8]  * m[5] * m[15] + m[8]  * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4]  * m[9] * m[14] + m[4]  * m[10] * m[13] + m[8]  * m[5] * m[14] - m[8]  * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1]  * m[10] * m[15] + m[1]  * m[11] * m[14] + m[9]  * m[2] * m[15] - m[9]  * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0]  * m[10] * m[15] - m[0]  * m[11] * m[14] - m[8]  * m[2] * m[15] + m[8]  * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0]  * m[9] * m[15] + m[0]  * m[11] * m[13] + m[8]  * m[1] * m[15] - m[8]  * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0]  * m[9] * m[14] - m[0]  * m[10] * m[13] - m[8]  * m[1] * m[14] + m[8]  * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1]  * m[6] * m[15] - m[1]  * m[7] * m[14] - m[5]  * m[2] * m[15] + m[5]  * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0]  * m[6] * m[15] + m[0]  * m[7] * m[14] + m[4]  * m[2] * m[15] - m[4]  * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0]  * m[5] * m[15] - m[0]  * m[7] * m[13] - m[4]  * m[1] * m[15] + m[4]  * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0]  * m[5] * m[14] + m[0]  * m[6] * m[13] + m[4]  * m[1] * m[14] - m[4]  * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (fabsf(det) < 1e-8f) {
        mat4Identity(invOut);
        return false;
    }
    det = 1.0f / det;
    for (int i = 0; i < 16; ++i) invOut[i] = inv[i] * det;
    return true;
}

static void mat4Translate(float x, float y, float z, float* m) {
    mat4Identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4Scale(float x, float y, float z, float* m) {
    mat4Identity(m);
    m[0] = x; m[5] = y; m[10] = z;
}

// Rotation around Y axis (degrees) — enough for a side-by-side test scene
static void mat4RotateY(float deg, float* m) {
    float rad = deg * 3.14159265f / 180.0f;
    float c = cosf(rad), s = sinf(rad);
    mat4Identity(m);
    m[0]  =  c;  m[8]  = s;
    m[2]  = -s;  m[10] = c;
}

static void mat4RotateX(float deg, float* m) {
    float rad = deg * 3.14159265f / 180.0f;
    float c = cosf(rad), s = sinf(rad);
    mat4Identity(m);
    m[5] = c;  m[9]  = -s;
    m[6] = s;  m[10] =  c;
}

static void makePerspective(float fovYDeg, float aspect, float nearZ, float farZ, float* m) {
    memset(m, 0, 64);
    float f = 1.0f / tanf(fovYDeg * 3.14159265f / 360.0f);
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (farZ + nearZ) / (nearZ - farZ);
    m[11] = -1.0f;
    m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
}

static void makeOrtho(
    float left, float right,
    float bottom, float top,
    float nearZ, float farZ,
    float* m
) {
    memset(m, 0, 64);

    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -2.0f / (farZ - nearZ);

    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(farZ + nearZ) / (farZ - nearZ);
    m[15] = 1.0f;
}

static void makeLookAt(
    float eyeX, float eyeY, float eyeZ,
    float centerX, float centerY, float centerZ,
    float upX, float upY, float upZ,
    float* m
) {
    float fx = centerX - eyeX;
    float fy = centerY - eyeY;
    float fz = centerZ - eyeZ;

    float fl = sqrtf(fx*fx + fy*fy + fz*fz);
    if (fl < 0.0001f) fl = 1.0f;
    fx /= fl; fy /= fl; fz /= fl;

    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;

    float sl = sqrtf(sx*sx + sy*sy + sz*sz);
    if (sl < 0.0001f) sl = 1.0f;
    sx /= sl; sy /= sl; sz /= sl;

    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    m[0] = sx;  m[1] = ux;  m[2]  = -fx; m[3]  = 0.0f;
    m[4] = sy;  m[5] = uy;  m[6]  = -fy; m[7]  = 0.0f;
    m[8] = sz;  m[9] = uz;  m[10] = -fz; m[11] = 0.0f;

    m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    m[14] =  (fx * eyeX + fy * eyeY + fz * eyeZ);
    m[15] = 1.0f;
}

// Build model matrix from a TransformComponent
static void buildModelMatrix(const TransformComponent* t, float* model) {
    float T[16], Rx[16], Ry[16], S[16], tmp[16];
    mat4Translate(t->position.x, t->position.y, t->position.z, T);
    mat4RotateX(t->rotation.x, Rx);
    mat4RotateY(t->rotation.y, Ry);
    mat4Scale(t->scale.x, t->scale.y, t->scale.z, S);

    // model = T * Ry * Rx * S
    mat4Mul(T,   Ry,  tmp);
    mat4Mul(tmp, Rx,  model);
    mat4Mul(model, S, tmp);
    memcpy(model, tmp, 64);
}


static unsigned int uploadGltfImageTexture(const GltfImageData& img) {
    if (!img.valid()) return 0;

    GLenum format = GL_RGBA;
    GLenum internalFormat = GL_RGBA;

    // glTF baseColorTexture images are authored in sRGB color space.
    // Store them as GL_SRGB* so texture() returns LINEAR values to the shader.
    // Without this, mid-gray 0.5 is treated as linear 0.5 instead of ~0.214,
    // which makes baked Blender textures look washed-out / too bright.
    if (img.component == 1) { format = GL_RED;  internalFormat = GL_RED;  }
    if (img.component == 2) { format = GL_RG;   internalFormat = GL_RG;   }
    if (img.component == 3) { format = GL_RGB;  internalFormat = GL_SRGB8; }
    if (img.component == 4) { format = GL_RGBA; internalFormat = GL_SRGB8_ALPHA8; }

    GLenum type = (img.bits == 16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 img.width, img.height, 0,
                 format, type, img.pixels.data());

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}


static unsigned int uploadStaticSceneImageTexture(const StaticSceneImageData& img) {
    if (!img.valid()) return 0;

    GLenum format = GL_RGBA;
    GLenum internalFormat = GL_RGBA;

    // Same color-space rule as the skinned GLB path: base color/albedo
    // textures are sRGB, while the shader lighting math expects linear RGB.
    if (img.component == 1) { format = GL_RED;  internalFormat = GL_RED;  }
    if (img.component == 2) { format = GL_RG;   internalFormat = GL_RG;   }
    if (img.component == 3) { format = GL_RGB;  internalFormat = GL_SRGB8; }
    if (img.component == 4) { format = GL_RGBA; internalFormat = GL_SRGB8_ALPHA8; }

    GLenum type = (img.bits == 16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 img.width, img.height, 0,
                 format, type, img.pixels.data());

    // Static level textures in AXEngine are usually Blender-baked atlases with
    // large black/empty padding outside UV islands. Generating mipmaps for those
    // atlases averages the padding into the islands and creates the dark/light
    // seam lines that look like UV/shadow corruption in-game. Keep the runtime
    // static scene path mip-free and clamp small UV overshoot at island borders.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}


static void setVec3(unsigned int program, const char* name, float x, float y, float z) {
    int loc = glGetUniformLocation(program, name);
    if (loc >= 0) {
        glUniform3f(loc, x, y, z);
    }
}

static void setFloat(unsigned int program, const char* name, float value) {
    int loc = glGetUniformLocation(program, name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

static void setInt(unsigned int program, const char* name, int value) {
    int loc = glGetUniformLocation(program, name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

static unsigned int compileShaderSource(unsigned int type, const char* src, const char* label) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "[Renderer] Shader compile failed (" << label << "): " << log << "\n";
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static unsigned int linkProgramSources(const char* vertSrc, const char* fragSrc, const char* label) {
    unsigned int vs = compileShaderSource(GL_VERTEX_SHADER, vertSrc, "sky vert");
    unsigned int fs = compileShaderSource(GL_FRAGMENT_SHADER, fragSrc, "sky frag");
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "[Renderer] Program link failed (" << label << "): " << log << "\n";
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static bool fileExists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static std::string resolveRuntimePath(const std::string& requested) {
    if (requested.empty()) {
        return requested;
    }

    if (fileExists(requested)) {
        return requested;
    }

    const char* baseRaw = SDL_GetBasePath();
    const std::string base = baseRaw ? std::string(baseRaw) : std::string();

    std::vector<std::string> candidates;
    if (!base.empty()) {
        candidates.push_back(base + requested);
        candidates.push_back(base + "../" + requested);
        candidates.push_back(base + "../../" + requested);
    }

    // Canonical project assets live under assets/. Keep a specific panorama
    // fallback for older project files that omitted the texture path.
    candidates.push_back("assets/textures/Panorama_Sky_02-512x512.png");
    if (!base.empty()) {
        candidates.push_back(base + "assets/textures/Panorama_Sky_02-512x512.png");
        candidates.push_back(base + "../assets/textures/Panorama_Sky_02-512x512.png");
    }

    for (const std::string& c : candidates) {
        if (fileExists(c)) {
            return c;
        }
    }

    return requested;
}

// ---------------------------------------------------------------------------
// Renderer
// ---------------------------------------------------------------------------

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(SDL_Window* window) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    glContext_ = SDL_GL_CreateContext(window);
    if (!glContext_) { std::cerr << "GL context failed\n"; return false; }
    if (!SDL_GL_MakeCurrent(window, glContext_)) return false;
    if (gladLoadGL() == 0) { std::cerr << "GLAD failed\n"; return false; }

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, viewportWidth_, viewportHeight_);

    if (!uiRenderer_.init(viewportWidth_, viewportHeight_)) {
        std::cerr << "[Renderer] Failed to initialize UI renderer.\n";
        return false;
    }

    // Set shader library to resolve relative to binary (same as assets)
    const char* base = SDL_GetBasePath();
    shaderLibrary_.setShaderRoot(base ? std::string(base) + "shaders/" : "shaders/");

    if (!createBuiltinShader()) {
        return false;
    }

    // Optional sky resources. If this fails, the renderer still works and uses
    // the clear color / gradient fallback.
    createPanoramaSkyResources();

    // Optional post-processing resources for bloom. If this fails, the renderer
    // still draws normally to the default framebuffer.
    createBloomResources();

    return true;
}

bool Renderer::createBuiltinShader() {
    // Written to a temp location and loaded, OR compiled inline.
    // We compile inline here so there's always a valid fallback.
    const char* vertSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 uMVP;
        void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
    )";
    const char* fragSrc = R"(
        #version 330 core
        uniform vec3 uColor;
        out vec4 FragColor;
        void main() { FragColor = vec4(uColor, 1.0); }
    )";

    auto compile = [](const char* src, unsigned int type, const char* label) -> unsigned int {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
            std::cerr << "[Builtin] " << label << ": " << log << "\n";
            return 0;
        }
        return s;
    };

    unsigned int vs = compile(vertSrc, GL_VERTEX_SHADER,   "vert");
    unsigned int fs = compile(fragSrc, GL_FRAGMENT_SHADER, "frag");
    if (!vs || !fs) return false;

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    // Hand the raw ID to builtinShader_ via a small friend trick —
    // easier: just expose an internal setter or keep as raw uint.
    // For simplicity we store the builtin as a raw uint here:
    builtinProgramID_ = prog; // add this field to the private section
    return true;
}

bool Renderer::uploadMesh(RenderComponent& rc, const MeshAsset& mesh) {
    if (rc.vao) glDeleteVertexArrays(1, &rc.vao);
    if (rc.vbo) glDeleteBuffers(1, &rc.vbo);
    if (rc.ebo) glDeleteBuffers(1, &rc.ebo);
    rc.vao = rc.vbo = rc.ebo = 0;

    rc.useIndices  = !mesh.indices.empty();
    rc.indexCount  = (unsigned int)mesh.indices.size();
    rc.vertexCount = (unsigned int)(mesh.vertices.size());

    glGenVertexArrays(1, &rc.vao);
    glGenBuffers(1, &rc.vbo);
    glBindVertexArray(rc.vao);

    glBindBuffer(GL_ARRAY_BUFFER, rc.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 mesh.vertices.size() * sizeof(Vertex),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    if (rc.useIndices) {
        glGenBuffers(1, &rc.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rc.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     mesh.indices.size() * sizeof(unsigned int),
                     mesh.indices.data(), GL_STATIC_DRAW);
    }

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    std::cout << "[Renderer] Uploaded mesh: "
    << rc.vertexCount << " verts, " << rc.indexCount << " indices\n";
    return true;
}


static unsigned int createRealmgateOverlaySkinnedProgram() {
    const char* vertSrc = R"(
        #version 330 core
        #extension GL_ARB_shading_language_420pack : enable

        layout(location = 0) in vec3  aPosition;
        layout(location = 1) in vec3  aNormal;
        layout(location = 2) in vec2  aUV;
        layout(location = 4) in uvec4 aJoints;
        layout(location = 5) in vec4  aWeights;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;
        uniform int  uSkinned;

        layout(std140, binding = 0) uniform JointBlock {
            mat4 uJointMatrices[256];
        };

        out vec2 vUV;
        out vec3 vNormal;
        out vec3 vWorldPos;

        void main() {
            vec4 pos = vec4(aPosition, 1.0);
            vec3 n = aNormal;

            if (uSkinned != 0) {
                mat4 skin =
                aWeights.x * uJointMatrices[aJoints.x] +
                aWeights.y * uJointMatrices[aJoints.y] +
                aWeights.z * uJointMatrices[aJoints.z] +
                aWeights.w * uJointMatrices[aJoints.w];

                pos = skin * pos;
                n = mat3(skin) * n;
            }

            vec4 world = uModel * pos;
            vWorldPos = world.xyz;
            vNormal = normalize(mat3(uModel) * n);
            vUV = aUV;

            gl_Position = uProjection * uView * world;
        }
        )";

    const char* fragSrc = R"(
        #version 330 core

        in vec2 vUV;
        in vec3 vNormal;
        in vec3 vWorldPos;

        uniform sampler2D uBaseColorTexture;
        uniform int uHasBaseColorTexture;
        uniform vec3 uColor;
        uniform float uAlpha;

        out vec4 FragColor;

        void main() {
            vec4 base = vec4(uColor, max(uAlpha, 1.0));

            if (uHasBaseColorTexture != 0) {
                base *= texture(uBaseColorTexture, vUV);
            }

            if (base.a < 0.1) {
                discard;
            }

            vec3 n = normalize(vNormal);
            vec3 lightDir = normalize(vec3(-0.35, 0.85, 0.25));
            float ndl = max(dot(n, lightDir), 0.0);
            float shade = 0.55 + ndl * 0.45;

            FragColor = vec4(base.rgb * shade, base.a);
        }
        )";

    return linkProgramSources(vertSrc, fragSrc, "realmgate overlay skinned");
}

void Renderer::renderScene(Scene& scene) {
    glViewport(0, 0, viewportWidth_, viewportHeight_);

    float proj[16], view[16], pv[16];
    const float nearPlane = 0.1f;
    const float farPlane = 500.0f;

    makePerspective(
        60.0f,
        (float)viewportWidth_ / (float)viewportHeight_,
                    nearPlane,
                    farPlane,
                    proj
    );
    makeLookAt(
        cameraX_, cameraY_, cameraZ_,
        cameraTargetX_, cameraTargetY_, cameraTargetZ_,
        0.0f, 1.0f, 0.0f,
        view
    );
    mat4Mul(proj, view, pv);

    buildLightSpaceMatrix();

    if (renderStyle_.lighting.realtimeShadows) {
        renderShadowPass(scene);
    }

    glViewport(0, 0, viewportWidth_, viewportHeight_);

    const bool useBloomTarget =
    bloomEnabled_ &&
    sceneFBO_ != 0 &&
    sceneColorTexture_ != 0 &&
    sceneBrightTexture_ != 0 &&
    sceneDepthRBO_ != 0 &&
    bloomPingFBO_[0] != 0 &&
    bloomPingFBO_[1] != 0 &&
    bloomPingTexture_[0] != 0 &&
    bloomPingTexture_[1] != 0 &&
    bloomBlurProgramID_ != 0 &&
    bloomCompositeProgramID_ != 0 &&
    screenQuadVAO_ != 0;
    if (useBloomTarget) {
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO_);
        GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, drawBuffers);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
    }

    Vec3f clearColor = renderStyle_.sky.color;

    if (!renderStyle_.skyEnabled || !renderStyle_.sky.enabled) {
        const auto& skyClear = renderStyle_.lighting.ambientSky;
        clearColor = {
            skyClear.x * 0.55f,
            skyClear.y * 0.55f,
            skyClear.z * 0.75f
        };
    }

    const float clearScene[4] = {
        clearColor.x,
        clearColor.y,
        clearColor.z,
        1.0f
    };

    if (useBloomTarget) {
        const float clearBright[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, clearScene);
        glClearBufferfv(GL_COLOR, 1, clearBright);
        glClear(GL_DEPTH_BUFFER_BIT);
    } else {
        glClearColor(clearScene[0], clearScene[1], clearScene[2], clearScene[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (renderStyle_.skyEnabled &&
        renderStyle_.sky.enabled &&
        renderStyle_.sky.mode == SkySettings::Mode::Texture) {
        renderPanoramaSky(view, proj);
        }

        // Choose shader for this frame
        unsigned int prog = 0;
    if (activeShader_ && activeShader_->isValid()) {
        activeShader_->bind();
        prog = activeShader_->id();
    } else {
        glUseProgram(builtinProgramID_);
        prog = builtinProgramID_;
    }

    applyRenderStyleUniforms(prog);
    resetTextureMaterialUniforms(prog);

    // Keep sampler bindings deterministic every frame. Shadow map lives on
    // texture unit 5; all GLB albedo/base-color textures live on texture unit 0.
    // Without resetting this after shadow setup, baked texture materials can
    // accidentally read the wrong texture state and look like flat white glow.
    if (int locBaseTexFrame = glGetUniformLocation(prog, "uBaseColorTexture"); locBaseTexFrame >= 0) {
        glUniform1i(locBaseTexFrame, 0);
    }
    if (int locShadowMapFrame = glGetUniformLocation(prog, "uShadowMap"); locShadowMapFrame >= 0) {
        glUniform1i(locShadowMapFrame, 5);
    }
    glActiveTexture(GL_TEXTURE0);

    // Bloom is a strict material/emission mask. Reset all bloom-related
    // uniforms before drawing generic/static objects so stale values from
    // the previous frame's BLOOM_Blade primitive cannot leak into the sky,
    // static scene, or debug geometry.
    resetMaterialBloomUniforms(prog);

    int locMVP     = glGetUniformLocation(prog, "uMVP");
    int locColor   = glGetUniformLocation(prog, "uColor");
    int locModel   = glGetUniformLocation(prog, "uModel");
    int locView    = glGetUniformLocation(prog, "uView");
    int locProj    = glGetUniformLocation(prog, "uProjection");
    int locSkinned = glGetUniformLocation(prog, "uSkinned");
    int locUseGeometryNormals = glGetUniformLocation(prog, "uUseGeometryNormals");

    if (locView >= 0) glUniformMatrix4fv(locView, 1, GL_FALSE, view);
    if (locProj >= 0) glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);

    // ── Static (non-skinned) entities ───────────────────────────────────────
    for (auto& entityPtr : scene.entities()) {
        Entity* e = entityPtr.get();
        if (!e->active) continue;
        auto* rc = e->getComponent<RenderComponent>();
        if (!rc || !rc->vao) continue;

        float model[16], mvp[16];
        buildModelMatrix(e->transform(), model);
        mat4Mul(pv, model, mvp);

        if (locMVP >= 0)     glUniformMatrix4fv(locMVP,   1, GL_FALSE, mvp);
        if (locModel >= 0)   glUniformMatrix4fv(locModel, 1, GL_FALSE, model);
        if (locSkinned >= 0) glUniform1i(locSkinned, 0);
        if (locUseGeometryNormals >= 0) glUniform1i(locUseGeometryNormals, 1);
        if (locColor >= 0)   glUniform3f(locColor, rc->color.x, rc->color.y, rc->color.z);

        // Generic RenderComponent objects are never bloom sources and never
        // inherit a GLB texture from the previous primitive.
        resetMaterialBloomUniforms(prog);
        resetTextureMaterialUniforms(prog);

        glBindVertexArray(rc->vao);
        if (rc->useIndices)
            glDrawElements(GL_TRIANGLES, rc->indexCount, GL_UNSIGNED_INT, 0);
        else
            glDrawArrays(GL_TRIANGLES, 0, rc->vertexCount);
    }
    glBindVertexArray(0);

    // ── Full static-scene GLB entities ────────────────────────────────────────
    // These are multi-node/multi-primitive GLB scenes loaded by DemoEnvironment.
    for (auto& entityPtr : scene.entities()) {
        Entity* e = entityPtr.get();
        if (!e->active) continue;
        if (e->getComponent<StaticSceneRenderComponent>())
            renderStaticSceneEntity(*e, prog, pv);
    }

    // ── Skinned entities ─────────────────────────────────────────────────────
    // Upload VP matrices to the skinned shader uniforms if it's active.
    {
        if (locView >= 0) glUniformMatrix4fv(locView, 1, GL_FALSE, view);
        if (locProj >= 0) glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);

        for (auto& entityPtr : scene.entities()) {
            Entity* e = entityPtr.get();
            if (!e->active) continue;
            if (e->getComponent<SkinnedRenderComponent>()) {
                if (locUseGeometryNormals >= 0) glUniform1i(locUseGeometryNormals, 0);
                renderSkinnedEntity(*e, prog);
            }
        }
    }

    // After skinned rendering, reset bloom uniforms again so debug lines or
    // later draws cannot inherit BLOOM_Blade / sword emission state.
    if (prog) {
        glUseProgram(prog);
        resetMaterialBloomUniforms(prog);
    }

    // Draw debug wireframes last so collider lines are easy to inspect.
    drawDebugLines(pv);

    if (useBloomTarget) {
        renderBloomComposite();
    }
}


void Renderer::renderSceneOverlay(Scene& scene) {
    (void)scene;
    // Disabled by design.
    // Skinned entities must be rendered only inside renderScene(), where they share
    // the same RenderStyle uniforms, shadows, fog, and bloom attachments as the
    // environment. The old overlay pass drew the player again directly to the
    // backbuffer after post-processing, which made the player ignore the world
    // shader and prevented sword bloom from reading correctly.
}


void Renderer::setCamera(
    float camX, float camY, float camZ,
    float targetX, float targetY, float targetZ
) {
    cameraX_ = camX;
    cameraY_ = camY;
    cameraZ_ = camZ;

    cameraTargetX_ = targetX;
    cameraTargetY_ = targetY;
    cameraTargetZ_ = targetZ;
}

void Renderer::setShadowAnchor(float x, float y, float z) {
    shadowAnchorX_ = x;
    shadowAnchorY_ = y;
    shadowAnchorZ_ = z;
    shadowAnchorSet_ = true;
}

void Renderer::buildLightSpaceMatrix() {
    const auto& l = renderStyle_.lighting;

    // sunDirection = direction light rays travel through the world.
    // Example: negative Y means sunlight travels downward.
    float dx = l.sunDirection.x;
    float dy = l.sunDirection.y;
    float dz = l.sunDirection.z;

    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 0.001f) len = 1.0f;

    dx /= len;
    dy /= len;
    dz /= len;

    float targetX = shadowAnchorSet_ ? shadowAnchorX_ : cameraTargetX_;
    float targetY = shadowAnchorSet_ ? shadowAnchorY_ : cameraTargetY_;
    float targetZ = shadowAnchorSet_ ? shadowAnchorZ_ : cameraTargetZ_;

    float range = (l.shadowOrthoRange > 1.0f) ? l.shadowOrthoRange : 80.0f;

    // Stabilize the directional shadow projection to whole shadow-map texels.
    // This prevents the shadow pattern from sliding/crawling across large flat
    // walls as the player/camera moves by tiny amounts.
    float upX = 0.0f;
    float upY = 1.0f;
    float upZ = 0.0f;

    // If the light is almost vertical, world-up becomes a weak basis. Use Z-up
    // only for the basis construction in that rare case.
    if (fabsf(dy) > 0.96f) {
        upX = 0.0f;
        upY = 0.0f;
        upZ = 1.0f;
    }

    // Same basis math as makeLookAt(): s = right, u = light-view up.
    float sx = dy * upZ - dz * upY;
    float sy = dz * upX - dx * upZ;
    float sz = dx * upY - dy * upX;
    float sl = sqrtf(sx * sx + sy * sy + sz * sz);
    if (sl < 0.0001f) sl = 1.0f;
    sx /= sl;
    sy /= sl;
    sz /= sl;

    float ux = sy * dz - sz * dy;
    float uy = sz * dx - sx * dz;
    float uz = sx * dy - sy * dx;

    const float texelWorldSize = (range * 2.0f) / static_cast<float>(shadowMapSize_);
    if (texelWorldSize > 0.000001f) {
        float lightX = targetX * sx + targetY * sy + targetZ * sz;
        float lightY = targetX * ux + targetY * uy + targetZ * uz;

        float snappedX = roundf(lightX / texelWorldSize) * texelWorldSize;
        float snappedY = roundf(lightY / texelWorldSize) * texelWorldSize;

        float deltaX = snappedX - lightX;
        float deltaY = snappedY - lightY;

        targetX += sx * deltaX + ux * deltaY;
        targetY += sy * deltaX + uy * deltaY;
        targetZ += sz * deltaX + uz * deltaY;
    }

    float dist = 180.0f;

    // The shadow camera must sit opposite the direction the light travels from.
    float eyeX = targetX - dx * dist;
    float eyeY = targetY - dy * dist;
    float eyeZ = targetZ - dz * dist;

    float lightView[16];
    makeLookAt(
        eyeX, eyeY, eyeZ,
        targetX, targetY, targetZ,
        upX, upY, upZ,
        lightView
    );

    float lightProj[16];
    makeOrtho(
        -range, range,
        -range, range,
        1.0f, 500.0f,
        lightProj
    );

    mat4Mul(lightProj, lightView, lightSpaceMatrix_);
}
void Renderer::clearDebugLines() {
    debugLines_.clear();
}

void Renderer::addDebugLine(
    float ax, float ay, float az,
    float bx, float by, float bz,
    float r, float g, float b
) {
    debugLines_.push_back(DebugLineVertex{ax, ay, az, r, g, b});
    debugLines_.push_back(DebugLineVertex{bx, by, bz, r, g, b});
}

void Renderer::addLightDebugLines() {
    const auto& l = renderStyle_.lighting;

    float dx = l.sunDirection.x;
    float dy = l.sunDirection.y;
    float dz = l.sunDirection.z;

    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 0.0001f) {
        return;
    }

    dx /= len;
    dy /= len;
    dz /= len;

    float targetX = cameraTargetX_;
    float targetY = cameraTargetY_ + 2.5f;
    float targetZ = cameraTargetZ_;

    // Directional light has no true position.
    // This point is only a visual marker showing where light appears to come from.
    float sourceX = targetX - dx * 10.0f;
    float sourceY = targetY - dy * 10.0f;
    float sourceZ = targetZ - dz * 10.0f;

    // One line: source marker toward player/camera target.
    addDebugLine(
        sourceX, sourceY, sourceZ,
        targetX, targetY, targetZ,
        1.0f, 0.9f, 0.1f
    );

    float s = 0.7f;

    addDebugLine(sourceX - s, sourceY, sourceZ, sourceX + s, sourceY, sourceZ, 1.0f, 0.9f, 0.1f);
    addDebugLine(sourceX, sourceY - s, sourceZ, sourceX, sourceY + s, sourceZ, 1.0f, 0.9f, 0.1f);
    addDebugLine(sourceX, sourceY, sourceZ - s, sourceX, sourceY, sourceZ + s, 1.0f, 0.9f, 0.1f);
}

void Renderer::drawDebugLines(const float* pv) {
    if (debugLines_.empty()) return;

    if (debugLineVAO_ == 0) {
        glGenVertexArrays(1, &debugLineVAO_);
        glGenBuffers(1, &debugLineVBO_);

        glBindVertexArray(debugLineVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, debugLineVBO_);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(DebugLineVertex),
                              reinterpret_cast<void*>(0)
        );

        // Attribute 1 is optional. The built-in shader ignores it, but a
        // future debug shader may use it for per-line colors.
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(DebugLineVertex),
                              reinterpret_cast<void*>(sizeof(float) * 3)
        );

        glBindVertexArray(0);
    }

    glBindVertexArray(debugLineVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, debugLineVBO_);
    glBufferData(
        GL_ARRAY_BUFFER,
        debugLines_.size() * sizeof(DebugLineVertex),
                 debugLines_.data(),
                 GL_DYNAMIC_DRAW
    );

    // Use the built-in shader because it supports uMVP + uColor.
    glUseProgram(builtinProgramID_);

    int locMVP = glGetUniformLocation(builtinProgramID_, "uMVP");
    if (locMVP >= 0) {
        glUniformMatrix4fv(locMVP, 1, GL_FALSE, pv);
    }

    int locColor = glGetUniformLocation(builtinProgramID_, "uColor");
    if (locColor >= 0) {
        // Bright green collider wireframe.
        glUniform3f(locColor, 0.0f, 1.0f, 0.1f);
    }

    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.0f);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(debugLines_.size()));

    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
}

void Renderer::beginUI() {
    uiRenderer_.begin(viewportWidth_, viewportHeight_);
}

void Renderer::drawUIRect(float x, float y, float w, float h,
                          float r, float g, float b, float a) {
    uiRenderer_.drawRect(x, y, w, h, r, g, b, a);
                          }

                          void Renderer::endUI() {
                              glEnable(GL_DEPTH_TEST);
                              glDisable(GL_BLEND);
                          }

                          void Renderer::drawUIImage(Texture2D& tex,
                                                     float x, float y, float w, float h,
                                                     float r, float g, float b, float a) {
                              uiRenderer_.drawImage(tex, x, y, w, h, r, g, b, a);
                                                     }

                                                     void Renderer::drawUIImageRegion(Texture2D& tex,
                                                                                      float x, float y, float w, float h,
                                                                                      float u0, float v0, float u1, float v1,
                                                                                      float r, float g, float b, float a) {
                                                         uiRenderer_.drawImageRegion(tex, x, y, w, h, u0, v0, u1, v1, r, g, b, a);
                                                                                      }

                                                                                      void Renderer::drawUIText(const std::string& text,
                                                                                                                float x, float y,
                                                                                                                float scale,
                                                                                                                float r, float g, float b, float a) {
                                                                                          uiRenderer_.drawText(text, x, y, scale, r, g, b, a);
                                                                                                                }

                                                                                                                void Renderer::onResize(int w, int h) {
                                                                                                                    viewportWidth_  = w;
                                                                                                                    viewportHeight_ = h;
                                                                                                                    glViewport(0, 0, w, h);

                                                                                                                    // Rebuild framebuffer-sized post-process resources.
                                                                                                                    destroyBloomResources();
                                                                                                                    createBloomResources();
                                                                                                                }

                                                                                                                void Renderer::shutdown() {
                                                                                                                    destroyBloomResources();

                                                                                                                    if (debugLineVBO_) {
                                                                                                                        glDeleteBuffers(1, &debugLineVBO_);
                                                                                                                        debugLineVBO_ = 0;
                                                                                                                    }

                                                                                                                    if (debugLineVAO_) {
                                                                                                                        glDeleteVertexArrays(1, &debugLineVAO_);
                                                                                                                        debugLineVAO_ = 0;
                                                                                                                    }

                                                                                                                    if (skyTextureID_) {
                                                                                                                        glDeleteTextures(1, &skyTextureID_);
                                                                                                                        skyTextureID_ = 0;
                                                                                                                    }

                                                                                                                    if (skyVBO_) {
                                                                                                                        glDeleteBuffers(1, &skyVBO_);
                                                                                                                        skyVBO_ = 0;
                                                                                                                    }

                                                                                                                    if (skyVAO_) {
                                                                                                                        glDeleteVertexArrays(1, &skyVAO_);
                                                                                                                        skyVAO_ = 0;
                                                                                                                    }

                                                                                                                    if (skyProgramID_) {
                                                                                                                        glDeleteProgram(skyProgramID_);
                                                                                                                        skyProgramID_ = 0;
                                                                                                                    }

                                                                                                                    if (shadowDepthTexture_) {
                                                                                                                        glDeleteTextures(1, &shadowDepthTexture_);
                                                                                                                        shadowDepthTexture_ = 0;
                                                                                                                    }

                                                                                                                    if (shadowFBO_) {
                                                                                                                        glDeleteFramebuffers(1, &shadowFBO_);
                                                                                                                        shadowFBO_ = 0;
                                                                                                                    }

                                                                                                                    if (shadowProgramID_) {
                                                                                                                        glDeleteProgram(shadowProgramID_);
                                                                                                                        shadowProgramID_ = 0;
                                                                                                                    }

                                                                                                                    if (builtinProgramID_) glDeleteProgram(builtinProgramID_);
                                                                                                                    builtinProgramID_ = 0;
                                                                                                                    uiRenderer_.shutdown();
                                                                                                                    if (glContext_) { SDL_GL_DestroyContext(glContext_); glContext_ = nullptr; }
                                                                                                                }


                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                // Bloom post-process
                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                bool Renderer::createBloomResources() {
                                                                                                                    if (viewportWidth_ <= 0 || viewportHeight_ <= 0) {
                                                                                                                        return false;
                                                                                                                    }

                                                                                                                    if (sceneFBO_ && sceneColorTexture_ && sceneBrightTexture_ && sceneDepthRBO_ &&
                                                                                                                        bloomPingFBO_[0] && bloomPingFBO_[1] &&
                                                                                                                        bloomPingTexture_[0] && bloomPingTexture_[1] &&
                                                                                                                        bloomBlurProgramID_ && bloomCompositeProgramID_ && screenQuadVAO_) {
                                                                                                                        return true;
                                                                                                                        }

                                                                                                                        destroyBloomResources();

                                                                                                                    // Fullscreen quad resources.
                                                                                                                    float quadVerts[] = {
                                                                                                                        // pos      uv
                                                                                                                        -1.0f, -1.0f, 0.0f, 0.0f,
                                                                                                                        1.0f, -1.0f, 1.0f, 0.0f,
                                                                                                                        -1.0f,  1.0f, 0.0f, 1.0f,
                                                                                                                        -1.0f,  1.0f, 0.0f, 1.0f,
                                                                                                                        1.0f, -1.0f, 1.0f, 0.0f,
                                                                                                                        1.0f,  1.0f, 1.0f, 1.0f
                                                                                                                    };

                                                                                                                    glGenVertexArrays(1, &screenQuadVAO_);
                                                                                                                    glGenBuffers(1, &screenQuadVBO_);
                                                                                                                    glBindVertexArray(screenQuadVAO_);
                                                                                                                    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO_);
                                                                                                                    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
                                                                                                                    glEnableVertexAttribArray(0);
                                                                                                                    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
                                                                                                                    glEnableVertexAttribArray(1);
                                                                                                                    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
                                                                                                                    glBindVertexArray(0);

                                                                                                                    const char* quadVert = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() {
            vUV = aUV;
            gl_Position = vec4(aPos.xy, 0.0, 1.0);
        }
    )";

    const char* blurFrag = R"(
        #version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uImage;
        uniform int uHorizontal;
        uniform float uRadius;

        void main() {
            vec2 texel = 1.0 / vec2(textureSize(uImage, 0));
            vec2 dir = (uHorizontal != 0) ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
            dir *= max(uRadius, 0.25);

            vec3 color = texture(uImage, vUV).rgb * 0.227027;
            color += texture(uImage, vUV + dir * 1.384615).rgb * 0.316216;
            color += texture(uImage, vUV - dir * 1.384615).rgb * 0.316216;
            color += texture(uImage, vUV + dir * 3.230769).rgb * 0.070270;
            color += texture(uImage, vUV - dir * 3.230769).rgb * 0.070270;
            FragColor = vec4(color, 1.0);
        }
    )";

    const char* compositeFrag = R"(
        #version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uScene;
        uniform sampler2D uBloom;
        uniform float uBloomStrength;

        void main() {
            vec3 scene = texture(uScene, vUV).rgb;
            vec3 bloom = texture(uBloom, vUV).rgb;
            vec3 color = scene + bloom * uBloomStrength;
            FragColor = vec4(color, 1.0);
        }
    )";

    bloomBlurProgramID_ = linkProgramSources(quadVert, blurFrag, "bloom blur");
    bloomCompositeProgramID_ = linkProgramSources(quadVert, compositeFrag, "bloom composite");
    if (!bloomBlurProgramID_ || !bloomCompositeProgramID_) {
        destroyBloomResources();
        return false;
    }

    glGenFramebuffers(1, &sceneFBO_);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO_);

    glGenTextures(1, &sceneColorTexture_);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth_, viewportHeight_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture_, 0);

    glGenTextures(1, &sceneBrightTexture_);
    glBindTexture(GL_TEXTURE_2D, sceneBrightTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth_, viewportHeight_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, sceneBrightTexture_, 0);

    glGenRenderbuffers(1, &sceneDepthRBO_);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRBO_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewportWidth_, viewportHeight_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRBO_);

    GLenum drawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Renderer] Bloom scene framebuffer incomplete. Bloom disabled for this run.\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroyBloomResources();
        return false;
    }

    glGenFramebuffers(2, bloomPingFBO_);
    glGenTextures(2, bloomPingTexture_);

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomPingFBO_[i]);
        glBindTexture(GL_TEXTURE_2D, bloomPingTexture_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewportWidth_, viewportHeight_, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomPingTexture_[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[Renderer] Bloom blur framebuffer incomplete. Bloom disabled for this run.\n";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroyBloomResources();
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
                                                                                                                }

                                                                                                                void Renderer::destroyBloomResources() {
                                                                                                                    if (sceneDepthRBO_) {
                                                                                                                        glDeleteRenderbuffers(1, &sceneDepthRBO_);
                                                                                                                        sceneDepthRBO_ = 0;
                                                                                                                    }
                                                                                                                    if (sceneColorTexture_) {
                                                                                                                        glDeleteTextures(1, &sceneColorTexture_);
                                                                                                                        sceneColorTexture_ = 0;
                                                                                                                    }
                                                                                                                    if (sceneBrightTexture_) {
                                                                                                                        glDeleteTextures(1, &sceneBrightTexture_);
                                                                                                                        sceneBrightTexture_ = 0;
                                                                                                                    }
                                                                                                                    if (sceneFBO_) {
                                                                                                                        glDeleteFramebuffers(1, &sceneFBO_);
                                                                                                                        sceneFBO_ = 0;
                                                                                                                    }
                                                                                                                    if (bloomPingTexture_[0] || bloomPingTexture_[1]) {
                                                                                                                        glDeleteTextures(2, bloomPingTexture_);
                                                                                                                        bloomPingTexture_[0] = bloomPingTexture_[1] = 0;
                                                                                                                    }
                                                                                                                    if (bloomPingFBO_[0] || bloomPingFBO_[1]) {
                                                                                                                        glDeleteFramebuffers(2, bloomPingFBO_);
                                                                                                                        bloomPingFBO_[0] = bloomPingFBO_[1] = 0;
                                                                                                                    }
                                                                                                                    if (screenQuadVBO_) {
                                                                                                                        glDeleteBuffers(1, &screenQuadVBO_);
                                                                                                                        screenQuadVBO_ = 0;
                                                                                                                    }
                                                                                                                    if (screenQuadVAO_) {
                                                                                                                        glDeleteVertexArrays(1, &screenQuadVAO_);
                                                                                                                        screenQuadVAO_ = 0;
                                                                                                                    }
                                                                                                                    if (bloomBlurProgramID_) {
                                                                                                                        glDeleteProgram(bloomBlurProgramID_);
                                                                                                                        bloomBlurProgramID_ = 0;
                                                                                                                    }
                                                                                                                    if (bloomCompositeProgramID_) {
                                                                                                                        glDeleteProgram(bloomCompositeProgramID_);
                                                                                                                        bloomCompositeProgramID_ = 0;
                                                                                                                    }
                                                                                                                }

                                                                                                                void Renderer::renderBloomComposite() {
                                                                                                                    glDisable(GL_DEPTH_TEST);
                                                                                                                    glDisable(GL_BLEND);

                                                                                                                    const int blurPasses = 8;
                                                                                                                    bool horizontal = true;
                                                                                                                    bool firstPass = true;

                                                                                                                    glUseProgram(bloomBlurProgramID_);
                                                                                                                    setInt(bloomBlurProgramID_, "uImage", 0);
                                                                                                                    setFloat(bloomBlurProgramID_, "uRadius", bloomRadius_);

                                                                                                                    for (int i = 0; i < blurPasses; ++i) {
                                                                                                                        glBindFramebuffer(GL_FRAMEBUFFER, bloomPingFBO_[horizontal ? 1 : 0]);
                                                                                                                        glDrawBuffer(GL_COLOR_ATTACHMENT0);
                                                                                                                        glClear(GL_COLOR_BUFFER_BIT);

                                                                                                                        setInt(bloomBlurProgramID_, "uHorizontal", horizontal ? 1 : 0);
                                                                                                                        glActiveTexture(GL_TEXTURE0);
                                                                                                                        glBindTexture(GL_TEXTURE_2D, firstPass ? sceneBrightTexture_ : bloomPingTexture_[horizontal ? 0 : 1]);

                                                                                                                        glBindVertexArray(screenQuadVAO_);
                                                                                                                        glDrawArrays(GL_TRIANGLES, 0, 6);

                                                                                                                        horizontal = !horizontal;
                                                                                                                        if (firstPass) firstPass = false;
                                                                                                                    }

                                                                                                                    unsigned int blurredTexture = bloomPingTexture_[horizontal ? 0 : 1];

                                                                                                                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                                                                                                                    glDrawBuffer(GL_BACK);
                                                                                                                    glViewport(0, 0, viewportWidth_, viewportHeight_);
                                                                                                                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                                                                                                                    glUseProgram(bloomCompositeProgramID_);
                                                                                                                    setInt(bloomCompositeProgramID_, "uScene", 0);
                                                                                                                    setInt(bloomCompositeProgramID_, "uBloom", 1);
                                                                                                                    setFloat(bloomCompositeProgramID_, "uBloomStrength", bloomStrength_);

                                                                                                                    glActiveTexture(GL_TEXTURE0);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, sceneColorTexture_);
                                                                                                                    glActiveTexture(GL_TEXTURE1);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, blurredTexture);

                                                                                                                    glBindVertexArray(screenQuadVAO_);
                                                                                                                    glDrawArrays(GL_TRIANGLES, 0, 6);
                                                                                                                    glBindVertexArray(0);

                                                                                                                    glActiveTexture(GL_TEXTURE1);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                    glActiveTexture(GL_TEXTURE0);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                    glEnable(GL_DEPTH_TEST);
                                                                                                                }



                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                // Panorama sky
                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                bool Renderer::createPanoramaSkyResources() {
                                                                                                                    if (skyProgramID_ != 0 && skyVAO_ != 0) {
                                                                                                                        return true;
                                                                                                                    }

                                                                                                                    const char* vertSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPosition;

        out vec3 vDir;

        uniform mat4 uView;
        uniform mat4 uProjection;

        void main() {
            vDir = aPosition;

            // Remove camera translation so the sky follows the camera.
            mat4 viewNoTranslation = mat4(mat3(uView));
            vec4 clip = uProjection * viewNoTranslation * vec4(aPosition, 1.0);

            // Keep sky at the far plane.
            gl_Position = clip.xyww;
        }
    )";

    const char* fragSrc = R"(
        #version 330 core

        in vec3 vDir;
        layout(location = 0) out vec4 FragColor;
        layout(location = 1) out vec4 BrightColor;

        uniform sampler2D uPanoramaTexture;
        uniform int uUsePanorama;
        uniform float uSkyIntensity;
        uniform float uSkyRotation;
        uniform vec3 uAmbientSky;
        uniform vec3 uFogColor;
        uniform vec3 uSunDirection;
        uniform vec3 uSunColor;
        uniform float uSunIntensity;

        const float PI = 3.14159265359;

        vec2 directionToPanoramaUV(vec3 dir) {
            dir = normalize(dir);

            float theta = atan(dir.z, dir.x) + uSkyRotation;
            float phi = asin(clamp(dir.y, -1.0, 1.0));

            float u = 0.5 + theta / (2.0 * PI);
            float v = 0.5 - phi / PI;

            return vec2(fract(u), clamp(v, 0.0, 1.0));
        }

        vec3 toneMap(vec3 color) {
            color = color / (color + vec3(1.0));
            color = pow(color, vec3(1.0 / 2.2));
            return color;
        }

        void main() {
            vec3 dir = normalize(vDir);
            vec3 sky;

            if (uUsePanorama != 0) {
                vec2 uv = directionToPanoramaUV(dir);
                sky = texture(uPanoramaTexture, uv).rgb * uSkyIntensity;
            } else {
                // Procedural fallback gradient when panorama is missing.
                float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
                vec3 horizon = uFogColor;
                vec3 zenith = uAmbientSky * 1.35;
                sky = mix(horizon, zenith, smoothstep(0.0, 1.0, t)) * uSkyIntensity;
            }

            // The panorama image may contain a painted sun, but the actual engine
            // light is uSunDirection. Add a subtle procedural sun/glow driven by the
            // same variable used by world lighting and the shadow camera.
            vec3 toSun = normalize(-uSunDirection);
            float sunAlign = max(dot(dir, toSun), 0.0);
            float sunDisk = smoothstep(0.9992, 0.99985, sunAlign);
            float sunGlow = pow(sunAlign, 96.0) * 0.45 + pow(sunAlign, 18.0) * 0.10;
            sky += uSunColor * uSunIntensity * (sunDisk * 5.0 + sunGlow);

            FragColor = vec4(toneMap(sky), 1.0);
            BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
        )";

    skyProgramID_ = linkProgramSources(vertSrc, fragSrc, "panorama sky");
    if (!skyProgramID_) {
        std::cerr << "[Renderer] Panorama sky shader failed; sky disabled.\n";
        return false;
    }

    // Cube positions. The vertex shader pushes this to the far plane.
    const float cubeVerts[] = {
        -1,-1,-1,  1,-1,-1,  1, 1,-1,
        1, 1,-1, -1, 1,-1, -1,-1,-1,

        -1,-1, 1,  1,-1, 1,  1, 1, 1,
        1, 1, 1, -1, 1, 1, -1,-1, 1,

        -1, 1, 1, -1, 1,-1, -1,-1,-1,
        -1,-1,-1, -1,-1, 1, -1, 1, 1,

        1, 1, 1,  1, 1,-1,  1,-1,-1,
        1,-1,-1,  1,-1, 1,  1, 1, 1,

        -1,-1,-1,  1,-1,-1,  1,-1, 1,
        1,-1, 1, -1,-1, 1, -1,-1,-1,

        -1, 1,-1,  1, 1,-1,  1, 1, 1,
        1, 1, 1, -1, 1, 1, -1, 1,-1
    };

    glGenVertexArrays(1, &skyVAO_);
    glGenBuffers(1, &skyVBO_);

    glBindVertexArray(skyVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

    glBindVertexArray(0);

    return true;
                                                                                                                }

                                                                                                                unsigned int Renderer::loadTexture2DFromFile(const std::string& path) {
                                                                                                                    const std::string resolved = resolveRuntimePath(path);

                                                                                                                    int width = 0;
                                                                                                                    int height = 0;
                                                                                                                    int comp = 0;
                                                                                                                    stbi_uc* pixels = stbi_load(resolved.c_str(), &width, &height, &comp, 4);

                                                                                                                    if (!pixels) {
                                                                                                                        std::cerr << "[Renderer] Could not load sky panorama texture: "
                                                                                                                        << path << " (resolved: " << resolved << ")\n";
                                                                                                                        return 0;
                                                                                                                    }

                                                                                                                    unsigned int tex = 0;
                                                                                                                    glGenTextures(1, &tex);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, tex);
                                                                                                                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                                                                                                                    // Panorama files are regular color images too, so decode them from sRGB
                                                                                                                    // into linear when sampled. The sky shader then does one final gamma encode.
                                                                                                                    glTexImage2D(
                                                                                                                        GL_TEXTURE_2D,
                                                                                                                        0,
                                                                                                                        GL_SRGB8_ALPHA8,
                                                                                                                        width,
                                                                                                                        height,
                                                                                                                        0,
                                                                                                                        GL_RGBA,
                                                                                                                        GL_UNSIGNED_BYTE,
                                                                                                                        pixels
                                                                                                                    );

                                                                                                                    glGenerateMipmap(GL_TEXTURE_2D);
                                                                                                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                                                                                                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                                                                                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                                                                                                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                                                                                                                    glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                    stbi_image_free(pixels);

                                                                                                                    std::cout << "[Renderer] Loaded sky panorama: " << resolved
                                                                                                                    << " (" << width << "x" << height << ")\n";

                                                                                                                    return tex;
                                                                                                                }

                                                                                                                void Renderer::renderPanoramaSky(const float* view, const float* proj) {
                                                                                                                    if (!createPanoramaSkyResources()) {
                                                                                                                        return;
                                                                                                                    }

                                                                                                                    const std::string desiredPath = renderStyle_.sky.panoramaTexturePath;

                                                                                                                    if (skyTextureID_ == 0 || loadedSkyTexturePath_ != desiredPath) {
                                                                                                                        if (skyTextureID_) {
                                                                                                                            glDeleteTextures(1, &skyTextureID_);
                                                                                                                            skyTextureID_ = 0;
                                                                                                                        }

                                                                                                                        loadedSkyTexturePath_ = desiredPath;
                                                                                                                        skyTextureID_ = loadTexture2DFromFile(desiredPath);
                                                                                                                    }

                                                                                                                    glDepthMask(GL_FALSE);
                                                                                                                    glDepthFunc(GL_LEQUAL);
                                                                                                                    glDisable(GL_BLEND);

                                                                                                                    glUseProgram(skyProgramID_);

                                                                                                                    int locView = glGetUniformLocation(skyProgramID_, "uView");
                                                                                                                    int locProj = glGetUniformLocation(skyProgramID_, "uProjection");

                                                                                                                    if (locView >= 0) {
                                                                                                                        glUniformMatrix4fv(locView, 1, GL_FALSE, view);
                                                                                                                    }

                                                                                                                    if (locProj >= 0) {
                                                                                                                        glUniformMatrix4fv(locProj, 1, GL_FALSE, proj);
                                                                                                                    }

                                                                                                                    const auto& l = renderStyle_.lighting;

                                                                                                                    setInt(skyProgramID_, "uPanoramaTexture", 0);
                                                                                                                    setInt(skyProgramID_, "uUsePanorama", skyTextureID_ ? 1 : 0);
                                                                                                                    setInt(skyProgramID_, "uHasPanoramaTexture", skyTextureID_ ? 1 : 0);

                                                                                                                    setFloat(skyProgramID_, "uSkyIntensity", renderStyle_.sky.intensity);
                                                                                                                    setFloat(skyProgramID_, "uSkyRotation", renderStyle_.sky.rotation);

                                                                                                                    // New toned sky controls.
                                                                                                                    setFloat(skyProgramID_, "uSkySaturation", renderStyle_.sky.saturation);
                                                                                                                    setFloat(skyProgramID_, "uSkyContrast", renderStyle_.sky.contrast);

                                                                                                                    // Shared color grading.
                                                                                                                    setFloat(skyProgramID_, "uExposure", renderStyle_.colorGrade.exposure);
                                                                                                                    setFloat(skyProgramID_, "uGamma", renderStyle_.colorGrade.gamma);

                                                                                                                    // Fallback/procedural sky colors.
                                                                                                                    setVec3(
                                                                                                                        skyProgramID_,
                                                                                                                        "uAmbientSky",
                                                                                                                        l.ambientSky.x,
                                                                                                                        l.ambientSky.y,
                                                                                                                        l.ambientSky.z
                                                                                                                    );

                                                                                                                    setVec3(
                                                                                                                        skyProgramID_,
                                                                                                                        "uFogColor",
                                                                                                                        l.fogColor.x,
                                                                                                                        l.fogColor.y,
                                                                                                                        l.fogColor.z
                                                                                                                    );

                                                                                                                    setVec3(
                                                                                                                        skyProgramID_,
                                                                                                                        "uSunDirection",
                                                                                                                        l.sunDirection.x,
                                                                                                                        l.sunDirection.y,
                                                                                                                        l.sunDirection.z
                                                                                                                    );

                                                                                                                    setVec3(
                                                                                                                        skyProgramID_,
                                                                                                                        "uSunColor",
                                                                                                                        l.sunColor.x,
                                                                                                                        l.sunColor.y,
                                                                                                                        l.sunColor.z
                                                                                                                    );

                                                                                                                    setFloat(skyProgramID_, "uSunIntensity", l.sunIntensity);

                                                                                                                    glActiveTexture(GL_TEXTURE0);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, skyTextureID_);

                                                                                                                    glBindVertexArray(skyVAO_);
                                                                                                                    glDrawArrays(GL_TRIANGLES, 0, 36);
                                                                                                                    glBindVertexArray(0);

                                                                                                                    glBindTexture(GL_TEXTURE_2D, 0);

                                                                                                                    glDepthFunc(GL_LESS);
                                                                                                                    glDepthMask(GL_TRUE);
                                                                                                                }

                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                // uploadStaticScene — upload every visible scene primitive to the GPU
                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                void Renderer::uploadStaticScene(StaticSceneRenderComponent& src) {
                                                                                                                    for (auto& g : src.gpuPrimitives) {
                                                                                                                        if (g.vao) glDeleteVertexArrays(1, &g.vao);
                                                                                                                        if (g.vbo) glDeleteBuffers(1, &g.vbo);
                                                                                                                        if (g.ebo) glDeleteBuffers(1, &g.ebo);
                                                                                                                    }

                                                                                                                    for (auto& m : src.gpuMaterials) {
                                                                                                                        if (m.baseColorTexture) {
                                                                                                                            glDeleteTextures(1, &m.baseColorTexture);
                                                                                                                            m.baseColorTexture = 0;
                                                                                                                        }
                                                                                                                        m.hasBaseColorTexture = false;
                                                                                                                    }

                                                                                                                    src.gpuPrimitives.clear();
                                                                                                                    src.gpuMaterials.clear();
                                                                                                                    if (!src.asset) return;

                                                                                                                    // Upload static scene material textures. This mirrors the skinned GLB path,
                                                                                                                    // but for Scene1.glb / level geometry.
                                                                                                                    src.gpuMaterials.resize(src.asset->materials.size());
                                                                                                                    for (int mi = 0; mi < static_cast<int>(src.asset->materials.size()); ++mi) {
                                                                                                                        const StaticSceneMaterial& mat = src.asset->materials[mi];
                                                                                                                        StaticSceneGpuMaterial& gpuMat = src.gpuMaterials[mi];

                                                                                                                        int imgIndex = mat.baseColorTextureIndex;
                                                                                                                        if (imgIndex >= 0 && imgIndex < static_cast<int>(src.asset->images.size())) {
                                                                                                                            unsigned int tex = uploadStaticSceneImageTexture(src.asset->images[imgIndex]);
                                                                                                                            gpuMat.baseColorTexture = tex;
                                                                                                                            gpuMat.hasBaseColorTexture = (tex != 0);

                                                                                                                            const auto& img = src.asset->images[imgIndex];
                                                                                                                            std::cout << "[Renderer] Uploaded static GLB texture image " << imgIndex
                                                                                                                            << " (" << img.width << "x" << img.height
                                                                                                                            << ", comp=" << img.component << ") for material "
                                                                                                                            << mi << " \"" << mat.name << "\"\n";
                                                                                                                        }
                                                                                                                    }

                                                                                                                    src.gpuPrimitives.reserve(src.asset->primitives.size());

                                                                                                                    for (const auto& prim : src.asset->primitives) {
                                                                                                                        if (prim.vertices.empty()) continue;

                                                                                                                        StaticSceneGpuPrimitive gpu{};
                                                                                                                        gpu.indexCount = (unsigned int)prim.indices.size();
                                                                                                                        gpu.vertexCount = (unsigned int)prim.vertices.size();
                                                                                                                        gpu.materialIndex = prim.materialIndex;
                                                                                                                        gpu.primitiveIndex = prim.primitiveIndex;
                                                                                                                        gpu.nodeIndex = prim.nodeIndex;
                                                                                                                        gpu.nodeName = prim.nodeName;
                                                                                                                        gpu.meshIndex = prim.meshIndex;
                                                                                                                        gpu.meshName = prim.meshName;
                                                                                                                        gpu.materialName = prim.materialName;
                                                                                                                        memcpy(gpu.nodeTransform, prim.nodeTransform, 64);

                                                                                                                        glGenVertexArrays(1, &gpu.vao);
                                                                                                                        glGenBuffers(1, &gpu.vbo);

                                                                                                                        glBindVertexArray(gpu.vao);
                                                                                                                        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
                                                                                                                        glBufferData(GL_ARRAY_BUFFER,
                                                                                                                                     prim.vertices.size() * sizeof(StaticSceneVertex),
                                                                                                                                     prim.vertices.data(),
                                                                                                                                     GL_STATIC_DRAW);

                                                                                                                        glEnableVertexAttribArray(0);
                                                                                                                        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(StaticSceneVertex),
                                                                                                                                              (void*)offsetof(StaticSceneVertex, position));

                                                                                                                        glEnableVertexAttribArray(1);
                                                                                                                        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(StaticSceneVertex),
                                                                                                                                              (void*)offsetof(StaticSceneVertex, normal));

                                                                                                                        glEnableVertexAttribArray(2);
                                                                                                                        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(StaticSceneVertex),
                                                                                                                                              (void*)offsetof(StaticSceneVertex, uv));

                                                                                                                        if (!prim.indices.empty()) {
                                                                                                                            glGenBuffers(1, &gpu.ebo);
                                                                                                                            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
                                                                                                                            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                                                                                                                         prim.indices.size() * sizeof(uint32_t),
                                                                                                                                         prim.indices.data(),
                                                                                                                                         GL_STATIC_DRAW);
                                                                                                                        }

                                                                                                                        glBindVertexArray(0);
                                                                                                                        src.gpuPrimitives.push_back(gpu);
                                                                                                                    }

                                                                                                                    std::cout << "[Renderer] Uploaded static scene: "
                                                                                                                    << src.gpuPrimitives.size() << " primitive(s)\n";
                                                                                                                }

                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                // renderStaticSceneEntity — draw all visible scene primitives
                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                void Renderer::renderStaticSceneEntity(const Entity& entity, unsigned int prog, const float* pv) {
                                                                                                                    auto* src = entity.getComponent<StaticSceneRenderComponent>();
                                                                                                                    if (!src || !src->asset || src->gpuPrimitives.empty()) return;

                                                                                                                    float entityModel[16];
                                                                                                                    buildModelMatrix(entity.transform(), entityModel);

                                                                                                                    int locMVP     = glGetUniformLocation(prog, "uMVP");
                                                                                                                    int locModel   = glGetUniformLocation(prog, "uModel");
                                                                                                                    int locSkinned = glGetUniformLocation(prog, "uSkinned");
                                                                                                                    int locUseGeometryNormals = glGetUniformLocation(prog, "uUseGeometryNormals");
                                                                                                                    int locColor   = glGetUniformLocation(prog, "uColor");
                                                                                                                    int locAlpha   = glGetUniformLocation(prog, "uAlpha");
                                                                                                                    int locHasBase = glGetUniformLocation(prog, "uHasBaseColorTexture");
                                                                                                                    int locBaseTex = glGetUniformLocation(prog, "uBaseColorTexture");
                                                                                                                    int locAlphaMode = glGetUniformLocation(prog, "uAlphaMode");
                                                                                                                    int locCutoff = glGetUniformLocation(prog, "uAlphaCutoff");

                                                                                                                    // Material-controlled bloom uniforms. Static scene bloom is enabled only
                                                                                                                    // when the Blender material name explicitly says BLOOM/EMISSIVE/GLOW.
                                                                                                                    int locSwordWhiteEmission = glGetUniformLocation(prog, "uSwordWhiteEmission");
                                                                                                                    int locSwordEmissionStrength = glGetUniformLocation(prog, "uSwordEmissionStrength");
                                                                                                                    int locBloomMask = glGetUniformLocation(prog, "uBloomMask");
                                                                                                                    int locBloomTint = glGetUniformLocation(prog, "uBloomTint");
                                                                                                                    int locBloomStrength = glGetUniformLocation(prog, "uBloomStrength");
                                                                                                                    int locExplicitBloomMaterial = glGetUniformLocation(prog, "uExplicitBloomMaterial");

                                                                                                                    if (locSkinned >= 0) glUniform1i(locSkinned, 0);
                                                                                                                    if (locUseGeometryNormals >= 0) glUniform1i(locUseGeometryNormals, 0);
                                                                                                                    if (locHasBase >= 0) glUniform1i(locHasBase, 0);
                                                                                                                    if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
                                                                                                                    if (locAlphaMode >= 0) glUniform1i(locAlphaMode, 0);
                                                                                                                    if (locCutoff >= 0) glUniform1f(locCutoff, 0.5f);
                                                                                                                    if (locSwordWhiteEmission >= 0) glUniform1i(locSwordWhiteEmission, 0);
                                                                                                                    if (locSwordEmissionStrength >= 0) glUniform1f(locSwordEmissionStrength, 0.0f);

                                                                                                                    glActiveTexture(GL_TEXTURE0);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, 0);

                                                                                                                    glDisable(GL_BLEND);

                                                                                                                    for (const auto& gpu : src->gpuPrimitives) {
                                                                                                                        if (!gpu.vao || !gpu.visible) continue;

                                                                                                                        // Do not let one primitive's material state leak into the next.
                                                                                                                        resetMaterialBloomUniforms(prog);
                                                                                                                        if (locHasBase >= 0) glUniform1i(locHasBase, 0);
                                                                                                                        if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
                                                                                                                        if (locAlphaMode >= 0) glUniform1i(locAlphaMode, 0);
                                                                                                                        glDisable(GL_BLEND);
                                                                                                                        glDepthMask(GL_TRUE);
                                                                                                                        glActiveTexture(GL_TEXTURE0);
                                                                                                                        glBindTexture(GL_TEXTURE_2D, 0);

                                                                                                                        float finalModel[16];
                                                                                                                        mat4Mul(entityModel, gpu.nodeTransform, finalModel);

                                                                                                                        float mvp[16];
                                                                                                                        mat4Mul(pv, finalModel, mvp);

                                                                                                                        if (locMVP >= 0)   glUniformMatrix4fv(locMVP,   1, GL_FALSE, mvp);
                                                                                                                        if (locModel >= 0) glUniformMatrix4fv(locModel, 1, GL_FALSE, finalModel);

                                                                                                                        const StaticSceneMaterial* mat = nullptr;
                                                                                                                        if (gpu.materialIndex >= 0 && gpu.materialIndex < (int)src->asset->materials.size()) {
                                                                                                                            mat = &src->asset->materials[gpu.materialIndex];
                                                                                                                        }

                                                                                                                        if (locColor >= 0) {
                                                                                                                            if (mat) glUniform3f(locColor,
                                                                                                                                mat->baseColorFactor[0],
                                                                                                                                mat->baseColorFactor[1],
                                                                                                                                mat->baseColorFactor[2]);
                                                                                                                            else     glUniform3f(locColor, 0.7f, 0.7f, 0.7f);
                                                                                                                        }

                                                                                                                        if (mat) {
                                                                                                                            if (locAlpha >= 0) glUniform1f(locAlpha, mat->baseColorFactor[3]);
                                                                                                                            if (locAlphaMode >= 0) glUniform1i(locAlphaMode, mat->alphaMask ? 1 : (mat->alphaBlend ? 2 : 0));
                                                                                                                            if (locCutoff >= 0) glUniform1f(locCutoff, mat->alphaCutoff);

                                                                                                                            if (mat->alphaBlend || mat->baseColorFactor[3] < 0.999f) {
                                                                                                                                glEnable(GL_BLEND);
                                                                                                                                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                                                                                                                            } else {
                                                                                                                                glDisable(GL_BLEND);
                                                                                                                            }
                                                                                                                        } else {
                                                                                                                            if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
                                                                                                                            if (locAlphaMode >= 0) glUniform1i(locAlphaMode, 0);
                                                                                                                            if (locCutoff >= 0) glUniform1f(locCutoff, 0.5f);
                                                                                                                            glDisable(GL_BLEND);
                                                                                                                        }

                                                                                                                        const StaticSceneGpuMaterial* gpuMat = nullptr;
                                                                                                                        if (gpu.materialIndex >= 0 && gpu.materialIndex < static_cast<int>(src->gpuMaterials.size())) {
                                                                                                                            gpuMat = &src->gpuMaterials[gpu.materialIndex];
                                                                                                                        }

                                                                                                                        const bool hasBaseTexture = gpuMat && gpuMat->hasBaseColorTexture && gpuMat->baseColorTexture != 0;
                                                                                                                        if (locBaseTex >= 0) glUniform1i(locBaseTex, 0);
                                                                                                                        if (hasBaseTexture) {
                                                                                                                            glActiveTexture(GL_TEXTURE0);
                                                                                                                            glBindTexture(GL_TEXTURE_2D, gpuMat->baseColorTexture);
                                                                                                                            if (locBaseTex >= 0) glUniform1i(locBaseTex, 0);
                                                                                                                        } else {
                                                                                                                            glActiveTexture(GL_TEXTURE0);
                                                                                                                            glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                        }
                                                                                                                        if (locHasBase >= 0) glUniform1i(locHasBase, hasBaseTexture ? 1 : 0);

                                                                                                                        // Explicit material identity: normal bright/white architecture is not bloom.
                                                                                                                        // Bloom/emission comes from GLB emissive data or Blender material prefixes
                                                                                                                        // such as BLOOM_*, EMISSIVE_*, GLOW_*, AX_BLOOM_*, AX_EMIT_*, VFX_*.
                                                                                                                        float bloomMask = 0.0f;
                                                                                                                        float bloomStrength = 0.0f;
                                                                                                                        float bloomTintR = 1.0f;
                                                                                                                        float bloomTintG = 1.0f;
                                                                                                                        float bloomTintB = 1.0f;
                                                                                                                        int explicitBloomMaterial = 0;

                                                                                                                        if (mat) {
                                                                                                                            const bool hasExplicitEmissionName = axMaterialNameHasExplicitEmission(mat->name);
                                                                                                                            const bool hasExplicitBloomName = axMaterialNameHasExplicitBloom(mat->name);
                                                                                                                            const bool shouldEmit = mat->isBloomSource || mat->isEmissiveSource || hasExplicitEmissionName;
                                                                                                                            const bool shouldBloom = mat->isBloomSource || hasExplicitBloomName;

                                                                                                                            // Baked/albedo textures are normal surface color unless the material
                                                                                                                            // is explicitly named BLOOM_/AX_BLOOM_/VFX_ or carries GLB emissive.
                                                                                                                            // A material having a texture must never make it an automatic bloom source.
                                                                                                                            if (shouldEmit) {
                                                                                                                                bloomTintR = std::max(mat->baseColorFactor[0], mat->emissiveFactor[0]);
                                                                                                                                bloomTintG = std::max(mat->baseColorFactor[1], mat->emissiveFactor[1]);
                                                                                                                                bloomTintB = std::max(mat->baseColorFactor[2], mat->emissiveFactor[2]);
                                                                                                                                bloomMask = shouldBloom ? 1.0f : 0.0f;
                                                                                                                                bloomStrength = mat->emissiveStrength > 0.001f ? mat->emissiveStrength : 1.35f;
                                                                                                                                explicitBloomMaterial = shouldEmit ? 1 : 0;
                                                                                                                            }
                                                                                                                        }

                                                                                                                        if (locBloomMask >= 0) glUniform1f(locBloomMask, bloomMask);
                                                                                                                        if (locBloomStrength >= 0) glUniform1f(locBloomStrength, bloomStrength);
                                                                                                                        if (locBloomTint >= 0) glUniform3f(locBloomTint, bloomTintR, bloomTintG, bloomTintB);
                                                                                                                        if (locExplicitBloomMaterial >= 0) glUniform1i(locExplicitBloomMaterial, explicitBloomMaterial);

                                                                                                                        const bool transparentStaticPrimitive =
                                                                                                                        mat && (mat->alphaBlend || mat->baseColorFactor[3] < 0.999f);
                                                                                                                        if (transparentStaticPrimitive) {
                                                                                                                            // Transparent level/VFX planes should never write depth before
                                                                                                                            // actors are drawn, otherwise the player/enemies can disappear
                                                                                                                            // behind an invisible or semi-transparent GLB material.
                                                                                                                            glDepthMask(GL_FALSE);
                                                                                                                        }

                                                                                                                        glBindVertexArray(gpu.vao);
                                                                                                                        if (gpu.indexCount > 0)
                                                                                                                            glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, nullptr);
                                                                                                                        else
                                                                                                                            glDrawArrays(GL_TRIANGLES, 0, gpu.vertexCount);

                                                                                                                        if (transparentStaticPrimitive) {
                                                                                                                            glDepthMask(GL_TRUE);
                                                                                                                        }
                                                                                                                    }

                                                                                                                    glBindVertexArray(0);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                    glDepthMask(GL_TRUE);
                                                                                                                    glDisable(GL_BLEND);
                                                                                                                }

                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                // uploadSkinnedMesh — upload every primitive of a SkinnedMeshAsset to the GPU
                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                void Renderer::uploadSkinnedMesh(SkinnedRenderComponent& src) {
                                                                                                                    // Clean up any previously uploaded GPU primitives
                                                                                                                    for (auto& g : src.gpuPrimitives) {
                                                                                                                        if (g.vao) glDeleteVertexArrays(1, &g.vao);
                                                                                                                        if (g.vbo) glDeleteBuffers(1, &g.vbo);
                                                                                                                        if (g.ebo) glDeleteBuffers(1, &g.ebo);
                                                                                                                    }
                                                                                                                    src.gpuPrimitives.clear();

                                                                                                                    // Clean up previously uploaded material textures
                                                                                                                    for (auto& m : src.gpuMaterials) {
                                                                                                                        if (m.baseColorTexture) {
                                                                                                                            glDeleteTextures(1, &m.baseColorTexture);
                                                                                                                            m.baseColorTexture = 0;
                                                                                                                        }
                                                                                                                    }
                                                                                                                    src.gpuMaterials.clear();

                                                                                                                    if (!src.asset) return;

                                                                                                                    // Upload material textures once. Materials reference image indices extracted from the GLB.
                                                                                                                    src.gpuMaterials.resize(src.asset->materials.size());

                                                                                                                    for (int mi = 0; mi < (int)src.asset->materials.size(); ++mi) {
                                                                                                                        const GltfMaterial& mat = src.asset->materials[mi];
                                                                                                                        SkinnedGpuMaterial& gpuMat = src.gpuMaterials[mi];

                                                                                                                        int imgIndex = mat.baseColorTextureIndex;
                                                                                                                        if (imgIndex >= 0 && imgIndex < (int)src.asset->images.size()) {
                                                                                                                            unsigned int tex = uploadGltfImageTexture(src.asset->images[imgIndex]);
                                                                                                                            gpuMat.baseColorTexture = tex;
                                                                                                                            gpuMat.hasBaseColorTexture = (tex != 0);

                                                                                                                            const auto& img = src.asset->images[imgIndex];
                                                                                                                            std::cout << "[Renderer] Uploaded GLB texture image " << imgIndex
                                                                                                                            << " (" << img.width << "x" << img.height
                                                                                                                            << ", comp=" << img.component << ") for material "
                                                                                                                            << mi << " \"" << mat.name << "\"\n";
                                                                                                                        }
                                                                                                                    }

                                                                                                                    src.gpuPrimitives.reserve(src.asset->primitives.size());

                                                                                                                    for (const auto& prim : src.asset->primitives) {
                                                                                                                        SkinnedGpuPrimitive gpu{};
                                                                                                                        gpu.indexCount    = (unsigned int)prim.indices.size();
                                                                                                                        gpu.vertexCount   = (unsigned int)prim.vertices.size();
                                                                                                                        gpu.materialIndex = prim.materialIndex;
                                                                                                                        gpu.hasSkin       = prim.hasSkin;

                                                                                                                        gpu.primitiveIndex = prim.primitiveIndex;
                                                                                                                        gpu.nodeIndex = prim.nodeIndex;
                                                                                                                        gpu.nodeName = prim.nodeName;
                                                                                                                        gpu.meshIndex = prim.meshIndex;
                                                                                                                        gpu.meshName = prim.meshName;
                                                                                                                        gpu.materialName = prim.materialName;
                                                                                                                        gpu.visible = prim.visible;

                                                                                                                        gpu.hasNodeTransform = prim.hasNodeTransform;
                                                                                                                        gpu.attachedJoint = prim.attachedJoint;
                                                                                                                        memcpy(gpu.nodeTransform, prim.nodeTransform, 64);
                                                                                                                        memcpy(gpu.localFromJoint, prim.localFromJoint, 64);

                                                                                                                        glGenVertexArrays(1, &gpu.vao);
                                                                                                                        glGenBuffers(1, &gpu.vbo);

                                                                                                                        glBindVertexArray(gpu.vao);

                                                                                                                        glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
                                                                                                                        glBufferData(GL_ARRAY_BUFFER,
                                                                                                                                     prim.vertices.size() * sizeof(SkinnedVertex),
                                                                                                                                     prim.vertices.data(), GL_STATIC_DRAW);

                                                                                                                        // loc 0 – position (vec3)
                                                                                                                        glEnableVertexAttribArray(0);
                                                                                                                        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(SkinnedVertex),
                                                                                                                                              (void*)offsetof(SkinnedVertex, position));

                                                                                                                        // loc 1 – normal (vec3)
                                                                                                                        glEnableVertexAttribArray(1);
                                                                                                                        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(SkinnedVertex),
                                                                                                                                              (void*)offsetof(SkinnedVertex, normal));

                                                                                                                        // loc 2 – uv (vec2)
                                                                                                                        glEnableVertexAttribArray(2);
                                                                                                                        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(SkinnedVertex),
                                                                                                                                              (void*)offsetof(SkinnedVertex, uv));

                                                                                                                        // loc 3 – tangent (vec4)
                                                                                                                        glEnableVertexAttribArray(3);
                                                                                                                        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(SkinnedVertex),
                                                                                                                                              (void*)offsetof(SkinnedVertex, tangent));

                                                                                                                        // loc 4 – joints (uvec4) — MUST use IPointer, not Pointer
                                                                                                                        glEnableVertexAttribArray(4);
                                                                                                                        glVertexAttribIPointer(4, 4, GL_UNSIGNED_SHORT,
                                                                                                                                               sizeof(SkinnedVertex),
                                                                                                                                               (void*)offsetof(SkinnedVertex, joints));

                                                                                                                        // loc 5 – weights (vec4)
                                                                                                                        glEnableVertexAttribArray(5);
                                                                                                                        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE,
                                                                                                                                              sizeof(SkinnedVertex),
                                                                                                                                              (void*)offsetof(SkinnedVertex, weights));

                                                                                                                        if (!prim.indices.empty()) {
                                                                                                                            glGenBuffers(1, &gpu.ebo);
                                                                                                                            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
                                                                                                                            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                                                                                                                         prim.indices.size() * sizeof(uint32_t),
                                                                                                                                         prim.indices.data(), GL_STATIC_DRAW);
                                                                                                                        }

                                                                                                                        glBindVertexArray(0);
                                                                                                                        src.gpuPrimitives.push_back(std::move(gpu));
                                                                                                                    }

                                                                                                                    std::cout << "[Renderer] Uploaded skinned mesh: "
                                                                                                                    << src.gpuPrimitives.size() << " primitive(s), "
                                                                                                                    << src.asset->skeleton.joints.size() << " joints\n";
                                                                                                                }

                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                // renderSkinnedEntity — bind joint matrices via UBO + draw each primitive
                                                                                                                // ---------------------------------------------------------------------------
                                                                                                                void Renderer::renderSkinnedEntity(const Entity& entity, unsigned int prog) {
                                                                                                                    auto* src = entity.getComponent<SkinnedRenderComponent>();
                                                                                                                    if (!src || !src->asset || src->gpuPrimitives.empty()) return;

                                                                                                                    float entityModel[16];
                                                                                                                    buildModelMatrix(entity.transform(), entityModel);
                                                                                                                    int locModel = glGetUniformLocation(prog, "uModel");

                                                                                                                    const auto& jm = src->anim.jointMatrices;
                                                                                                                    int jointCount  = (int)jm.size() / 16;

                                                                                                                    if (jointCount > 0) {
                                                                                                                        if (src->jointUBO_ == 0) {
                                                                                                                            glGenBuffers(1, &src->jointUBO_);
                                                                                                                            glBindBuffer(GL_UNIFORM_BUFFER, src->jointUBO_);
                                                                                                                            glBufferData(GL_UNIFORM_BUFFER, 256 * 64, nullptr, GL_DYNAMIC_DRAW);
                                                                                                                            glBindBuffer(GL_UNIFORM_BUFFER, 0);
                                                                                                                        }

                                                                                                                        int uploadJointCount = jointCount;
                                                                                                                        if (uploadJointCount > 256) uploadJointCount = 256;

                                                                                                                        glBindBuffer(GL_UNIFORM_BUFFER, src->jointUBO_);
                                                                                                                        glBufferSubData(GL_UNIFORM_BUFFER, 0, uploadJointCount * 64, jm.data());
                                                                                                                        glBindBuffer(GL_UNIFORM_BUFFER, 0);
                                                                                                                        glBindBufferBase(GL_UNIFORM_BUFFER, 0, src->jointUBO_);

                                                                                                                        unsigned int blockIdx = glGetUniformBlockIndex(prog, "JointBlock");
                                                                                                                        if (blockIdx != GL_INVALID_INDEX)
                                                                                                                            glUniformBlockBinding(prog, blockIdx, 0);
                                                                                                                    }

                                                                                                                    int locSkinned   = glGetUniformLocation(prog, "uSkinned");
                                                                                                                    int locColor     = glGetUniformLocation(prog, "uColor");
                                                                                                                    int locAlpha     = glGetUniformLocation(prog, "uAlpha");
                                                                                                                    int locHasBase   = glGetUniformLocation(prog, "uHasBaseColorTexture");
                                                                                                                    int locBaseTex   = glGetUniformLocation(prog, "uBaseColorTexture");
                                                                                                                    int locAlphaMode = glGetUniformLocation(prog, "uAlphaMode");
                                                                                                                    int locCutoff    = glGetUniformLocation(prog, "uAlphaCutoff");

                                                                                                                    // VFX: sword-only white material emission.
                                                                                                                    // The shader still decides the exact white-material mask, but this
                                                                                                                    // per-primitive switch prevents every white material in the world from glowing.
                                                                                                                    int locSwordWhiteEmission = glGetUniformLocation(prog, "uSwordWhiteEmission");
                                                                                                                    int locSwordEmissionColor = glGetUniformLocation(prog, "uSwordEmissionColor");
                                                                                                                    int locSwordEmissionStrength = glGetUniformLocation(prog, "uSwordEmissionStrength");

                                                                                                                    // Material-controlled bloom. Only primitives explicitly marked by material
                                                                                                                    // name, or the white sword rule, write into the bloom attachment.
                                                                                                                    int locBloomMask = glGetUniformLocation(prog, "uBloomMask");
                                                                                                                    int locBloomTint = glGetUniformLocation(prog, "uBloomTint");
                                                                                                                    int locBloomStrength = glGetUniformLocation(prog, "uBloomStrength");
                                                                                                                    int locExplicitBloomMaterial = glGetUniformLocation(prog, "uExplicitBloomMaterial");

                                                                                                                    for (int i = 0; i < (int)src->gpuPrimitives.size(); ++i) {
                                                                                                                        const SkinnedGpuPrimitive& gpu = src->gpuPrimitives[i];
                                                                                                                        if (!gpu.vao || !gpu.visible) continue;

                                                                                                                        // Hard reset per primitive.  The static level, player, enemies, debug
                                                                                                                        // lines, and VFX can all use the same shader, so state leakage is fatal.
                                                                                                                        resetMaterialBloomUniforms(prog);
                                                                                                                        if (locHasBase >= 0) glUniform1i(locHasBase, 0);
                                                                                                                        if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
                                                                                                                        if (locAlphaMode >= 0) glUniform1i(locAlphaMode, 0);
                                                                                                                        glDisable(GL_BLEND);
                                                                                                                        glDepthMask(GL_TRUE);
                                                                                                                        glActiveTexture(GL_TEXTURE0);
                                                                                                                        glBindTexture(GL_TEXTURE_2D, 0);

                                                                                                                        float primitiveLocal[16];
                                                                                                                        mat4Identity(primitiveLocal);

                                                                                                                        if (!gpu.hasSkin) {
                                                                                                                            const int animatedGlobalCount = (int)src->anim.animatedGlobalMatrices.size() / 16;
                                                                                                                            if (gpu.attachedJoint >= 0 && gpu.attachedJoint < animatedGlobalCount) {
                                                                                                                                mat4Mul(&src->anim.animatedGlobalMatrices[gpu.attachedJoint * 16],
                                                                                                                                        gpu.localFromJoint,
                                                                                                                                        primitiveLocal);
                                                                                                                            } else if (gpu.hasNodeTransform) {
                                                                                                                                memcpy(primitiveLocal, gpu.nodeTransform, 64);
                                                                                                                            }
                                                                                                                        }

                                                                                                                        float finalModel[16];
                                                                                                                        mat4Mul(entityModel, primitiveLocal, finalModel);
                                                                                                                        if (locModel >= 0)
                                                                                                                            glUniformMatrix4fv(locModel, 1, GL_FALSE, finalModel);

                                                                                                                        if (locSkinned >= 0)
                                                                                                                            glUniform1i(locSkinned, (gpu.hasSkin && jointCount > 0) ? 1 : 0);

                                                                                                                        const GltfMaterial* mat = nullptr;
                                                                                                                        const SkinnedGpuMaterial* gpuMat = nullptr;
                                                                                                                        if (gpu.materialIndex >= 0 && gpu.materialIndex < (int)src->asset->materials.size()) {
                                                                                                                            mat = &src->asset->materials[gpu.materialIndex];
                                                                                                                            if (gpu.materialIndex < (int)src->gpuMaterials.size())
                                                                                                                                gpuMat = &src->gpuMaterials[gpu.materialIndex];
                                                                                                                        }

                                                                                                                        if (mat) {
                                                                                                                            if (locColor >= 0)
                                                                                                                                glUniform3f(locColor, mat->baseColorFactor[0], mat->baseColorFactor[1], mat->baseColorFactor[2]);
                                                                                                                            if (locAlpha >= 0)
                                                                                                                                glUniform1f(locAlpha, mat->baseColorFactor[3]);
                                                                                                                            if (locAlphaMode >= 0)
                                                                                                                                glUniform1i(locAlphaMode, mat->alphaMask ? 1 : (mat->alphaBlend ? 2 : 0));
                                                                                                                            if (locCutoff >= 0)
                                                                                                                                glUniform1f(locCutoff, mat->alphaCutoff);

                                                                                                                            if (mat->alphaBlend || mat->baseColorFactor[3] < 0.999f) {
                                                                                                                                glEnable(GL_BLEND);
                                                                                                                                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                                                                                                                            } else {
                                                                                                                                glDisable(GL_BLEND);
                                                                                                                            }
                                                                                                                        } else {
                                                                                                                            if (locColor >= 0) glUniform3f(locColor, src->color[0], src->color[1], src->color[2]);
                                                                                                                            if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
                                                                                                                            if (locAlphaMode >= 0) glUniform1i(locAlphaMode, 0);
                                                                                                                            if (locCutoff >= 0) glUniform1f(locCutoff, 0.5f);
                                                                                                                            glDisable(GL_BLEND);
                                                                                                                        }

                                                                                                                        // Sword VFX rule:
                                                                                                                        // Only sword primitives can enable the white-material emission path.
                                                                                                                        // This relies on Blender/node/mesh/material names copied from the GLB.
                                                                                                                        const auto lowerCopyLocal = [](std::string value) {
                                                                                                                            for (char& c : value) {
                                                                                                                                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                                                                                                                            }
                                                                                                                            return value;
                                                                                                                        };

                                                                                                                        const std::string nodeLower = lowerCopyLocal(gpu.nodeName);
                                                                                                                        const std::string meshLower = lowerCopyLocal(gpu.meshName);
                                                                                                                        const std::string matLower  = lowerCopyLocal(gpu.materialName);

                                                                                                                        const bool isSwordPrimitive =
                                                                                                                        nodeLower.find("sword") != std::string::npos ||
                                                                                                                        meshLower.find("sword") != std::string::npos ||
                                                                                                                        matLower.find("sword") != std::string::npos;

                                                                                                                        bool isWhiteSwordMaterial = false;
                                                                                                                        if (mat) {
                                                                                                                            const float r = mat->baseColorFactor[0];
                                                                                                                            const float g = mat->baseColorFactor[1];
                                                                                                                            const float b = mat->baseColorFactor[2];
                                                                                                                            const float maxC = std::max(r, std::max(g, b));
                                                                                                                            const float minC = std::min(r, std::min(g, b));
                                                                                                                            const float luma = r * 0.2126f + g * 0.7152f + b * 0.0722f;
                                                                                                                            const float chroma = maxC - minC;

                                                                                                                            const bool colorLooksWhite = luma > 0.68f && chroma < 0.22f;
                                                                                                                            const bool nameLooksWhite =
                                                                                                                            matLower.find("white") != std::string::npos ||
                                                                                                                            matLower.find("light") != std::string::npos ||
                                                                                                                            matLower.find("holy")  != std::string::npos ||
                                                                                                                            matLower.find("glow")  != std::string::npos ||
                                                                                                                            matLower.find("blade") != std::string::npos;

                                                                                                                            isWhiteSwordMaterial = isSwordPrimitive && (colorLooksWhite || nameLooksWhite);
                                                                                                                        }

                                                                                                                        const bool materialWantsBloom =
                                                                                                                        (mat && (mat->isBloomSource || mat->isEmissiveSource)) ||
                                                                                                                        axMaterialNameHasExplicitEmission(gpu.materialName);
                                                                                                                        const bool materialShouldWriteBloom =
                                                                                                                        (mat && mat->isBloomSource) ||
                                                                                                                        axMaterialNameHasExplicitBloom(gpu.materialName);

                                                                                                                        float bloomMask = 0.0f;
                                                                                                                        float bloomStrength = 0.0f;
                                                                                                                        float bloomTintR = 1.0f;
                                                                                                                        float bloomTintG = 1.0f;
                                                                                                                        float bloomTintB = 1.0f;
                                                                                                                        int explicitBloomMaterial = 0;

                                                                                                                        if (materialWantsBloom && mat) {
                                                                                                                            bloomMask = materialShouldWriteBloom ? 1.0f : 0.0f;
                                                                                                                            bloomStrength = mat->emissiveStrength > 0.001f ? mat->emissiveStrength : 1.35f;
                                                                                                                            bloomTintR = std::max(mat->baseColorFactor[0], mat->emissiveFactor[0]);
                                                                                                                            bloomTintG = std::max(mat->baseColorFactor[1], mat->emissiveFactor[1]);
                                                                                                                            bloomTintB = std::max(mat->baseColorFactor[2], mat->emissiveFactor[2]);
                                                                                                                            explicitBloomMaterial = 1;
                                                                                                                        }

                                                                                                                        if (isWhiteSwordMaterial) {
                                                                                                                            bloomMask = 1.0f;
                                                                                                                            // Let the shader's white-region sword mask drive the bloom source.
                                                                                                                            // Do not enable explicitMaterialEmission here or the whole sword
                                                                                                                            // primitive would glow instead of only its white/light regions.
                                                                                                                            if (!materialWantsBloom) {
                                                                                                                                bloomStrength = 0.0f;
                                                                                                                                explicitBloomMaterial = 0;
                                                                                                                                bloomTintR = 0.78f;
                                                                                                                                bloomTintG = 0.93f;
                                                                                                                                bloomTintB = 1.00f;
                                                                                                                            }
                                                                                                                        }

                                                                                                                        if (locBloomMask >= 0) {
                                                                                                                            glUniform1f(locBloomMask, bloomMask);
                                                                                                                        }
                                                                                                                        if (locBloomTint >= 0) {
                                                                                                                            glUniform3f(locBloomTint, bloomTintR, bloomTintG, bloomTintB);
                                                                                                                        }
                                                                                                                        if (locBloomStrength >= 0) {
                                                                                                                            glUniform1f(locBloomStrength, bloomStrength);
                                                                                                                        }
                                                                                                                        if (locExplicitBloomMaterial >= 0) {
                                                                                                                            glUniform1i(locExplicitBloomMaterial, explicitBloomMaterial);
                                                                                                                        }

                                                                                                                        if (locSwordWhiteEmission >= 0) {
                                                                                                                            glUniform1i(locSwordWhiteEmission, isWhiteSwordMaterial ? 1 : 0);
                                                                                                                        }
                                                                                                                        if (locSwordEmissionColor >= 0) {
                                                                                                                            // White-gold/cyan blend. Good for holy sword without looking like a flashlight.
                                                                                                                            glUniform3f(locSwordEmissionColor, 0.78f, 0.93f, 1.00f);
                                                                                                                        }
                                                                                                                        if (locSwordEmissionStrength >= 0) {
                                                                                                                            glUniform1f(locSwordEmissionStrength, isWhiteSwordMaterial ? 2.75f : 0.0f);
                                                                                                                        }

                                                                                                                        bool hasBaseTexture = gpuMat && gpuMat->hasBaseColorTexture && gpuMat->baseColorTexture != 0;
                                                                                                                        if (locBaseTex >= 0) glUniform1i(locBaseTex, 0);
                                                                                                                        if (hasBaseTexture) {
                                                                                                                            glActiveTexture(GL_TEXTURE0);
                                                                                                                            glBindTexture(GL_TEXTURE_2D, gpuMat->baseColorTexture);
                                                                                                                            if (locBaseTex >= 0) glUniform1i(locBaseTex, 0);
                                                                                                                        } else {
                                                                                                                            glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                        }
                                                                                                                        if (locHasBase >= 0) glUniform1i(locHasBase, hasBaseTexture ? 1 : 0);

                                                                                                                        glBindVertexArray(gpu.vao);
                                                                                                                        if (gpu.indexCount > 0)
                                                                                                                            glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, nullptr);
                                                                                                                        else
                                                                                                                            glDrawArrays(GL_TRIANGLES, 0, gpu.vertexCount);
                                                                                                                    }

                                                                                                                    glBindVertexArray(0);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                    glDisable(GL_BLEND);
                                                                                                                }

                                                                                                                void Renderer::setSkyColor(float r, float g, float b) {
                                                                                                                    renderStyle_.sky.mode = SkySettings::Mode::Color;
                                                                                                                    renderStyle_.sky.enabled = true;
                                                                                                                    renderStyle_.skyEnabled = true;
                                                                                                                    renderStyle_.sky.color = {r, g, b};
                                                                                                                }

                                                                                                                void Renderer::setSkyTexture(const std::string& path) {
                                                                                                                    renderStyle_.sky.mode = SkySettings::Mode::Texture;
                                                                                                                    renderStyle_.sky.enabled = true;
                                                                                                                    renderStyle_.skyEnabled = true;

                                                                                                                    const bool skyPathChanged =
                                                                                                                    renderStyle_.sky.panoramaTexturePath != path;

                                                                                                                    renderStyle_.sky.panoramaTexturePath = path;

                                                                                                                    if (skyPathChanged && skyTextureID_) {
                                                                                                                        glDeleteTextures(1, &skyTextureID_);
                                                                                                                        skyTextureID_ = 0;
                                                                                                                        loadedSkyTexturePath_.clear();
                                                                                                                    }
                                                                                                                }

                                                                                                                void Renderer::useSkyColor() {
                                                                                                                    renderStyle_.sky.mode = SkySettings::Mode::Color;
                                                                                                                    renderStyle_.sky.enabled = true;
                                                                                                                    renderStyle_.skyEnabled = true;
                                                                                                                }

                                                                                                                void Renderer::useSkyTexture() {
                                                                                                                    renderStyle_.sky.mode = SkySettings::Mode::Texture;
                                                                                                                    renderStyle_.sky.enabled = true;
                                                                                                                    renderStyle_.skyEnabled = true;
                                                                                                                }

                                                                                                                void Renderer::setRenderStyle(const RenderStyle& style) {
                                                                                                                    const bool skyPathChanged =
                                                                                                                    renderStyle_.sky.panoramaTexturePath != style.sky.panoramaTexturePath;

                                                                                                                    renderStyle_ = style;
                                                                                                                    renderStyle_.syncFeaturesFromLegacyFlags();

                                                                                                                    bloomEnabled_ = renderStyle_.features.bloom && renderStyle_.bloom.enabled;
                                                                                                                    bloomThreshold_ = renderStyle_.bloom.threshold;
                                                                                                                    bloomStrength_ = renderStyle_.bloom.strength;
                                                                                                                    bloomRadius_ = renderStyle_.bloom.radius;

                                                                                                                    std::cout << "[Renderer] RenderStyle sunDirection=("
                                                                                                                    << renderStyle_.lighting.sunDirection.x << ", "
                                                                                                                    << renderStyle_.lighting.sunDirection.y << ", "
                                                                                                                    << renderStyle_.lighting.sunDirection.z << ") "
                                                                                                                    << "shadowBias=" << renderStyle_.lighting.shadowBias
                                                                                                                    << " shadowMapSoftness=" << renderStyle_.lighting.shadowMapSoftness
                                                                                                                    << " shadowOrthoRange=" << renderStyle_.lighting.shadowOrthoRange
                                                                                                                    << " skyRotation=" << renderStyle_.sky.rotation
                                                                                                                    << "\n";

                                                                                                                    if (skyPathChanged && skyTextureID_) {
                                                                                                                        glDeleteTextures(1, &skyTextureID_);
                                                                                                                        skyTextureID_ = 0;
                                                                                                                        loadedSkyTexturePath_.clear();
                                                                                                                    }
                                                                                                                }

                                                                                                                const RenderStyle& Renderer::renderStyle() const {
                                                                                                                    return renderStyle_;
                                                                                                                }

                                                                                                                void Renderer::applyRenderStyleUniforms(unsigned int program) {
                                                                                                                    const auto& style = renderStyle_;
                                                                                                                    const auto& l = style.lighting;

                                                                                                                    setVec3(program, "uCameraPos", cameraX_, cameraY_, cameraZ_);

                                                                                                                    setInt(program, "uUseStylizedLighting", style.features.stylizedLighting ? 1 : 0);
                                                                                                                    setInt(program, "uUseFog", style.features.fog ? 1 : 0);
                                                                                                                    setInt(program, "uUseCelShading", style.features.celShading ? 1 : 0);
                                                                                                                    setInt(program, "uUseRimLighting", style.features.rimLighting ? 1 : 0);
                                                                                                                    setInt(program, "uUseMaterialEmission", style.features.materialEmission ? 1 : 0);
                                                                                                                    setInt(program, "uUseBloom", (style.features.bloom && bloomEnabled_) ? 1 : 0);

                                                                                                                    setVec3(program, "uSunDirection",
                                                                                                                            l.sunDirection.x,
                                                                                                                            l.sunDirection.y,
                                                                                                                            l.sunDirection.z
                                                                                                                    );

                                                                                                                    setVec3(program, "uSunColor",
                                                                                                                            l.sunColor.x,
                                                                                                                            l.sunColor.y,
                                                                                                                            l.sunColor.z
                                                                                                                    );

                                                                                                                    setFloat(program, "uSunIntensity", l.sunIntensity);

                                                                                                                    setVec3(program, "uAmbientSky",
                                                                                                                            l.ambientSky.x,
                                                                                                                            l.ambientSky.y,
                                                                                                                            l.ambientSky.z
                                                                                                                    );

                                                                                                                    setVec3(program, "uAmbientGround",
                                                                                                                            l.ambientGround.x,
                                                                                                                            l.ambientGround.y,
                                                                                                                            l.ambientGround.z
                                                                                                                    );

                                                                                                                    setVec3(program, "uRimColor",
                                                                                                                            l.rimColor.x,
                                                                                                                            l.rimColor.y,
                                                                                                                            l.rimColor.z
                                                                                                                    );

                                                                                                                    setFloat(program, "uRimIntensity",
                                                                                                                             style.features.rimLighting ? l.rimIntensity : 0.0f
                                                                                                                    );

                                                                                                                    setFloat(program, "uRimPower", l.rimPower);

                                                                                                                    setVec3(program, "uFogColor",
                                                                                                                            l.fogColor.x,
                                                                                                                            l.fogColor.y,
                                                                                                                            l.fogColor.z
                                                                                                                    );

                                                                                                                    setFloat(program, "uFogDensity",
                                                                                                                             style.features.fog ? l.fogDensity : 0.0f
                                                                                                                    );

                                                                                                                    setFloat(program, "uCelStrength",
                                                                                                                             style.features.celShading ? l.celStrength : 0.0f
                                                                                                                    );

                                                                                                                    setFloat(program, "uShadowSoftness", l.shadowSoftness);

                                                                                                                    applyShadowUniforms(program);

                                                                                                                    setFloat(program, "uExposure", style.colorGrade.exposure);
                                                                                                                    setFloat(program, "uSaturation", style.colorGrade.saturation);
                                                                                                                    setFloat(program, "uContrast", style.colorGrade.contrast);
                                                                                                                    setFloat(program, "uGamma", style.colorGrade.gamma);

                                                                                                                    setVec3(program, "uShadowColor",
                                                                                                                            l.shadowColor.x,
                                                                                                                            l.shadowColor.y,
                                                                                                                            l.shadowColor.z
                                                                                                                    );

                                                                                                                    setFloat(program, "uShadowStrength", l.shadowStrength);
                                                                                                                    setFloat(program, "uMinLight", l.minLight);
                                                                                                                    setFloat(program, "uShadowBias", l.shadowBias);
                                                                                                                    setFloat(program, "uShadowMapSoftness", l.shadowMapSoftness);
                                                                                                                    setFloat(program, "uShadowTexelWorldSize", (l.shadowOrthoRange * 2.0f) / static_cast<float>(std::max(shadowMapSize_, 1)));

                                                                                                                    setInt(program, "uUseShadows",
                                                                                                                           (l.realtimeShadows && shadowDepthTexture_ != 0) ? 1 : 0
                                                                                                                    );

                                                                                                                    int locLightSpace = glGetUniformLocation(program, "uLightSpaceMatrix");
                                                                                                                    if (locLightSpace >= 0) {
                                                                                                                        glUniformMatrix4fv(locLightSpace, 1, GL_FALSE, lightSpaceMatrix_);
                                                                                                                    }

                                                                                                                    glActiveTexture(GL_TEXTURE5);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
                                                                                                                    setInt(program, "uShadowMap", 5);
                                                                                                                }


                                                                                                                bool Renderer::createShadowResources() {
                                                                                                                    if (shadowFBO_ && shadowDepthTexture_ && shadowProgramID_) {
                                                                                                                        return true;
                                                                                                                    }

                                                                                                                    if (!shadowProgramID_) {
                                                                                                                        const char* shadowVert = R"(
            #version 330 core
            #extension GL_ARB_shading_language_420pack : enable

            layout(location = 0) in vec3  aPosition;
            layout(location = 4) in uvec4 aJoints;
            layout(location = 5) in vec4  aWeights;

            uniform mat4 uModel;
            uniform mat4 uLightSpaceMatrix;
            uniform int  uSkinned;

            layout(std140, binding = 0) uniform JointBlock {
                mat4 uJointMatrices[256];
            };

            void main() {
                vec4 pos = vec4(aPosition, 1.0);

                if (uSkinned != 0) {
                    mat4 skin =
                        aWeights.x * uJointMatrices[aJoints.x] +
                        aWeights.y * uJointMatrices[aJoints.y] +
                        aWeights.z * uJointMatrices[aJoints.z] +
                        aWeights.w * uJointMatrices[aJoints.w];

                    pos = skin * pos;
                }

                gl_Position = uLightSpaceMatrix * uModel * pos;
            }
            )";

        const char* shadowFrag = R"(
            #version 330 core
            void main() {
                // Depth only.
            }
        )";

    shadowProgramID_ = linkProgramSources(
        shadowVert,
        shadowFrag,
        "shadow depth"
    );

    if (!shadowProgramID_) {
        std::cerr << "[Renderer] Failed to create shadow shader.\n";
        return false;
    }
                                                                                                                    }

                                                                                                                    if (!shadowFBO_) {
                                                                                                                        glGenFramebuffers(1, &shadowFBO_);
                                                                                                                    }

                                                                                                                    if (!shadowDepthTexture_) {
                                                                                                                        glGenTextures(1, &shadowDepthTexture_);
                                                                                                                        glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);

                                                                                                                        glTexImage2D(
                                                                                                                            GL_TEXTURE_2D,
                                                                                                                            0,
                                                                                                                            GL_DEPTH_COMPONENT32F,
                                                                                                                            shadowMapSize_,
                                                                                                                            shadowMapSize_,
                                                                                                                            0,
                                                                                                                            GL_DEPTH_COMPONENT,
                                                                                                                            GL_FLOAT,
                                                                                                                            nullptr
                                                                                                                        );

                                                                                                                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                                                                                                                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                                                                                                                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                                                                                                                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

                                                                                                                        float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
                                                                                                                        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

                                                                                                                        glBindTexture(GL_TEXTURE_2D, 0);
                                                                                                                    }

                                                                                                                    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
                                                                                                                    glFramebufferTexture2D(
                                                                                                                        GL_FRAMEBUFFER,
                                                                                                                        GL_DEPTH_ATTACHMENT,
                                                                                                                        GL_TEXTURE_2D,
                                                                                                                        shadowDepthTexture_,
                                                                                                                        0
                                                                                                                    );

                                                                                                                    glDrawBuffer(GL_NONE);
                                                                                                                    glReadBuffer(GL_NONE);

                                                                                                                    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

                                                                                                                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                                                                                                                    if (!ok) {
                                                                                                                        std::cerr << "[Renderer] Shadow framebuffer incomplete.\n";
                                                                                                                        return false;
                                                                                                                    }

                                                                                                                    std::cout << "[Renderer] Shadow map ready: "
                                                                                                                    << shadowMapSize_ << "x" << shadowMapSize_ << "\n";

                                                                                                                    return true;
                                                                                                                }

                                                                                                                void Renderer::renderShadowPass(Scene& scene) {
                                                                                                                    if (!createShadowResources()) {
                                                                                                                        return;
                                                                                                                    }

                                                                                                                    glViewport(0, 0, shadowMapSize_, shadowMapSize_);
                                                                                                                    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
                                                                                                                    glClear(GL_DEPTH_BUFFER_BIT);

                                                                                                                    // Contact-shadow fix:
                                                                                                                    // Do NOT use polygon offset in the shadow depth pass for this renderer.
                                                                                                                    // glPolygonOffset pushes the stored shadow-caster depth away from the real
                                                                                                                    // mesh surface, which creates visible detached / peter-panning shadows even
                                                                                                                    // when Environment.h sets shadowBias to 0.0f.
                                                                                                                    GLboolean wasPolygonOffsetFillEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
                                                                                                                    glDisable(GL_POLYGON_OFFSET_FILL);

                                                                                                                    glUseProgram(shadowProgramID_);

                                                                                                                    int locModel = glGetUniformLocation(shadowProgramID_, "uModel");
                                                                                                                    int locLight = glGetUniformLocation(shadowProgramID_, "uLightSpaceMatrix");
                                                                                                                    int locSkinned = glGetUniformLocation(shadowProgramID_, "uSkinned");

                                                                                                                    if (locLight >= 0) {
                                                                                                                        glUniformMatrix4fv(locLight, 1, GL_FALSE, lightSpaceMatrix_);
                                                                                                                    }

                                                                                                                    GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
                                                                                                                    GLint oldCullFace = GL_BACK;
                                                                                                                    glGetIntegerv(GL_CULL_FACE_MODE, &oldCullFace);

                                                                                                                    // Shadow pass culling policy:
                                                                                                                    // Static level geometry is often made of thin / single-sided graybox panels,
                                                                                                                    // so render its front faces into the shadow map. Front-face culling makes
                                                                                                                    // those panels cast bloated or missing shadows.
                                                                                                                    glEnable(GL_CULL_FACE);
                                                                                                                    glCullFace(GL_BACK);

                                                                                                                    // Regular non-skinned mesh entities.
                                                                                                                    for (auto& entityPtr : scene.entities()) {
                                                                                                                        Entity* e = entityPtr.get();
                                                                                                                        if (!e || !e->active) continue;

                                                                                                                        auto* rc = e->getComponent<RenderComponent>();
                                                                                                                        if (!rc || !rc->vao) continue;

                                                                                                                        float model[16];
                                                                                                                        buildModelMatrix(e->transform(), model);

                                                                                                                        if (locModel >= 0) {
                                                                                                                            glUniformMatrix4fv(locModel, 1, GL_FALSE, model);
                                                                                                                        }

                                                                                                                        if (locSkinned >= 0) {
                                                                                                                            glUniform1i(locSkinned, 0);
                                                                                                                        }

                                                                                                                        glBindVertexArray(rc->vao);

                                                                                                                        if (rc->useIndices) {
                                                                                                                            glDrawElements(GL_TRIANGLES, rc->indexCount, GL_UNSIGNED_INT, 0);
                                                                                                                        } else {
                                                                                                                            glDrawArrays(GL_TRIANGLES, 0, rc->vertexCount);
                                                                                                                        }
                                                                                                                    }

                                                                                                                    // Static scene GLB entities.
                                                                                                                    for (auto& entityPtr : scene.entities()) {
                                                                                                                        Entity* e = entityPtr.get();
                                                                                                                        if (!e || !e->active) continue;

                                                                                                                        auto* src = e->getComponent<StaticSceneRenderComponent>();
                                                                                                                        if (!src || !src->asset || src->gpuPrimitives.empty()) continue;

                                                                                                                        float entityModel[16];
                                                                                                                        buildModelMatrix(e->transform(), entityModel);

                                                                                                                        if (locSkinned >= 0) {
                                                                                                                            glUniform1i(locSkinned, 0);
                                                                                                                        }

                                                                                                                        for (const auto& gpu : src->gpuPrimitives) {
                                                                                                                            if (!gpu.vao || !gpu.visible) continue;

                                                                                                                            float finalModel[16];
                                                                                                                            mat4Mul(entityModel, gpu.nodeTransform, finalModel);

                                                                                                                            if (locModel >= 0) {
                                                                                                                                glUniformMatrix4fv(locModel, 1, GL_FALSE, finalModel);
                                                                                                                            }

                                                                                                                            glBindVertexArray(gpu.vao);

                                                                                                                            if (gpu.indexCount > 0) {
                                                                                                                                glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, nullptr);
                                                                                                                            } else {
                                                                                                                                glDrawArrays(GL_TRIANGLES, 0, gpu.vertexCount);
                                                                                                                            }
                                                                                                                        }
                                                                                                                    }

                                                                                                                    // Skinned character meshes are closed enough that back-face shadow casting
                                                                                                                    // helps reduce self-acne on the body without breaking the thin level panels.
                                                                                                                    glCullFace(GL_FRONT);

                                                                                                                    // Skinned character entities.
                                                                                                                    for (auto& entityPtr : scene.entities()) {
                                                                                                                        Entity* e = entityPtr.get();
                                                                                                                        if (!e || !e->active) continue;

                                                                                                                        auto* src = e->getComponent<SkinnedRenderComponent>();
                                                                                                                        if (!src || !src->asset || src->gpuPrimitives.empty()) continue;

                                                                                                                        float entityModel[16];
                                                                                                                        buildModelMatrix(e->transform(), entityModel);

                                                                                                                        const auto& jm = src->anim.jointMatrices;
                                                                                                                        int jointCount = static_cast<int>(jm.size()) / 16;

                                                                                                                        if (jointCount > 0) {
                                                                                                                            if (src->jointUBO_ == 0) {
                                                                                                                                glGenBuffers(1, &src->jointUBO_);
                                                                                                                                glBindBuffer(GL_UNIFORM_BUFFER, src->jointUBO_);
                                                                                                                                glBufferData(GL_UNIFORM_BUFFER, 256 * 64, nullptr, GL_DYNAMIC_DRAW);
                                                                                                                                glBindBuffer(GL_UNIFORM_BUFFER, 0);
                                                                                                                            }

                                                                                                                            int uploadJointCount = jointCount;
                                                                                                                            if (uploadJointCount > 256) uploadJointCount = 256;

                                                                                                                            glBindBuffer(GL_UNIFORM_BUFFER, src->jointUBO_);
                                                                                                                            glBufferSubData(GL_UNIFORM_BUFFER, 0, uploadJointCount * 64, jm.data());
                                                                                                                            glBindBuffer(GL_UNIFORM_BUFFER, 0);
                                                                                                                            glBindBufferBase(GL_UNIFORM_BUFFER, 0, src->jointUBO_);

                                                                                                                            unsigned int blockIdx = glGetUniformBlockIndex(shadowProgramID_, "JointBlock");
                                                                                                                            if (blockIdx != GL_INVALID_INDEX) {
                                                                                                                                glUniformBlockBinding(shadowProgramID_, blockIdx, 0);
                                                                                                                            }
                                                                                                                        }

                                                                                                                        for (const auto& gpu : src->gpuPrimitives) {
                                                                                                                            if (!gpu.vao || !gpu.visible) continue;

                                                                                                                            float primitiveLocal[16];
                                                                                                                            mat4Identity(primitiveLocal);

                                                                                                                            if (!gpu.hasSkin) {
                                                                                                                                const int animatedGlobalCount =
                                                                                                                                static_cast<int>(src->anim.animatedGlobalMatrices.size()) / 16;

                                                                                                                                if (gpu.attachedJoint >= 0 && gpu.attachedJoint < animatedGlobalCount) {
                                                                                                                                    mat4Mul(
                                                                                                                                        &src->anim.animatedGlobalMatrices[gpu.attachedJoint * 16],
                                                                                                                                        gpu.localFromJoint,
                                                                                                                                        primitiveLocal
                                                                                                                                    );
                                                                                                                                } else if (gpu.hasNodeTransform) {
                                                                                                                                    memcpy(primitiveLocal, gpu.nodeTransform, 64);
                                                                                                                                }
                                                                                                                            }

                                                                                                                            float finalModel[16];
                                                                                                                            mat4Mul(entityModel, primitiveLocal, finalModel);

                                                                                                                            if (locModel >= 0) {
                                                                                                                                glUniformMatrix4fv(locModel, 1, GL_FALSE, finalModel);
                                                                                                                            }

                                                                                                                            if (locSkinned >= 0) {
                                                                                                                                glUniform1i(locSkinned, (gpu.hasSkin && jointCount > 0) ? 1 : 0);
                                                                                                                            }

                                                                                                                            glBindVertexArray(gpu.vao);

                                                                                                                            if (gpu.indexCount > 0) {
                                                                                                                                glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, nullptr);
                                                                                                                            } else {
                                                                                                                                glDrawArrays(GL_TRIANGLES, 0, gpu.vertexCount);
                                                                                                                            }
                                                                                                                        }
                                                                                                                    }

                                                                                                                    glCullFace(oldCullFace);
                                                                                                                    if (wasCullEnabled) {
                                                                                                                        glEnable(GL_CULL_FACE);
                                                                                                                    } else {
                                                                                                                        glDisable(GL_CULL_FACE);
                                                                                                                    }

                                                                                                                    // Restore polygon offset state. For normal use this should stay disabled.
                                                                                                                    if (wasPolygonOffsetFillEnabled) {
                                                                                                                        glEnable(GL_POLYGON_OFFSET_FILL);
                                                                                                                    } else {
                                                                                                                        glDisable(GL_POLYGON_OFFSET_FILL);
                                                                                                                    }

                                                                                                                    glBindVertexArray(0);
                                                                                                                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                                                                                                                }

                                                                                                                void Renderer::applyShadowUniforms(unsigned int program) {
                                                                                                                    const auto& l = renderStyle_.lighting;

                                                                                                                    setVec3(program, "uShadowColor",
                                                                                                                            l.shadowColor.x,
                                                                                                                            l.shadowColor.y,
                                                                                                                            l.shadowColor.z
                                                                                                                    );

                                                                                                                    setFloat(program, "uShadowStrength", l.shadowStrength);
                                                                                                                    setFloat(program, "uMinLight", l.minLight);
                                                                                                                    setFloat(program, "uShadowBias", l.shadowBias);
                                                                                                                    setFloat(program, "uShadowMapSoftness", l.shadowMapSoftness);
                                                                                                                    setFloat(program, "uShadowTexelWorldSize", (l.shadowOrthoRange * 2.0f) / static_cast<float>(std::max(shadowMapSize_, 1)));

                                                                                                                    setInt(program, "uUseShadows",
                                                                                                                           (l.realtimeShadows && shadowDepthTexture_ != 0) ? 1 : 0
                                                                                                                    );

                                                                                                                    int locLightSpace = glGetUniformLocation(program, "uLightSpaceMatrix");
                                                                                                                    if (locLightSpace >= 0) {
                                                                                                                        glUniformMatrix4fv(locLightSpace, 1, GL_FALSE, lightSpaceMatrix_);
                                                                                                                    }

                                                                                                                    glActiveTexture(GL_TEXTURE5);
                                                                                                                    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
                                                                                                                    setInt(program, "uShadowMap", 5);

                                                                                                                    // Important: restore default material texture unit.
                                                                                                                    // Otherwise later glBindTexture(..., 0) calls may unbind the shadow map.
                                                                                                                    glActiveTexture(GL_TEXTURE0);
                                                                                                                }
