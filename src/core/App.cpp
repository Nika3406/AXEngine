// src/core/App.cpp
#include "core/App.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <iostream>
#include <chrono>
#include <cstdint>

namespace core {

// ------------------------------------------------------------
// Helpers: X11 borderless + fullscreen state
// ------------------------------------------------------------

static void setWindowDecorations(Display* dpy, Window win, bool decorated) {
    Atom mwmHintsProperty = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    if (mwmHintsProperty == None) return;

    struct MotifWmHints {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long inputMode;
        unsigned long status;
    };

    MotifWmHints hints{};
    hints.flags = 1UL << 1;               // MWM_HINTS_DECORATIONS
    hints.decorations = decorated ? 1 : 0;

    XChangeProperty(dpy, win,
                    mwmHintsProperty, mwmHintsProperty,
                    32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&hints),
                    5);
}

// Request the WM to treat the window as fullscreen (_NET_WM_STATE_FULLSCREEN).
// Not strictly required for borderless fullscreen, but improves behavior on many WMs.
static void requestNetWmFullscreen(Display* dpy, Window win) {
    Atom wmState   = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom wmFull    = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    if (wmState == None || wmFull == None) return;

    XEvent xev{};
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;      // _NET_WM_STATE_ADD
    xev.xclient.data.l[1] = static_cast<long>(wmFull);
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    Window root = DefaultRootWindow(dpy);
    XSendEvent(dpy, root, False,
               SubstructureRedirectMask | SubstructureNotifyMask,
               &xev);
}

App::App(const Config& config)
    : m_config(config) {
}

App::~App() {
    shutdownWindow();
}

void App::run() {
    if (!initWindow()) {
        std::cerr << "Failed to initialize window\n";
        return;
    }

    // Initialize renderer
    render::Renderer::Config rendererConfig;
    rendererConfig.width  = m_config.width;
    rendererConfig.height = m_config.height;
    rendererConfig.vsync  = m_config.vsync;

    if (!m_renderer.init(rendererConfig, (void*)(uintptr_t)m_window, m_display)) {
        std::cerr << "Failed to initialize renderer\n";
        shutdownWindow();
        return;
    }

    m_initialized = true;
    m_running = true;

    // Initialize time tracking
    auto now = std::chrono::high_resolution_clock::now();
    m_lastFrameTime = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    // Default search roots (engine + user space)
    m_assets.clearSearchPaths();
    m_assets.addSearchPath(".");
    m_assets.addSearchPath("assets");
    m_assets.addSearchPath("scripts/assets");

    // Call user initialization
    onInit();

    // Main loop
    while (m_running) {
        processEvents();
        updateTime();
        updateInput();

        m_renderer.beginFrame();
        m_renderer.clearDebugText();

        // User update
        onUpdate(m_deltaTime);

        // User render
        onRender();

        // Display frame info
        if (m_deltaTime > 0.000001f) {
            m_renderer.debugText(0, 0, 0x0f, "Frame: %llu | FPS: %.1f | dt: %.3fms",
                                m_frameCount,
                                1.0f / m_deltaTime,
                                m_deltaTime * 1000.0f);
        }

        m_renderer.endFrame();
        m_frameCount++;
    }

    // Call user shutdown
    onShutdown();

    m_renderer.shutdown();
    shutdownWindow();
}

