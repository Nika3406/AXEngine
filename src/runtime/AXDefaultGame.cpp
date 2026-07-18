#include "Game.h"
#include "runtime/AXProject.h"
#include "runtime/AXSceneRuntime.h"

#include <string>

// Built-in fallback app used only when scripts/ contains no user .cpp files.
// This keeps AXEngine bootable like Unreal/Godot with a brand-new empty project.
class AXDefaultGame final : public Game {
public:
    const char* getTitle() const override { return "AXEngine - Empty Project"; }
    int getWidth() const override { return 1280; }
    int getHeight() const override { return 720; }

    void onStart() override {
        // The empty project uses Renderer's compiled-in fallback shader. It must
        // not depend on any optional project shader or asset file.
        print("AXEngine empty runtime started.");

        AXProjectData project;
        std::string error;
        if (AXProject::load("project.axproject", project, &error)) {
            print("Loaded project: " + project.name);
            AXSceneRuntimeData scene;
            std::string sceneError;
            if (AXSceneRuntime::load(project.startupScene, scene, &sceneError)) {
                print("Startup scene: " + scene.name);
            } else {
                print("Startup scene not loaded: " + sceneError);
            }
        } else {
            print("No project.axproject found. Using built-in empty runtime.");
        }
    }

    void onUpdate(float) override {}
};

Game* createGame() {
    return new AXDefaultGame();
}
