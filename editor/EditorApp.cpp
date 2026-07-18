#include "EditorApp.h"

#include "panels/ProjectBrowserPanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/SceneViewPanel.h"
#include "panels/AssetPreviewPanel.h"
#include "panels/ConsolePanel.h"
#include "panels/AttackColliderPanel.h"
#include "panels/CommandLauncherPanel.h"
#include "panels/RenderMaterialPanel.h"
#include "panels/AnimationTimelinePanel.h"
#include "panels/VFXEditorPanel.h"
#include "panels/InputMapPanel.h"
#include "panels/AIEditorPanel.h"
#include "panels/GameUIPanel.h"
#include "panels/AudioPreviewPanel.h"
#include "panels/DebugProfilerPanel.h"
#include "panels/AssetRegistryPanel.h"
#include "panels/PanelLayout.h"
#include "codegen/CppGenerator.h"
#include "codegen/JsonProjectIO.h"
#include "scene/AXSceneIO.h"

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>

namespace {
void applyAxEditorTheme(const std::string& themeName, EditorState& state, bool logChange = true) {
    ImGuiStyle& style = ImGui::GetStyle();
    if (themeName == "Classic ImGui") {
        ImGui::StyleColorsClassic();
    } else if (themeName == "Light Studio") {
        ImGui::StyleColorsLight();
        style.WindowRounding = 2.0f;
        style.FrameRounding = 2.0f;
        style.GrabRounding = 2.0f;
        style.TabRounding = 2.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.Colors[ImGuiCol_Header] = ImVec4(0.68f, 0.82f, 1.00f, 0.65f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.52f, 0.74f, 1.00f, 0.90f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.78f, 0.88f, 1.00f, 0.65f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.58f, 0.76f, 1.00f, 0.95f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.60f, 0.95f, 1.00f);
    } else {
        ImGui::StyleColorsDark();
        style.WindowRounding = 3.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.ItemSpacing = ImVec2(8.0f, 5.0f);
        style.FramePadding = ImVec2(6.0f, 4.0f);
        auto& c = style.Colors;
        if (themeName == "AX Midnight Blue") {
            c[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.050f, 0.080f, 1.0f);
            c[ImGuiCol_ChildBg] = ImVec4(0.045f, 0.065f, 0.105f, 1.0f);
            c[ImGuiCol_TitleBgActive] = ImVec4(0.060f, 0.140f, 0.260f, 1.0f);
            c[ImGuiCol_Header] = ImVec4(0.100f, 0.220f, 0.390f, 0.85f);
            c[ImGuiCol_Button] = ImVec4(0.110f, 0.250f, 0.430f, 0.85f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.160f, 0.360f, 0.610f, 1.0f);
            c[ImGuiCol_FrameBg] = ImVec4(0.060f, 0.085f, 0.125f, 1.0f);
        } else if (themeName == "AX Warm Graphite") {
            c[ImGuiCol_WindowBg] = ImVec4(0.080f, 0.075f, 0.070f, 1.0f);
            c[ImGuiCol_ChildBg] = ImVec4(0.105f, 0.098f, 0.090f, 1.0f);
            c[ImGuiCol_TitleBgActive] = ImVec4(0.220f, 0.160f, 0.100f, 1.0f);
            c[ImGuiCol_Header] = ImVec4(0.290f, 0.210f, 0.130f, 0.85f);
            c[ImGuiCol_Button] = ImVec4(0.330f, 0.240f, 0.150f, 0.85f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.520f, 0.370f, 0.210f, 1.0f);
            c[ImGuiCol_FrameBg] = ImVec4(0.135f, 0.125f, 0.115f, 1.0f);
        } else if (themeName == "AX DMC Crimson") {
            c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.045f, 0.050f, 1.0f);
            c[ImGuiCol_ChildBg] = ImVec4(0.075f, 0.055f, 0.060f, 1.0f);
            c[ImGuiCol_TitleBgActive] = ImVec4(0.250f, 0.045f, 0.060f, 1.0f);
            c[ImGuiCol_Header] = ImVec4(0.330f, 0.060f, 0.080f, 0.85f);
            c[ImGuiCol_Button] = ImVec4(0.360f, 0.070f, 0.090f, 0.90f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.600f, 0.100f, 0.130f, 1.0f);
            c[ImGuiCol_FrameBg] = ImVec4(0.120f, 0.075f, 0.080f, 1.0f);
        } else if (themeName == "High Contrast Dark") {
            c[ImGuiCol_WindowBg] = ImVec4(0.000f, 0.000f, 0.000f, 1.0f);
            c[ImGuiCol_ChildBg] = ImVec4(0.020f, 0.020f, 0.020f, 1.0f);
            c[ImGuiCol_Text] = ImVec4(1.000f, 1.000f, 1.000f, 1.0f);
            c[ImGuiCol_TextDisabled] = ImVec4(0.760f, 0.760f, 0.760f, 1.0f);
            c[ImGuiCol_Border] = ImVec4(0.800f, 0.800f, 0.800f, 0.9f);
            c[ImGuiCol_Header] = ImVec4(0.000f, 0.250f, 0.900f, 0.95f);
            c[ImGuiCol_Button] = ImVec4(0.000f, 0.300f, 0.900f, 0.95f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.100f, 0.500f, 1.000f, 1.0f);
            c[ImGuiCol_FrameBg] = ImVec4(0.060f, 0.060f, 0.060f, 1.0f);
        } else { // AX Dark Pro
            c[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.080f, 0.090f, 1.0f);
            c[ImGuiCol_ChildBg] = ImVec4(0.090f, 0.095f, 0.110f, 1.0f);
            c[ImGuiCol_TitleBgActive] = ImVec4(0.120f, 0.160f, 0.220f, 1.0f);
            c[ImGuiCol_Header] = ImVec4(0.160f, 0.220f, 0.320f, 0.90f);
            c[ImGuiCol_Button] = ImVec4(0.150f, 0.230f, 0.360f, 0.90f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.220f, 0.350f, 0.550f, 1.0f);
            c[ImGuiCol_FrameBg] = ImVec4(0.115f, 0.125f, 0.145f, 1.0f);
        }
        c[ImGuiCol_HeaderHovered] = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_Tab] = c[ImGuiCol_Button];
        c[ImGuiCol_TabHovered] = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_TabSelected] = c[ImGuiCol_Header];
        c[ImGuiCol_CheckMark] = ImVec4(0.450f, 0.750f, 1.000f, 1.0f);
        c[ImGuiCol_SliderGrab] = ImVec4(0.450f, 0.750f, 1.000f, 1.0f);
    }
    if (logChange) state.log("UI theme: " + themeName);
}

}

EditorApp::EditorApp(std::filesystem::path projectRoot) {
    state_.projectRoot = std::move(projectRoot);
}

EditorApp::~EditorApp() {
    shutdown();
}

