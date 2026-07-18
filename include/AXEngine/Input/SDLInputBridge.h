#pragma once

#include <SDL3/SDL.h>

namespace ax::input {

// Temporary SDL adapter for current native samples. This is engine-owned now,
// so user scripts do not duplicate low-level SDL polling.
bool sdlKeyDown(SDL_Scancode scancode);
bool sdlMouseButtonDown(Uint32 buttonMask);
bool sdlMouseButtonPressed(Uint32 buttonMask, bool& previousState);

template <class PreviousKeyState>
inline bool sdlKeyPressed(SDL_Scancode scancode, PreviousKeyState& previous) {
    const bool now = sdlKeyDown(scancode);
    const bool wasDown = previous[scancode];
    previous[scancode] = now;
    return now && !wasDown;
}

} // namespace ax::input
