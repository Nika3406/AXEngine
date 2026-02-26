// include/core/App.hpp
#pragma once

#include "render/Renderer.hpp"
#include "assets/AssetSystem.hpp"
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <cstdint>

// Forward declarations for X11
typedef struct _XDisplay Display;
typedef unsigned long Window;

namespace core {

// ============================================================================
// Application Base Class
// ============================================================================
class App {
public:
    struct Config {
        std::string title;
        uint32_t width;
        uint32_t height;
        bool vsync;

        // If true: borderless fullscreen (fullscreen windowed)
        bool fullscreen;

        Config(const std::string& t = "Game Engine",
               uint32_t w = 1280,
               uint32_t h = 720,
               bool v = true,
               bool f = false)
            : title(t), width(w), height(h), vsync(v), fullscreen(f) {}
    };

    App(const Config& config = Config());
    virtual ~App();

    // Main loop
    void run();

    // Override these in derived class
    virtual void onInit() {}
    virtual void onUpdate(float deltaTime) {}
    virtual void onRender() {}
    virtual void onShutdown() {}
    virtual void onResize(uint32_t width, uint32_t height) {}
    virtual void onKeyPress(int key) {}
    virtual void onKeyRelease(int key) {}
    virtual void onMouseMove(int x, int y, int dx, int dy) {}
    virtual void onMouseButton(int button, bool pressed) {}

protected:
    // Access to engine systems
    render::Renderer& getRenderer() { return m_renderer; }
    assets::AssetSystem& getAssets() { return m_assets; }
    entt::registry& getRegistry() { return m_registry; }

    // Input queries
    bool isKeyDown(int key) const;
    void getMouseDelta(int& outDx, int& outDy) const { outDx = m_mouseDx; outDy = m_mouseDy; }
    void getMousePosition(int& outX, int& outY) const { outX = m_mouseX; outY = m_mouseY; }
    void setMouseGrabbed(bool grabbed);
    bool isMouseGrabbed() const { return m_mouseGrabbed; }

    // Window control
    void quit() { m_running = false; }
    bool isRunning() const { return m_running; }

    // Time
    float getDeltaTime() const { return m_deltaTime; }
    float getTotalTime() const { return m_totalTime; }
    uint64_t getFrameCount() const { return m_frameCount; }

private:
    bool initWindow();
    void shutdownWindow();
    void processEvents();
    void updateTime();
    void updateInput();

private:
    Config m_config;
    bool m_running = false;
    bool m_initialized = false;

    // X11 window handles
    Display* m_display = nullptr;
    Window m_window = 0;

    // Engine systems
    render::Renderer m_renderer;
    assets::AssetSystem m_assets;
    entt::registry m_registry;

    // Time tracking
    float m_deltaTime = 0.0f;
    float m_totalTime = 0.0f;
    uint64_t m_frameCount = 0;
    uint64_t m_lastFrameTime = 0;

    // Input state
    bool m_keyStates[256] = {};
    int m_mouseX = 0;
    int m_mouseY = 0;
    int m_mouseDx = 0;
    int m_mouseDy = 0;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    bool m_mouseGrabbed = false;
    bool m_mouseInitialized = false;
};

} // namespace core