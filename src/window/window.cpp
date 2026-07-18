#include "window/window.h"
#include <iostream>

Window::~Window() {
    destroy();
}

bool Window::create(const std::string& title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
                               width,
                               height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return false;
    }

    width_       = width;
    height_      = height;
    shouldClose_ = false;
    return true;
}

void Window::destroy() {
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                shouldClose_ = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                width_  = event.window.data1;
                height_ = event.window.data2;
                if (resizeCallback_) {
                    resizeCallback_(width_, height_);
                }
                break;

                // SDL3 on some platforms fires PIXEL_SIZE_CHANGED for HiDPI —
                // handle it the same way so the viewport stays correct on Retina/HiDPI
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                width_  = event.window.data1;
                height_ = event.window.data2;
                if (resizeCallback_) {
                    resizeCallback_(width_, height_);
                }
                break;

            default:
                break;
        }
    }
}

bool Window::shouldClose() const { return shouldClose_; }
SDL_Window* Window::getNativeHandle() const { return window_; }
