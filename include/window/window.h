#pragma once

#include <string>
#include <functional>
#include <SDL3/SDL.h>

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(const std::string& title, int width, int height);
    void destroy();

    void pollEvents();
    bool shouldClose() const;

    int getWidth() const  { return width_; }
    int getHeight() const { return height_; }

    // Called whenever the window is resized — renderer hooks into this
    void setResizeCallback(std::function<void(int, int)> cb) { resizeCallback_ = std::move(cb); }

    SDL_Window* getNativeHandle() const;

private:
    SDL_Window* window_   = nullptr;
    bool shouldClose_     = false;
    int width_            = 0;
    int height_           = 0;

    std::function<void(int, int)> resizeCallback_;
};
