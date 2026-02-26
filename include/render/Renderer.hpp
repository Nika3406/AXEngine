// include/render/Renderer.hpp
#pragma once

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <entt/entt.hpp>

namespace render {

// ============================================================================
// Renderer - Main rendering system
// ============================================================================
class Renderer {
public:
    struct Config {
        uint32_t width = 1280;
        uint32_t height = 720;
        bool vsync = true;
        bool msaa = false;
        bgfx::RendererType::Enum rendererType = bgfx::RendererType::Count; // Auto-detect
        uint32_t clearColor = 0x101822ff;
        float clearDepth = 1.0f;
    };

    Renderer() = default;
    ~Renderer();

    // Initialize renderer
    bool init(const Config& config, void* nativeWindowHandle, void* nativeDisplayHandle = nullptr);
    
    // Shutdown renderer
    void shutdown();
    
    // Frame management
    void beginFrame();
    void endFrame();
    
    // View setup
    void setViewClear(bgfx::ViewId view, uint32_t color = 0x101822ff, float depth = 1.0f);
    void setViewRect(bgfx::ViewId view, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void setViewTransform(bgfx::ViewId view, const float* viewMtx, const float* projMtx);
    
    // Window resize
    void resize(uint32_t width, uint32_t height);
    
    // Debug text
    void debugText(uint16_t x, uint16_t y, uint8_t attr, const char* format, ...);
    void clearDebugText();
    
    // Getters
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    float getAspect() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }
    bool isInitialized() const { return m_initialized; }
    
    // Stats
    const bgfx::Stats* getStats() const { return bgfx::getStats(); }

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_initialized = false;
    Config m_config;
};

} // namespace render