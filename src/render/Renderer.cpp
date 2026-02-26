// src/render/Renderer.cpp
#include "render/Renderer.hpp"
#include <bgfx/platform.h>
#include <iostream>
#include <cstdarg>

namespace render {

Renderer::~Renderer() {
    if (m_initialized) {
        shutdown();
    }
}

bool Renderer::init(const Config& config, void* nativeWindowHandle, void* nativeDisplayHandle) {
    if (m_initialized) {
        std::cerr << "Renderer already initialized\n";
        return false;
    }
    
    m_config = config;
    m_width = config.width;
    m_height = config.height;
    
    // Setup platform data
    bgfx::PlatformData pd{};
    pd.nwh = nativeWindowHandle;
    pd.ndt = nativeDisplayHandle;
    bgfx::setPlatformData(pd);
    
    // Initialize bgfx
    bgfx::Init init{};
    init.type = config.rendererType;
    init.platformData = pd;
    init.resolution.width = m_width;
    init.resolution.height = m_height;
    init.resolution.reset = BGFX_RESET_VSYNC;
    
    if (config.msaa) {
        init.resolution.reset |= BGFX_RESET_MSAA_X4;
    }
    
    if (!bgfx::init(init)) {
        std::cerr << "Failed to initialize bgfx\n";
        return false;
    }
    
    // Set debug flags
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    
    // Set default view clear
    setViewClear(0, config.clearColor, config.clearDepth);
    setViewRect(0, 0, 0, m_width, m_height);
    
    m_initialized = true;
    
    const bgfx::RendererType::Enum rendererType = bgfx::getRendererType();
    std::cout << "Renderer initialized: " << bgfx::getRendererName(rendererType) << "\n";
    std::cout << "Resolution: " << m_width << "x" << m_height << "\n";
    
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    bgfx::shutdown();
    m_initialized = false;
    
    std::cout << "Renderer shut down\n";
}

void Renderer::beginFrame() {
    // Nothing needed here for bgfx
}

void Renderer::endFrame() {
    bgfx::frame();
}

void Renderer::setViewClear(bgfx::ViewId view, uint32_t color, float depth) {
    bgfx::setViewClear(view, 
                      BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 
                      color, 
                      depth, 
                      0);
}

void Renderer::setViewRect(bgfx::ViewId view, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    bgfx::setViewRect(view, x, y, width, height);
}

void Renderer::setViewTransform(bgfx::ViewId view, const float* viewMtx, const float* projMtx) {
    bgfx::setViewTransform(view, viewMtx, projMtx);
}

void Renderer::resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    
    m_width = width;
    m_height = height;
    
    uint32_t reset = BGFX_RESET_VSYNC;
    if (m_config.msaa) {
        reset |= BGFX_RESET_MSAA_X4;
    }
    
    bgfx::reset(m_width, m_height, reset);
    setViewRect(0, 0, 0, m_width, m_height);
}

void Renderer::debugText(uint16_t x, uint16_t y, uint8_t attr, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    bgfx::dbgTextPrintf(x, y, attr, "%s", buffer);
}

void Renderer::clearDebugText() {
    bgfx::dbgTextClear();
}

} // namespace render