bool EditorApp::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "[AXEditor] SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Rect usableBounds{};
    const SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    if (primaryDisplay != 0 && SDL_GetDisplayUsableBounds(primaryDisplay, &usableBounds)) {
        // Never request a window larger than the compositor's usable area.
        // The lower bounds are preferences, not hard requirements, so AXEditor
        // still starts correctly on compact virtual machines and small displays.
        const int targetWidth = std::max(720, static_cast<int>(usableBounds.w * 0.90f));
        const int targetHeight = std::max(480, static_cast<int>(usableBounds.h * 0.86f));
        state_.windowWidth = std::min({state_.windowWidth, targetWidth, usableBounds.w});
        state_.windowHeight = std::min({state_.windowHeight, targetHeight, usableBounds.h});
    }

    window_ = SDL_CreateWindow(
        "AXEditor",
        state_.windowWidth,
        state_.windowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (!window_) {
        std::cerr << "[AXEditor] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_SetWindowMinimumSize(window_, 720, 480);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
        std::cerr << "[AXEditor] SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        std::cerr << "[AXEditor] gladLoadGLLoader failed.\n";
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    state_.displayScale = std::max(0.5f, SDL_GetWindowDisplayScale(window_));
    applyEditorTheme("AX Dark Pro");
    applyReadableFont(16.0f);
    ImGui_ImplSDL3_InitForOpenGL(window_, glContext_);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    loadInitialSceneStub();
    state_.log("AXEditor initialized at: " + state_.projectRoot.string());
    state_.log("Adaptive workspace active: one document, one right pane, and one bottom pane always fit the current display.");
    state_.log("Scene Preview edits scene objects; Asset Preview edits one selected asset/rig/mesh target.");
    state_.log("Stage 1.2 previews are interactive: click/drag objects or bones, wheel zooms, middle/right drag pans.");
    state_.log("Shortcuts: Ctrl+Z undo, Ctrl+Y/Ctrl+Shift+Z redo, Ctrl+S save, Delete deletes, Ctrl+D duplicates, Ctrl+C/V copies/pastes objects.");
    state_.lastCommittedSnapshot = makeSnapshot();
    state_.lastCommittedEditSignature = makeEditSignature();
    running_ = true;
    return true;
}

void EditorApp::run() {
    while (running_) {
        pollChildProcesses();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running_ = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window_)) {
                running_ = false;
            }
            if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED &&
                event.window.windowID == SDL_GetWindowID(window_)) {
                refreshDisplayScale(true);
            }
        }

        SDL_GetWindowSize(window_, &state_.windowWidth, &state_.windowHeight);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        processGlobalShortcuts();
        drawMainMenu();
        drawDockspace();
        drawPanels();
        trackUndoableEdits();

        ImGui::Render();
        int w = 0;
        int h = 0;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.085f, 0.095f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }
}