bool App::initWindow() {
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        std::cerr << "Failed to open X11 display\n";
        return false;
    }

    const int screen = DefaultScreen(m_display);
    Window root = RootWindow(m_display, screen);

    // If fullscreen: overwrite config size to match monitor size
    if (m_config.fullscreen) {
        m_config.width  = static_cast<uint32_t>(DisplayWidth(m_display, screen));
        m_config.height = static_cast<uint32_t>(DisplayHeight(m_display, screen));
    }

    XSetWindowAttributes swa{};
    swa.event_mask =
        ExposureMask |
        KeyPressMask | KeyReleaseMask |
        StructureNotifyMask |
        ButtonPressMask | ButtonReleaseMask;

    // Borderless fullscreen should start at 0,0
    int x = m_config.fullscreen ? 0 : 0;
    int y = m_config.fullscreen ? 0 : 0;

    m_window = XCreateWindow(
        m_display, root,
        x, y,
        m_config.width, m_config.height,
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWEventMask,
        &swa
    );

    if (!m_window) {
        std::cerr << "Failed to create X11 window\n";
        XCloseDisplay(m_display);
        m_display = nullptr;
        return false;
    }

    XStoreName(m_display, m_window, m_config.title.c_str());

    // Handle window close button
    Atom wmDelete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(m_display, m_window, &wmDelete, 1);

    // Apply borderless fullscreen behavior
    if (m_config.fullscreen) {
        setWindowDecorations(m_display, m_window, false);
        XMoveResizeWindow(m_display, m_window, 0, 0, m_config.width, m_config.height);
    }

    XMapWindow(m_display, m_window);
    XFlush(m_display);

    // Ask WM to treat it as fullscreen (helps on many WMs)
    if (m_config.fullscreen) {
        requestNetWmFullscreen(m_display, m_window);
        XFlush(m_display);
    }

    std::cout << "Window created: " << m_config.title << " ("
              << m_config.width << "x" << m_config.height << ")"
              << (m_config.fullscreen ? " [Borderless Fullscreen]" : " [Windowed]")
              << "\n";

    return true;
}

void App::shutdownWindow() {
    if (m_window && m_display) {
        XDestroyWindow(m_display, m_window);
        m_window = 0;
    }

    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
}

void App::processEvents() {
    if (!m_display) return;

    while (XPending(m_display)) {
        XEvent event;
        XNextEvent(m_display, &event);

        switch (event.type) {
            case ClientMessage: {
                Atom wmDelete = XInternAtom(m_display, "WM_DELETE_WINDOW", False);
                if ((Atom)event.xclient.data.l[0] == wmDelete) {
                    m_running = false;
                }
                break;
            }

            case ConfigureNotify: {
                uint32_t width  = static_cast<uint32_t>(event.xconfigure.width);
                uint32_t height = static_cast<uint32_t>(event.xconfigure.height);

                if (width != m_renderer.getWidth() || height != m_renderer.getHeight()) {
                    m_renderer.resize(width, height);
                    onResize(width, height);
                }
                break;
            }

            case KeyPress: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                int keyCode = static_cast<int>(key);
                
                if (keyCode < 256) {
                    m_keyStates[keyCode] = true;
                }
                
                onKeyPress(keyCode);

                // ESC to quit
                if (key == XK_Escape) {
                    m_running = false;
                }
                break;
            }

            case KeyRelease: {
                // Handle key repeat (X11 sends Press+Release for repeated keys)
                if (XEventsQueued(m_display, QueuedAfterReading)) {
                    XEvent nextEvent;
                    XPeekEvent(m_display, &nextEvent);
                    
                    if (nextEvent.type == KeyPress &&
                        nextEvent.xkey.time == event.xkey.time &&
                        nextEvent.xkey.keycode == event.xkey.keycode) {
                        // This is a key repeat, ignore the release
                        break;
                    }
                }

                KeySym key = XLookupKeysym(&event.xkey, 0);
                int keyCode = static_cast<int>(key);
                
                if (keyCode < 256) {
                    m_keyStates[keyCode] = false;
                }
                
                onKeyRelease(keyCode);
                break;
            }

            case MotionNotify: {
                m_mouseX = event.xmotion.x;
                m_mouseY = event.xmotion.y;
                break;
            }

            case ButtonPress: {
                onMouseButton(event.xbutton.button, true);
                break;
            }

            case ButtonRelease: {
                onMouseButton(event.xbutton.button, false);
                break;
            }

            default:
                break;
        }
    }
}

