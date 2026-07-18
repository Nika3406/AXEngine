#include <iostream>
#include <SDL3/SDL_main.h>

#include "AXEngine.h"
#include "Game.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // User-provided factory — defined in scripts/
    Game* game = createGame();
    if (!game) {
        std::cerr << "[AXEngine] createGame() returned nullptr.\n";
        return 1;
    }

    Window       window;
    Renderer     renderer;
    AssetManager assets;
    Scene        scene;

    if (!window.create(game->getTitle(), game->getWidth(), game->getHeight())) return 1;
    if (!renderer.init(window.getNativeHandle())) return 1;

    window.setResizeCallback([&](int w, int h) {
        renderer.onResize(w, h);
        game->onResize(w, h);
    });

    if (!assets.initialize()) {
        std::cerr << "[AXEngine] AssetManager failed to initialize.\n";
        return 1;
    }

    game->_init(&scene, &renderer, &assets);
    game->onStart();
    scene.onStart();

    Uint64 lastTime = SDL_GetTicksNS() / 1000000;

    while (!window.shouldClose()) {
        Uint64 now = SDL_GetTicks();
        float  dt  = (now - lastTime) / 1000.0f;
        lastTime   = now;

        window.pollEvents();
        game->onUpdate(dt);
        game->_tickInput();
        scene.onUpdate(dt);
        renderer.renderScene(scene);
        game->onRender3D();
        // Do NOT draw skinned entities a second time after post-processing.
        // The old overlay pass bypassed the main environment shader and bloom pass,
        // making the player ignore fog/shadows/style and hiding sword bloom.
        // renderer.renderSceneOverlay(scene);

        renderer.beginUI();
        game->onUI();
        renderer.endUI();

        SDL_GL_SwapWindow(window.getNativeHandle());
    }

    assets.clear();
    delete game;
    return 0;
}