void EditorApp::shutdown() {
    if (gameProcess_) {
        SDL_KillProcess(gameProcess_, false);
        SDL_DestroyProcess(gameProcess_);
        gameProcess_ = nullptr;
    }
    if (buildProcess_) {
        SDL_KillProcess(buildProcess_, false);
        SDL_DestroyProcess(buildProcess_);
        buildProcess_ = nullptr;
    }

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    if (glContext_) {
        SDL_GL_DestroyContext(glContext_);
        glContext_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void EditorApp::drawMainMenu() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New AXScene")) newAXScene();
        if (ImGui::MenuItem("Open Selected .axscene")) openSelectedAXScene();
        if (ImGui::MenuItem("Save AXScene + Generate C++", "Ctrl+S")) saveProjectData();
        if (ImGui::MenuItem("Generate C++ Only")) generateCppOnly();
        ImGui::Separator();
        ImGui::TextDisabled("Current scene:");
        ImGui::TextWrapped("%s%s", state_.currentScenePath.string().c_str(), state_.sceneDirty ? " *" : "");
        ImGui::Separator();
        if (ImGui::MenuItem("Reveal Project Root")) state_.log("Project root: " + state_.projectRoot.string());
        if (ImGui::MenuItem("Exit")) running_ = false;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !state_.undoStack.empty())) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z", false, !state_.redoStack.empty())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Selected Object", "Ctrl+C", false, state_.selectedObject >= 0)) copySelectedObject();
        if (ImGui::MenuItem("Paste Object", "Ctrl+V", false, state_.copiedObject >= 0)) pasteCopiedObject();
        if (ImGui::MenuItem("Duplicate Selected Object", "Ctrl+D", false, state_.selectedObject >= 0)) duplicateSelectedObject();
        if (ImGui::MenuItem("Delete Selected Object", "Delete", false, state_.selectedObject >= 0)) deleteSelectedObject();
        ImGui::Separator();
        ImGui::TextDisabled("Undo steps: %d / %d", (int)state_.undoStack.size(), state_.maxUndoSteps);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Scene Preview", nullptr, &state_.showSceneView) && state_.showSceneView)
            state_.workspaceDocument = EditorWorkspaceDocument::Scene;
        if (ImGui::MenuItem("Asset Preview", nullptr, &state_.showAssetPreview) && state_.showAssetPreview)
            state_.workspaceDocument = EditorWorkspaceDocument::Asset;
        if (ImGui::MenuItem("Project Browser", nullptr, &state_.showProjectBrowser) && state_.showProjectBrowser)
            state_.workspaceBottomPane = EditorWorkspaceBottomPane::ProjectBrowser;
        if (ImGui::MenuItem("Hierarchy", nullptr, &state_.showHierarchy) && state_.showHierarchy)
            state_.workspaceRightPane = EditorWorkspaceRightPane::Hierarchy;
        if (ImGui::MenuItem("Inspector", nullptr, &state_.showInspector) && state_.showInspector)
            state_.workspaceRightPane = EditorWorkspaceRightPane::Inspector;
        if (ImGui::MenuItem("Console", nullptr, &state_.showConsole) && state_.showConsole)
            state_.workspaceBottomPane = EditorWorkspaceBottomPane::Console;
        if (ImGui::MenuItem("Attack Collider Editor", nullptr, &state_.showAttackColliderEditor) && state_.showAttackColliderEditor)
            state_.workspaceDocument = EditorWorkspaceDocument::Attack;
        ImGui::Separator();
        ImGui::MenuItem("Command Search", "Ctrl+Space", &state_.showCommandLauncher);
        ImGui::MenuItem("Render / Material Editor", nullptr, &state_.showRenderMaterialEditor);
        ImGui::MenuItem("Animation Timeline", nullptr, &state_.showAnimationTimeline);
        ImGui::MenuItem("VFX Editor", nullptr, &state_.showVFXEditor);
        ImGui::MenuItem("Input Map Editor", nullptr, &state_.showInputMapEditor);
        ImGui::MenuItem("AI Editor", nullptr, &state_.showAIEditor);
        ImGui::MenuItem("Game UI Editor", nullptr, &state_.showGameUIEditor);
        ImGui::MenuItem("Audio Preview", nullptr, &state_.showAudioPreview);
        ImGui::MenuItem("Debug / Profiler", nullptr, &state_.showDebugProfiler);
        ImGui::MenuItem("Asset Registry", nullptr, &state_.showAssetRegistry);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Default Layout")) resetDefaultLayout();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Build")) {
        if (ImGui::MenuItem("Generate C++ Registries")) generateCppOnly();
        if (ImGui::MenuItem("Save + Generate")) saveProjectData();
        ImGui::Separator();
        if (ImGui::MenuItem("Build Game Target", nullptr, false, buildProcess_ == nullptr)) buildGameTargetDetached();
        if (ImGui::MenuItem("Run Game Detached", nullptr, false, gameProcess_ == nullptr)) runGameDetached(false);
        if (ImGui::MenuItem("Build + Run Game Detached", nullptr, false, gameProcess_ == nullptr && buildProcess_ == nullptr)) runGameDetached(true);
        if (ImGui::MenuItem("Stop Running Game", nullptr, false, gameProcess_ != nullptr)) stopGameProcess();
        ImGui::Separator();
        ImGui::TextDisabled("Game log: build/AXGame.log");
        ImGui::TextDisabled("Build log: build/AXEditorBuild.log");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Settings")) {
        ImGui::MenuItem("Generate C++ On Save", nullptr, &state_.generateCppOnSave);
        ImGui::InputText("Scene Name", &state_.sceneName);
        ImGui::InputText("Render Profile", &state_.sceneRenderProfile);
        ImGui::DragFloat("Gravity", &state_.sceneGravity, 0.1f, -100.0f, 100.0f);
        ImGui::DragFloat("Unit Scale", &state_.sceneUnitScale, 0.01f, 0.001f, 100.0f);
        ImGui::MenuItem("Show Demo Scene", nullptr, &state_.showDemoScene);
        ImGui::MenuItem("Preview Grid", nullptr, &state_.showGrid);
        ImGui::MenuItem("Scene Objects", nullptr, &state_.sceneShowObjects);
        ImGui::MenuItem("Scene Gizmos", nullptr, &state_.sceneShowGizmos);
        ImGui::MenuItem("Asset Mesh", nullptr, &state_.assetShowMesh);
        ImGui::MenuItem("Asset Skeleton", nullptr, &state_.assetShowSkeleton);
        ImGui::MenuItem("Asset Sockets", nullptr, &state_.assetShowSockets);
        ImGui::Separator();
        ImGui::MenuItem("Snap Transform", nullptr, &state_.snapEnabled);
        ImGui::DragFloat("Snap Step", &state_.snapStep, 0.01f, 0.01f, 10.0f);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("UI")) {
        if (ImGui::BeginMenu("Theme Template")) {
            if (ImGui::MenuItem("AX Dark Pro")) applyEditorTheme("AX Dark Pro");
            if (ImGui::MenuItem("AX Midnight Blue")) applyEditorTheme("AX Midnight Blue");
            if (ImGui::MenuItem("AX Warm Graphite")) applyEditorTheme("AX Warm Graphite");
            if (ImGui::MenuItem("AX DMC Crimson")) applyEditorTheme("AX DMC Crimson");
            ImGui::Separator();
            if (ImGui::MenuItem("Light Studio")) applyEditorTheme("Light Studio");
            if (ImGui::MenuItem("High Contrast Dark")) applyEditorTheme("High Contrast Dark");
            if (ImGui::MenuItem("Classic ImGui")) applyEditorTheme("Classic ImGui");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Font / Readability")) {
            if (ImGui::MenuItem("Readable 14 px")) applyReadableFont(14.0f);
            if (ImGui::MenuItem("Readable 16 px Recommended")) applyReadableFont(16.0f);
            if (ImGui::MenuItem("Readable 18 px")) applyReadableFont(18.0f);
            if (ImGui::MenuItem("Readable 20 px")) applyReadableFont(20.0f);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("UI Scale")) {
            ImGui::TextDisabled("Display scale: %.2fx", state_.displayScale);
            if (ImGui::Checkbox("Follow operating-system scale", &state_.followDisplayScale)) rebuildUiStyle(true);
            ImGui::Separator();
            if (ImGui::MenuItem("80%", nullptr, std::abs(state_.uiScale - 0.80f) < 0.01f)) applyUiScale(0.80f);
            if (ImGui::MenuItem("90%", nullptr, std::abs(state_.uiScale - 0.90f) < 0.01f)) applyUiScale(0.90f);
            if (ImGui::MenuItem("100%", nullptr, std::abs(state_.uiScale - 1.00f) < 0.01f)) applyUiScale(1.00f);
            if (ImGui::MenuItem("110%", nullptr, std::abs(state_.uiScale - 1.10f) < 0.01f)) applyUiScale(1.10f);
            if (ImGui::MenuItem("125%", nullptr, std::abs(state_.uiScale - 1.25f) < 0.01f)) applyUiScale(1.25f);
            if (ImGui::MenuItem("150%", nullptr, std::abs(state_.uiScale - 1.50f) < 0.01f)) applyUiScale(1.50f);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Workspace Layout")) {
            ImGui::Checkbox("Adaptive workspace", &state_.adaptiveWorkspace);
            ImGui::TextDisabled("Adaptive mode prevents panel overlap on small and HiDPI displays.");
            ImGui::Separator();
            if (ImGui::MenuItem("Auto", nullptr, state_.workspaceLayoutMode == EditorWorkspaceLayoutMode::Auto)) state_.workspaceLayoutMode = EditorWorkspaceLayoutMode::Auto;
            if (ImGui::MenuItem("Compact", nullptr, state_.workspaceLayoutMode == EditorWorkspaceLayoutMode::Compact)) state_.workspaceLayoutMode = EditorWorkspaceLayoutMode::Compact;
            if (ImGui::MenuItem("Standard", nullptr, state_.workspaceLayoutMode == EditorWorkspaceLayoutMode::Standard)) state_.workspaceLayoutMode = EditorWorkspaceLayoutMode::Standard;
            if (ImGui::MenuItem("Wide", nullptr, state_.workspaceLayoutMode == EditorWorkspaceLayoutMode::Wide)) state_.workspaceLayoutMode = EditorWorkspaceLayoutMode::Wide;
            ImGui::Separator();
            ImGui::Checkbox("Lock manual layout", &state_.layoutLocked);
            ImGui::DragFloat("Panel gap", &state_.layoutGap, 0.25f, 3.0f, 18.0f, "%.1f");
            if (ImGui::MenuItem("Reset Workspace")) resetDefaultLayout();
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        ImGui::TextUnformatted("AXEditor Stage 1");
        ImGui::Separator();
        ImGui::BulletText("Scene Preview: edit scene objects and transforms.");
        ImGui::BulletText("Attack Collider Editor: author big attack volumes for hack-and-slash moves.");
        ImGui::BulletText("Asset Preview: inspect/edit one mesh, GLB, rig, bones, sockets.");
        ImGui::BulletText("Generated C++ stays readable in generated/.");
        ImGui::BulletText("Shortcuts: Ctrl+Z undo, Ctrl+Y/Ctrl+Shift+Z redo, Ctrl+S save.");
        ImGui::BulletText("Delete deletes selected object; Ctrl+D duplicates; Ctrl+C/V copy-pastes.");
        ImGui::BulletText("Adaptive workspace tabs keep the editor usable without GPU-vendor or OS-specific layout code.");
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void EditorApp::drawDockspace() {
    if (!state_.adaptiveWorkspace) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;

    const float height = AXEditorLayout::toolbarHeight(state_);
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("##AXAdaptiveWorkspaceToolbar", nullptr, flags);
    ImGui::PopStyleVar(3);

    auto tabButton = [](const char* label, bool selected) {
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        const bool clicked = ImGui::Button(label);
        if (selected) ImGui::PopStyleColor();
        return clicked;
    };

    const bool compactToolbar = AXEditorLayout::resolvedLayoutMode(state_) == EditorWorkspaceLayoutMode::Compact ||
        viewport->WorkSize.x < 1250.0f * AXEditorLayout::effectiveUiScale(state_);

    if (!compactToolbar) { ImGui::TextDisabled("DOCUMENT"); ImGui::SameLine(); }
    if (tabButton("Scene", state_.workspaceDocument == EditorWorkspaceDocument::Scene)) {
        state_.workspaceDocument = EditorWorkspaceDocument::Scene;
        state_.showSceneView = true;
    }
    ImGui::SameLine();
    if (tabButton("Asset", state_.workspaceDocument == EditorWorkspaceDocument::Asset)) {
        state_.workspaceDocument = EditorWorkspaceDocument::Asset;
        state_.showAssetPreview = true;
    }
    ImGui::SameLine();
    if (tabButton("Attack", state_.workspaceDocument == EditorWorkspaceDocument::Attack)) {
        state_.workspaceDocument = EditorWorkspaceDocument::Attack;
        state_.showAttackColliderEditor = true;
    }

    if (compactToolbar) {
        ImGui::NewLine();
    } else {
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::TextDisabled("RIGHT");
        ImGui::SameLine();
    }
    if (tabButton("Hierarchy", state_.workspaceRightPane == EditorWorkspaceRightPane::Hierarchy)) {
        state_.workspaceRightPane = EditorWorkspaceRightPane::Hierarchy;
        state_.showHierarchy = true;
    }
    ImGui::SameLine();
    if (tabButton("Inspector", state_.workspaceRightPane == EditorWorkspaceRightPane::Inspector)) {
        state_.workspaceRightPane = EditorWorkspaceRightPane::Inspector;
        state_.showInspector = true;
    }

    ImGui::SameLine(0.0f, compactToolbar ? 12.0f : 20.0f);
    if (!compactToolbar) { ImGui::TextDisabled("BOTTOM"); ImGui::SameLine(); }
    if (tabButton("Project", state_.workspaceBottomPane == EditorWorkspaceBottomPane::ProjectBrowser)) {
        state_.workspaceBottomPane = EditorWorkspaceBottomPane::ProjectBrowser;
        state_.showProjectBrowser = true;
    }
    ImGui::SameLine();
    if (tabButton("Console", state_.workspaceBottomPane == EditorWorkspaceBottomPane::Console)) {
        state_.workspaceBottomPane = EditorWorkspaceBottomPane::Console;
        state_.showConsole = true;
    }

    const auto mode = AXEditorLayout::resolvedLayoutMode(state_);
    const char* modeName = mode == EditorWorkspaceLayoutMode::Compact ? "Compact" :
                           mode == EditorWorkspaceLayoutMode::Wide ? "Wide" : "Standard";
    const std::string status = std::string(modeName) + "  |  " + AX_PLATFORM_NAME +
        "  |  UI " + std::to_string((int)std::lround(state_.uiScale * 100.0f)) + "%";
    const float statusWidth = ImGui::CalcTextSize(status.c_str()).x;
    if (!compactToolbar && ImGui::GetContentRegionAvail().x > statusWidth + 16.0f) {
        ImGui::SameLine(ImGui::GetWindowWidth() - statusWidth - 16.0f);
        ImGui::TextDisabled("%s", status.c_str());
    }

    ImGui::End();
}

void EditorApp::drawPanels() {
    if (state_.adaptiveWorkspace) {
        if (state_.workspaceDocument == EditorWorkspaceDocument::Scene && !state_.showSceneView)
            state_.workspaceDocument = state_.showAssetPreview ? EditorWorkspaceDocument::Asset : EditorWorkspaceDocument::Attack;
        if (state_.workspaceDocument == EditorWorkspaceDocument::Asset && !state_.showAssetPreview)
            state_.workspaceDocument = state_.showSceneView ? EditorWorkspaceDocument::Scene : EditorWorkspaceDocument::Attack;
        if (state_.workspaceDocument == EditorWorkspaceDocument::Attack && !state_.showAttackColliderEditor)
            state_.workspaceDocument = state_.showSceneView ? EditorWorkspaceDocument::Scene : EditorWorkspaceDocument::Asset;

        if (state_.workspaceRightPane == EditorWorkspaceRightPane::Hierarchy && !state_.showHierarchy)
            state_.workspaceRightPane = EditorWorkspaceRightPane::Inspector;
        if (state_.workspaceRightPane == EditorWorkspaceRightPane::Inspector && !state_.showInspector)
            state_.workspaceRightPane = EditorWorkspaceRightPane::Hierarchy;

        if (state_.workspaceBottomPane == EditorWorkspaceBottomPane::ProjectBrowser && !state_.showProjectBrowser)
            state_.workspaceBottomPane = EditorWorkspaceBottomPane::Console;
        if (state_.workspaceBottomPane == EditorWorkspaceBottomPane::Console && !state_.showConsole)
            state_.workspaceBottomPane = EditorWorkspaceBottomPane::ProjectBrowser;

        if (state_.workspaceDocument == EditorWorkspaceDocument::Scene && state_.showSceneView) drawSceneViewPanel(state_);
        else if (state_.workspaceDocument == EditorWorkspaceDocument::Asset && state_.showAssetPreview) drawAssetPreviewPanel(state_);
        else if (state_.showAttackColliderEditor) drawAttackColliderPanel(state_);

        if (state_.workspaceRightPane == EditorWorkspaceRightPane::Hierarchy && state_.showHierarchy) drawHierarchyPanel(state_);
        else if (state_.showInspector) drawInspectorPanel(state_);

        if (state_.workspaceBottomPane == EditorWorkspaceBottomPane::ProjectBrowser && state_.showProjectBrowser) drawProjectBrowserPanel(state_);
        else if (state_.showConsole) drawConsolePanel(state_);
    } else {
        if (state_.showProjectBrowser) drawProjectBrowserPanel(state_);
        if (state_.showHierarchy) drawHierarchyPanel(state_);
        if (state_.showInspector) drawInspectorPanel(state_);
        if (state_.showSceneView) drawSceneViewPanel(state_);
        if (state_.showAssetPreview) drawAssetPreviewPanel(state_);
        if (state_.showConsole) drawConsolePanel(state_);
        if (state_.showAttackColliderEditor) drawAttackColliderPanel(state_);
    }

    if (state_.showCommandLauncher) drawCommandLauncherPanel(state_);
    if (state_.showRenderMaterialEditor) drawRenderMaterialPanel(state_);
    if (state_.showAnimationTimeline) drawAnimationTimelinePanel(state_);
    if (state_.showVFXEditor) drawVFXEditorPanel(state_);
    if (state_.showInputMapEditor) drawInputMapPanel(state_);
    if (state_.showAIEditor) drawAIEditorPanel(state_);
    if (state_.showGameUIEditor) drawGameUIPanel(state_);
    if (state_.showAudioPreview) drawAudioPreviewPanel(state_);
    if (state_.showDebugProfiler) drawDebugProfilerPanel(state_);
    if (state_.showAssetRegistry) drawAssetRegistryPanel(state_);
}

void EditorApp::loadInitialSceneStub() {
    const auto defaultScene = state_.projectRoot / "assets" / "scenes" / "Scene1.axscene";
    if (std::filesystem::exists(defaultScene)) {
        std::string error;
        if (AXSceneIO::readSceneFile(defaultScene, state_, &error)) {
            state_.currentScenePath = defaultScene;
            state_.log("Loaded AXScene: " + defaultScene.string());
            return;
        }
        state_.log("Could not load existing AXScene: " + error);
    }

    state_.hierarchy.clear();

    auto makeObject = [](
        const std::string& name,
        const std::string& type,
        const std::string& sourcePath,
        float px, float py, float pz,
        float rx, float ry, float rz,
        float sx, float sy, float sz
    ) {
        EditorObject obj;
        obj.name = name;
        obj.type = type;
        obj.sourcePath = sourcePath;
        obj.active = true;
        obj.isStatic = type == "StaticScene";
        obj.parentIndex = -1;
        obj.position[0] = px; obj.position[1] = py; obj.position[2] = pz;
        obj.rotation[0] = rx; obj.rotation[1] = ry; obj.rotation[2] = rz;
        obj.scale[0] = sx; obj.scale[1] = sy; obj.scale[2] = sz;
        return obj;
    };

    EditorObject playerStart = makeObject(
        "PlayerStart", "PlayerStart", "assets/scenes/Scene1.glb",
        0.0f, 0.0f, 0.0f,
        0.0f, 180.0f, 0.0f,
        1.0f, 1.0f, 1.0f
    );
    playerStart.components.push_back({"PlayerStart", true, false, {
        {"spawnCameraYaw", "180"}
    }});
    state_.hierarchy.push_back(playerStart);

    EditorObject sceneGlb = makeObject(
        "SceneGLB", "StaticScene", "assets/Scene1.glb",
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        1.5f, 1.5f, 1.5f
    );
    sceneGlb.components.push_back({"StaticMeshRenderer", true, false, {
        {"model", "assets/Scene1.glb"},
        {"renderProfile", "StylizedAction"}
    }});
    sceneGlb.components.push_back({"MeshCollider", true, false, {
        {"source", "COLLIDER_* nodes"}
    }});
    state_.hierarchy.push_back(sceneGlb);

    EditorObject gunner = makeObject(
        "Enemy_Gunner_01", "Enemy", "assets/Man1_Cinema.glb",
        8.0f, 0.0f, 16.0f,
        0.0f, 180.0f, 0.0f,
        1.0f, 1.0f, 1.0f
    );
    gunner.components.push_back({"SkinnedMeshRenderer", true, false, {
        {"model", "assets/Man1_Cinema.glb"},
        {"defaultAnimation", "Idle"}
    }});
    gunner.components.push_back({"EnemyAI", true, true, {
        {"archetype", "Gunner"},
        {"hp", "160"},
        {"agroRange", "24"}
    }});
    gunner.components.push_back({"GunnerAim", true, true, {
        {"muzzleBone", "Muzzle"},
        {"maxPitchUp", "65"},
        {"maxPitchDown", "-45"},
        {"maxYaw", "75"}
    }});
    state_.hierarchy.push_back(gunner);
    state_.sceneName = "Scene1";
    state_.sceneRenderProfile = "StylizedAction";
    state_.currentScenePath = state_.projectRoot / "assets" / "scenes" / "Scene1.axscene";
    state_.sceneDirty = true;
}

void EditorApp::saveProjectData() {
    auto scenePath = state_.currentScenePath;
    if (scenePath.empty()) scenePath = state_.projectRoot / "assets" / "scenes" / (state_.sceneName + ".axscene");
    if (scenePath.is_relative()) scenePath = state_.projectRoot / scenePath;
    const auto enemyPath = state_.projectRoot / "assets" / "enemies" / "Gunner.axenemy";
    const auto materialPath = state_.projectRoot / "assets" / "materials" / "BLOOM_Blade.axmat";
    const auto generatedPath = state_.projectRoot / "generated" / "AXGeneratedRegistries.cpp";

    std::string error;
    if (!AXSceneIO::writeSceneFile(scenePath, state_, &error)) {
        state_.log("Save AXScene failed: " + error);
        return;
    }
    state_.currentScenePath = scenePath;
    state_.sceneDirty = false;

    // Save canonical AXEnemy and AXMaterial sidecars alongside the scene.
    JsonProjectIO::writeDefaultGunner(enemyPath);
    JsonProjectIO::writeDefaultBloomBlade(materialPath);

    if (state_.generateCppOnSave) {
        CppGenerator::writeRegistries(generatedPath, state_.hierarchy);
    }

    state_.log("Saved AXScene: " + scenePath.string());
    if (state_.generateCppOnSave) state_.log("Generated C++: " + generatedPath.string());
}

void EditorApp::newAXScene() {
    pushUndoSnapshot(makeSnapshot(), "New AXScene");
    state_.hierarchy.clear();
    state_.sceneName = "UntitledScene";
    state_.sceneRenderProfile = "StylizedAction";
    state_.sceneGlbPath.clear();
    state_.currentScenePath = state_.projectRoot / "assets" / "scenes" / "UntitledScene.axscene";
    clearSelection(state_);
    state_.sceneDirty = true;
    state_.log("Created new AXScene: " + state_.currentScenePath.string());
}

void EditorApp::openSelectedAXScene() {
    if (state_.selectedPath.empty()) {
        state_.log("Open AXScene: no selected file.");
        return;
    }
    openAXSceneFile(state_.selectedPath);
}

void EditorApp::openAXSceneFile(const std::filesystem::path& path) {
    std::filesystem::path p = path;
    if (p.is_relative()) p = state_.projectRoot / p;
    std::string error;
    pushUndoSnapshot(makeSnapshot(), "Open AXScene");
    if (!AXSceneIO::readSceneFile(p, state_, &error)) {
        state_.log("Open AXScene failed: " + error);
        return;
    }
    state_.lastCommittedSnapshot = makeSnapshot();
    state_.lastCommittedEditSignature = makeEditSignature();
    state_.log("Opened AXScene: " + p.string());
}

void EditorApp::generateCppOnly() {
    const auto generatedPath = state_.projectRoot / "generated" / "AXGeneratedRegistries.cpp";
    CppGenerator::writeRegistries(generatedPath, state_.hierarchy);
    state_.log("Generated C++: " + generatedPath.string());
}

void EditorApp::resetDefaultLayout() {
    state_.layoutInitialized = false;
    state_.adaptiveWorkspace = true;
    state_.workspaceLayoutMode = EditorWorkspaceLayoutMode::Auto;
    state_.workspaceDocument = EditorWorkspaceDocument::Scene;
    state_.workspaceRightPane = EditorWorkspaceRightPane::Hierarchy;
    state_.workspaceBottomPane = EditorWorkspaceBottomPane::ProjectBrowser;
    state_.showProjectBrowser = true;
    state_.showHierarchy = true;
    state_.showInspector = true;
    state_.showSceneView = true;
    state_.showAssetPreview = true;
    state_.showConsole = true;
    state_.showAttackColliderEditor = true;
    state_.layoutLeftWidthRatio = 0.16f;
    state_.layoutRightWidthRatio = 0.23f;
    state_.layoutTopHeightRatio = 0.66f;
    state_.layoutSceneAssetSplitRatio = 0.56f;
    state_.layoutGap = 6.0f;
    state_.layoutLocked = true;
    state_.log("Adaptive workspace reset for the current display.");
}

void EditorApp::addSceneObject(const std::string& type) {
    pushUndoSnapshot(makeSnapshot(), "Add " + type);
    EditorObject obj;
    obj.name = type + "_" + std::to_string(state_.hierarchy.size());
    obj.type = type;
    obj.sourcePath = type == "Enemy" ? "assets/Man1_Cinema.glb" : "";
    state_.hierarchy.push_back(obj);
    selectSingle(state_, static_cast<int>(state_.hierarchy.size()) - 1);
    state_.lastCommittedSnapshot = makeSnapshot();
    state_.lastCommittedEditSignature = makeEditSignature();
    state_.log("Added scene object: " + obj.name);
}

void EditorApp::deleteSelectedObject() {
    if (state_.selectedObject < 0 || state_.selectedObject >= static_cast<int>(state_.hierarchy.size())) return;
    pushUndoSnapshot(makeSnapshot(), "Delete selected object");
    int index = state_.selectedObject;
    std::string name = state_.hierarchy[index].name;
    int deletedParent = state_.hierarchy[index].parentIndex;
    state_.hierarchy.erase(state_.hierarchy.begin() + index);
    // Re-point the deleted object's children to its own parent, and shift
    // every remaining parentIndex/selection reference down past the gap -
    // otherwise reparented hierarchies get silently corrupted on delete.
    for (auto& o : state_.hierarchy) {
        if (o.parentIndex == index) o.parentIndex = deletedParent;
        if (o.parentIndex > index) o.parentIndex -= 1;
    }
    fixupSelectionAfterErase(state_, index);
    state_.lastCommittedSnapshot = makeSnapshot();
    state_.lastCommittedEditSignature = makeEditSignature();
    state_.log("Deleted scene object: " + name);
}



EditorSnapshot EditorApp::makeSnapshot() const {
    EditorSnapshot snapshot;
    snapshot.hierarchy = state_.hierarchy;
    snapshot.selectedObject = state_.selectedObject;
    snapshot.copiedObject = state_.copiedObject;
    snapshot.selectedPath = state_.selectedPath;
    snapshot.assetBoneTransforms = state_.assetBoneTransforms;
    snapshot.selectedAssetBone = state_.selectedAssetBone;
    snapshot.sceneGlbPath = state_.sceneGlbPath;
    snapshot.currentScenePath = state_.currentScenePath;
    snapshot.sceneName = state_.sceneName;
    snapshot.sceneRenderProfile = state_.sceneRenderProfile;
    snapshot.sceneDirty = state_.sceneDirty;
    snapshot.assetGlbPath = state_.assetGlbPath;
    snapshot.assetFilePath = state_.assetFilePath;
    snapshot.assetPreviewKind = state_.assetPreviewKind;
    return snapshot;
}

void EditorApp::restoreSnapshot(const EditorSnapshot& snapshot) {
    state_.hierarchy = snapshot.hierarchy;
    state_.selectedObject = snapshot.selectedObject;
    state_.copiedObject = snapshot.copiedObject;
    state_.selectedPath = snapshot.selectedPath;
    state_.assetBoneTransforms = snapshot.assetBoneTransforms;
    state_.selectedAssetBone = snapshot.selectedAssetBone;
    state_.sceneGlbPath = snapshot.sceneGlbPath;
    state_.currentScenePath = snapshot.currentScenePath;
    state_.sceneName = snapshot.sceneName;
    state_.sceneRenderProfile = snapshot.sceneRenderProfile;
    state_.sceneDirty = snapshot.sceneDirty;
    state_.assetGlbPath = snapshot.assetGlbPath;
    state_.assetFilePath = snapshot.assetFilePath;
    state_.assetPreviewKind = snapshot.assetPreviewKind;

    if (state_.selectedObject >= static_cast<int>(state_.hierarchy.size())) {
        state_.selectedObject = static_cast<int>(state_.hierarchy.size()) - 1;
    }
    if (state_.selectedObject < -1) state_.selectedObject = -1;
    // Undo/redo snapshots don't carry the multi-selection set (it's
    // transient UI state, not scene data) - collapse back to whatever the
    // snapshot's primary selection resolves to.
    selectSingle(state_, state_.selectedObject);
}

std::string EditorApp::makeEditSignature() const {
    std::ostringstream ss;
    ss << "selected=" << state_.selectedObject << "\n";
    ss << "scene=" << state_.sceneGlbPath << "|" << state_.currentScenePath.string() << "|" << state_.sceneName << "|" << state_.sceneRenderProfile << "\n";
    ss << "asset=" << state_.assetGlbPath << "|" << state_.assetFilePath << "|" << state_.assetPreviewKind << "\n";
    for (const EditorObject& obj : state_.hierarchy) {
        ss << obj.name << "|" << obj.type << "|" << obj.sourcePath << "|"
           << obj.active << "|" << obj.isStatic << "|" << obj.parentIndex << "|";
        for (float v : obj.position) ss << v << ",";
        ss << "|";
        for (float v : obj.rotation) ss << v << ",";
        ss << "|";
        for (float v : obj.scale) ss << v << ",";
        ss << "|components=" << obj.components.size();
        for (const EditorComponent& c : obj.components) {
            ss << "|" << c.type << ":" << c.active << ":" << c.dynamic;
            for (const EditorProperty& p : c.properties) ss << ":" << p.name << "=" << p.value;
        }
        ss << "\n";
    }
    ss << "bones=" << state_.selectedAssetBone << "\n";
    for (const PreviewTransform& t : state_.assetBoneTransforms) {
        for (float v : t.position) ss << v << ",";
        ss << "|";
        for (float v : t.rotation) ss << v << ",";
        ss << "|";
        for (float v : t.scale) ss << v << ",";
        ss << "\n";
    }
    return ss.str();
}

void EditorApp::pushUndoSnapshot(const EditorSnapshot& snapshot, const std::string& reason) {
    state_.undoStack.push_back(snapshot);
    if (static_cast<int>(state_.undoStack.size()) > state_.maxUndoSteps) {
        state_.undoStack.erase(state_.undoStack.begin());
    }
    state_.redoStack.clear();
    if (!reason.empty()) state_.log("Undo checkpoint: " + reason);
}

void EditorApp::undo() {
    if (state_.undoStack.empty()) {
        state_.log("Undo: nothing to undo.");
        return;
    }

    EditorSnapshot current = makeSnapshot();
    EditorSnapshot previous = state_.undoStack.back();
    state_.undoStack.pop_back();
    state_.redoStack.push_back(current);
    restoreSnapshot(previous);
    state_.lastCommittedSnapshot = makeSnapshot();
    state_.lastCommittedEditSignature = makeEditSignature();
    state_.editDragInProgress = false;
    state_.log("Undo");
}

void EditorApp::redo() {
    if (state_.redoStack.empty()) {
        state_.log("Redo: nothing to redo.");
        return;
    }

    EditorSnapshot current = makeSnapshot();
    EditorSnapshot next = state_.redoStack.back();
    state_.redoStack.pop_back();
    state_.undoStack.push_back(current);
    restoreSnapshot(next);
    state_.lastCommittedSnapshot = makeSnapshot();
    state_.lastCommittedEditSignature = makeEditSignature();
    state_.editDragInProgress = false;
    state_.log("Redo");
}

void EditorApp::copySelectedObject() {
    if (state_.selectedObject < 0 || state_.selectedObject >= static_cast<int>(state_.hierarchy.size())) return;
    state_.copiedObject = state_.selectedObject;
    state_.log("Copied object: " + state_.hierarchy[state_.selectedObject].name);
}

void EditorApp::pasteCopiedObject() {
    if (state_.copiedObject < 0 || state_.copiedObject >= static_cast<int>(state_.hierarchy.size())) return;
    pushUndoSnapshot(makeSnapshot(), "Paste object");
    EditorObject obj = state_.hierarchy[state_.copiedObject];
    obj.name += "_Copy";
    obj.position[0] += 1.0f;
    obj.position[2] += 1.0f;
    state_.hierarchy.push_back(obj);
    selectSingle(state_, static_cast<int>(state_.hierarchy.size()) - 1);
    state_.lastCommittedSnapshot = makeSnapshot();
    state_.lastCommittedEditSignature = makeEditSignature();
    state_.log("Pasted object: " + obj.name);
}

void EditorApp::duplicateSelectedObject() {
    if (state_.selectedObject < 0 || state_.selectedObject >= static_cast<int>(state_.hierarchy.size())) return;
    state_.copiedObject = state_.selectedObject;
    pasteCopiedObject();
}

void EditorApp::processGlobalShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        saveProjectData();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        state_.showCommandLauncher = !state_.showCommandLauncher;
        if (state_.showCommandLauncher) state_.commandSearch.clear();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (shift) redo();
        else undo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        redo();
    }
    if (!io.WantTextInput && ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        copySelectedObject();
    }
    if (!io.WantTextInput && ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        pasteCopiedObject();
    }
    if (!io.WantTextInput && ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        duplicateSelectedObject();
    }
    if (!io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
        deleteSelectedObject();
    }
    if (shift && ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        stopGameProcess();
    } else if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        runGameDetached(false);
    }
}