void App::updateTime() {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    m_deltaTime = (currentTime - m_lastFrameTime) / 1000000.0f; // seconds
    m_totalTime += m_deltaTime;
    m_lastFrameTime = currentTime;

    // Clamp delta time to prevent huge jumps
    if (m_deltaTime > 0.1f) {
        m_deltaTime = 0.1f;
    }
}

void App::updateInput() {
    if (!m_display || !m_window) return;

    if (m_mouseGrabbed) {
        // Use actual renderer dimensions, not config (important for fullscreen!)
        int centerX = m_renderer.getWidth() / 2;
        int centerY = m_renderer.getHeight() / 2;

        // Query actual pointer position (more reliable than events)
        Window root_return, child_return;
        int root_x, root_y;
        int win_x, win_y;
        unsigned int mask_return;
        
        XQueryPointer(m_display, m_window, &root_return, &child_return,
                     &root_x, &root_y, &win_x, &win_y, &mask_return);

        // Calculate mouse delta from center
        if (m_mouseInitialized) {
            m_mouseDx = win_x - centerX;
            m_mouseDy = win_y - centerY;
        } else {
            m_mouseDx = 0;
            m_mouseDy = 0;
            m_mouseInitialized = true;
        }

        // Update stored position
        m_mouseX = win_x;
        m_mouseY = win_y;

        // Call user callback with delta
        if (m_mouseDx != 0 || m_mouseDy != 0) {
            onMouseMove(m_mouseX, m_mouseY, m_mouseDx, m_mouseDy);
        }

        // Warp back to center
        XWarpPointer(m_display, None, m_window, 0, 0, 0, 0, centerX, centerY);
        XFlush(m_display);
        
        // Drain motion events
        XEvent event;
        while (XCheckTypedWindowEvent(m_display, m_window, MotionNotify, &event)) {
            // Discard
        }
    } else {
        m_mouseDx = 0;
        m_mouseDy = 0;
    }
}

bool App::isKeyDown(int key) const {
    if (key < 0 || key >= 256) return false;
    return m_keyStates[key];
}

void App::setMouseGrabbed(bool grabbed) {
    if (!m_display || !m_window) return;
    
    if (grabbed && !m_mouseGrabbed) {
        // Grab mouse
        XGrabPointer(m_display, m_window, True,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, m_window, None, CurrentTime);
        
        // Hide cursor
        Cursor invisibleCursor;
        Pixmap bitmapNoData;
        XColor black;
        static char noData[] = { 0,0,0,0,0,0,0,0 };
        black.red = black.green = black.blue = 0;

        bitmapNoData = XCreateBitmapFromData(m_display, m_window, noData, 8, 8);
        invisibleCursor = XCreatePixmapCursor(m_display, bitmapNoData, bitmapNoData, 
                                             &black, &black, 0, 0);
        XDefineCursor(m_display, m_window, invisibleCursor);
        XFreeCursor(m_display, invisibleCursor);
        XFreePixmap(m_display, bitmapNoData);

        // Center mouse (use actual renderer dimensions!)
        int centerX = m_renderer.getWidth() / 2;
        int centerY = m_renderer.getHeight() / 2;
        
        XWarpPointer(m_display, None, m_window, 0, 0, 0, 0, centerX, centerY);
        XSync(m_display, False);
        
        // Drain warp events
        XEvent event;
        while (XCheckTypedWindowEvent(m_display, m_window, MotionNotify, &event)) {
            // Discard
        }
        
        m_mouseX = centerX;
        m_mouseY = centerY;
        m_mouseInitialized = false;
        
    } else if (!grabbed && m_mouseGrabbed) {
        // Ungrab mouse
        XUngrabPointer(m_display, CurrentTime);
        XUndefineCursor(m_display, m_window);
        XFlush(m_display);
    }
    
    m_mouseGrabbed = grabbed;
}

} // namespace core