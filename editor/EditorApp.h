#pragma once

#include "EditorState.h"

#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_process.h>

class EditorApp {
public:
    explicit EditorApp(std::filesystem::path projectRoot);
    ~EditorApp();

    bool initialize();
    void run();
    void shutdown();

private:
    void drawMainMenu();
    void drawDockspace();
    void drawPanels();
    void processGlobalShortcuts();
    void trackUndoableEdits();

    EditorSnapshot makeSnapshot() const;
    void restoreSnapshot(const EditorSnapshot& snapshot);
    std::string makeEditSignature() const;
    void pushUndoSnapshot(const EditorSnapshot& snapshot, const std::string& reason);
    void undo();
    void redo();
    void duplicateSelectedObject();
    void copySelectedObject();
    void pasteCopiedObject();
    void loadInitialSceneStub();
    void saveProjectData();
    void newAXScene();
    void openSelectedAXScene();
    void openAXSceneFile(const std::filesystem::path& path);
    void resetDefaultLayout();
    void generateCppOnly();
    void addSceneObject(const std::string& type);
    void deleteSelectedObject();
    void applyUiScale(float scale);
    void applyEditorTheme(const std::string& themeName);
    void applyReadableFont(float pixelSize);
    void refreshDisplayScale(bool force = false);
    void rebuildUiStyle(bool logChange = false);

    SDL_Process* launchProcess(const std::string& label, const std::vector<std::string>& args,
                               const std::filesystem::path& logFile);
    std::filesystem::path findGameExecutable() const;
    bool syncRuntimeAssets();
    void buildGameTargetDetached();
    void runGameDetached(bool buildFirst);
    void stopGameProcess();
    void pollChildProcesses();

private:
    EditorState state_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool running_ = false;
    SDL_Process* buildProcess_ = nullptr;
    SDL_Process* gameProcess_ = nullptr;
    bool runGameAfterBuild_ = false;
    std::string currentTheme_ = "AX Dark Pro";
};