void EditorApp::trackUndoableEdits() {
    const std::string currentSignature = makeEditSignature();
    const bool changed = currentSignature != state_.lastCommittedEditSignature;
    const bool editingWidget = ImGui::IsAnyItemActive();

    if (!state_.editDragInProgress && editingWidget && changed) {
        state_.editDragInProgress = true;
        state_.editDragStartSignature = state_.lastCommittedEditSignature;
        state_.editDragStartSnapshot = state_.lastCommittedSnapshot;
    }

    if (state_.editDragInProgress && !editingWidget) {
        if (changed) {
            pushUndoSnapshot(state_.editDragStartSnapshot, "Viewport/details edit");
            state_.sceneDirty = true;
            state_.lastCommittedSnapshot = makeSnapshot();
            state_.lastCommittedEditSignature = currentSignature;
        }
        state_.editDragInProgress = false;
        return;
    }

    if (!editingWidget && changed && !state_.editDragInProgress) {
        pushUndoSnapshot(state_.lastCommittedSnapshot, "Editor edit");
        state_.sceneDirty = true;
        state_.lastCommittedSnapshot = makeSnapshot();
        state_.lastCommittedEditSignature = currentSignature;
    }
}

SDL_Process* EditorApp::launchProcess(const std::string& label, const std::vector<std::string>& args,
                                      const std::filesystem::path& logFile) {
    if (args.empty()) {
        state_.log(label + " failed: no command was supplied.");
        return nullptr;
    }

    std::error_code ec;
    std::filesystem::create_directories(logFile.parent_path(), ec);
    SDL_IOStream* logStream = SDL_IOFromFile(logFile.string().c_str(), "wb");
    if (!logStream) {
        state_.log(label + " could not open log file: " + std::string(SDL_GetError()));
        return nullptr;
    }

    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) argv.push_back(arg.c_str());
    argv.push_back(nullptr);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        state_.log(label + " could not create SDL process properties: " + std::string(SDL_GetError()));
        SDL_CloseIO(logStream);
        return nullptr;
    }

    const std::string workingDirectory = state_.projectRoot.string();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, workingDirectory.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, logStream);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);

    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_CloseIO(logStream);

    if (!process) {
        state_.log(label + " failed: " + std::string(SDL_GetError()));
        return nullptr;
    }

    state_.log(label + " launched through SDL's cross-platform process API.");
    state_.log(label + " log: " + logFile.string());
    return process;
}

