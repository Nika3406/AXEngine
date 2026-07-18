#include "AXEngine/Input/SDLInputBridge.h"

namespace ax::input {

bool sdlKeyDown(SDL_Scancode scancode) {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    return keys && keys[scancode];
}

bool sdlMouseButtonDown(Uint32 buttonMask) {
    float mx = 0.0f;
    float my = 0.0f;
    const Uint32 buttons = SDL_GetMouseState(&mx, &my);
    return (buttons & buttonMask) != 0;
}

bool sdlMouseButtonPressed(Uint32 buttonMask, bool& previousState) {
    const bool now = sdlMouseButtonDown(buttonMask);
    const bool pressed = now && !previousState;
    previousState = now;
    return pressed;
}

} // namespace ax::input