bool EditorApp::syncRuntimeAssets() {
    const auto buildRoot = state_.projectRoot / "build";
    std::error_code ec;
    std::filesystem::create_directories(buildRoot, ec);
    if (ec) {
        state_.log("Runtime asset sync failed to create build/: " + ec.message());
        return false;
    }

    auto replaceDirectory = [this](const std::filesystem::path& source,
                                   const std::filesystem::path& destination,
                                   const char* label) -> bool {
        std::error_code opError;
        if (!std::filesystem::exists(source, opError) || opError) {
            state_.log(std::string("Runtime asset sync skipped missing ") + label + ": " + source.string());
            return !opError;
        }

        std::filesystem::remove_all(destination, opError);
        if (opError) {
            state_.log(std::string("Runtime asset sync could not clear ") + destination.string() + ": " + opError.message());
            return false;
        }
        std::filesystem::create_directories(destination, opError);
        if (opError) {
            state_.log(std::string("Runtime asset sync could not create ") + destination.string() + ": " + opError.message());
            return false;
        }

        std::filesystem::copy(source, destination,
            std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing,
            opError);
        if (opError) {
            state_.log(std::string("Runtime asset sync failed for ") + label + ": " + opError.message());
            return false;
        }
        return true;
    };

    if (!replaceDirectory(state_.projectRoot / "assets", buildRoot / "assets", "assets/")) return false;

    // Game also supports a legacy top-level shaders/ lookup, so mirror the
    // canonical assets/shaders directory there without using a platform shell.
    if (!replaceDirectory(state_.projectRoot / "assets" / "shaders", buildRoot / "shaders", "assets/shaders/")) return false;

    const auto generatedSource = state_.projectRoot / "generated";
    std::error_code generatedExistsError;
    if (std::filesystem::exists(generatedSource, generatedExistsError)) {
        if (!replaceDirectory(generatedSource, buildRoot / "generated", "generated/")) return false;
    } else if (generatedExistsError) {
        state_.log("Runtime asset sync could not inspect generated/: " + generatedExistsError.message());
        return false;
    }

    const auto projectFile = state_.projectRoot / "project.axproject";
    std::error_code projectExistsError;
    if (std::filesystem::exists(projectFile, projectExistsError)) {
        std::error_code copyError;
        std::filesystem::copy_file(projectFile, buildRoot / "project.axproject",
                                   std::filesystem::copy_options::overwrite_existing, copyError);
        if (copyError) {
            state_.log("Runtime asset sync failed for project.axproject: " + copyError.message());
            return false;
        }
    } else if (projectExistsError) {
        state_.log("Runtime asset sync could not inspect project.axproject: " + projectExistsError.message());
        return false;
    }

    state_.log("Runtime assets synchronized with the editor project.");
    return true;
}

std::filesystem::path EditorApp::findGameExecutable() const {
    const std::vector<std::filesystem::path> candidates = {
        state_.projectRoot / "build" / "Game",
        state_.projectRoot / "build" / "Game.exe",
        state_.projectRoot / "build" / "Debug" / "Game.exe",
        state_.projectRoot / "build" / "Release" / "Game.exe",
        state_.projectRoot / "build" / "RelWithDebInfo" / "Game.exe",
        state_.projectRoot / "build" / "MinSizeRel" / "Game.exe"
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}

void EditorApp::buildGameTargetDetached() {
    if (buildProcess_) {
        state_.log("A Game build is already running.");
        return;
    }
    const auto logPath = state_.projectRoot / "build" / "AXEditorBuild.log";
    buildProcess_ = launchProcess("Build Game", {"cmake", "--build", "build", "--target", "Game", "--parallel"}, logPath);
}

void EditorApp::runGameDetached(bool buildFirst) {
    if (gameProcess_) {
        state_.log("Game is already running. Use Build > Stop Running Game first.");
        return;
    }

    if (buildFirst) {
        if (buildProcess_) {
            state_.log("A Game build is already running.");
            return;
        }
        runGameAfterBuild_ = true;
        buildGameTargetDetached();
        return;
    }

    const auto gameExe = findGameExecutable();
    if (gameExe.empty()) {
        state_.log("Game executable was not found under build/. Use Build + Run Game Detached first.");
        return;
    }
    if (!syncRuntimeAssets()) {
        state_.log("Game launch cancelled because runtime asset synchronization failed.");
        return;
    }
    const auto logPath = state_.projectRoot / "build" / "AXGame.log";
    gameProcess_ = launchProcess("Run Game", {gameExe.string()}, logPath);
}

void EditorApp::stopGameProcess() {
    if (!gameProcess_) {
        state_.log("No detached game process is registered.");
        return;
    }
    if (SDL_KillProcess(gameProcess_, false)) state_.log("Requested graceful Game shutdown.");
    else state_.log("Failed to stop Game: " + std::string(SDL_GetError()));
}

void EditorApp::pollChildProcesses() {
    auto pollOne = [this](SDL_Process*& process, const char* label) -> int {
        if (!process) return 999999;
        int exitCode = 0;
        if (!SDL_WaitProcess(process, false, &exitCode)) return 999999;
        state_.log(std::string(label) + " exited with code " + std::to_string(exitCode));
        SDL_DestroyProcess(process);
        process = nullptr;
        return exitCode;
    };

    const int buildExit = pollOne(buildProcess_, "Build Game");
    if (buildExit != 999999 && runGameAfterBuild_) {
        runGameAfterBuild_ = false;
        if (buildExit == 0) runGameDetached(false);
        else state_.log("Build + Run stopped because the build failed. See build/AXEditorBuild.log.");
    }
    pollOne(gameProcess_, "Game");
}

void EditorApp::rebuildUiStyle(bool logChange) {
    if (!ImGui::GetCurrentContext()) return;
    ImGui::GetStyle() = ImGuiStyle();
    applyAxEditorTheme(currentTheme_, state_, false);

    ImGuiStyle& style = ImGui::GetStyle();
    const float display = state_.followDisplayScale ? std::clamp(state_.displayScale, 0.75f, 2.5f) : 1.0f;
    const float user = std::clamp(state_.uiScale, 0.75f, 2.0f);
    const float layoutScale = display * user;
    style.ScaleAllSizes(layoutScale);
    style.FontScaleMain = std::clamp(state_.readableFontPixels / 13.0f, 0.80f, 2.0f) * user;
    style.FontScaleDpi = display;

    if (logChange) {
        state_.log("UI scale rebuilt: display " + std::to_string(display) + "x, user " +
                   std::to_string(user) + "x, font " + std::to_string((int)state_.readableFontPixels) + " px.");
    }
}

void EditorApp::refreshDisplayScale(bool force) {
    if (!window_) return;
    const float queried = SDL_GetWindowDisplayScale(window_);
    const float newScale = queried > 0.0f ? queried : 1.0f;
    if (force || std::abs(newScale - state_.displayScale) > 0.01f) {
        state_.displayScale = newScale;
        rebuildUiStyle(true);
    }
}

void EditorApp::applyEditorTheme(const std::string& themeName) {
    currentTheme_ = themeName;
    rebuildUiStyle(false);
    state_.log("UI theme: " + themeName);
}

void EditorApp::applyReadableFont(float pixelSize) {
    state_.readableFontPixels = std::clamp(pixelSize, 12.0f, 24.0f);
    rebuildUiStyle(true);
}

void EditorApp::applyUiScale(float scale) {
    state_.uiScale = std::clamp(scale, 0.75f, 2.0f);
    rebuildUiStyle(true);
}
