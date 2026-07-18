#include "CommandLauncherPanel.h"
#include "PanelLayout.h"
#include "EditorState.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <string>

namespace {
struct Cmd { const char* name; const char* desc; bool EditorState::*flag; };
static Cmd commands[] = {
    {"Scene Preview", "Open the main .axscene viewport.", &EditorState::showSceneView},
    {"Asset Preview", "Inspect GLB meshes, skeletons, sockets, and asset metadata.", &EditorState::showAssetPreview},
    {"Project Browser", "Browse project files.", &EditorState::showProjectBrowser},
    {"Hierarchy", "Scene object tree.", &EditorState::showHierarchy},
    {"Inspector", "Selected object details/components.", &EditorState::showInspector},
    {"Console", "Editor/runtime messages.", &EditorState::showConsole},
    {"Attack Collider Editor", "Author attack volumes, hurt/trigger data, active times.", &EditorState::showAttackColliderEditor},
    {"Render / Material Editor", "Realistic/cartoon material and shader settings.", &EditorState::showRenderMaterialEditor},
    {"Animation Timeline", "Preview Blender GLB clips, frames, seconds, events.", &EditorState::showAnimationTimeline},
    {"VFX Editor", "Slash mesh, impact, shockwave, afterimage, animated texture VFX.", &EditorState::showVFXEditor},
    {"Input Map Editor", "Actions, bindings, controller support, buffers.", &EditorState::showInputMapEditor},
    {"AI Editor", "Enemy combat behavior data.", &EditorState::showAIEditor},
    {"Game UI Editor", "HUD layout and runtime UI metadata.", &EditorState::showGameUIEditor},
    {"Audio Preview", "Waveform/spectrum preview only; DAW stays external.", &EditorState::showAudioPreview},
    {"Debug / Profiler", "Colliders, input buffer, AI state, frame timings.", &EditorState::showDebugProfiler},
    {"Asset Registry", "Stable asset IDs and version tracking.", &EditorState::showAssetRegistry}
};
static bool containsNoCase(const std::string& a, const std::string& b) {
    std::string x=a,y=b; std::transform(x.begin(),x.end(),x.begin(),[](unsigned char c){return (char)std::tolower(c);});
    std::transform(y.begin(),y.end(),y.begin(),[](unsigned char c){return (char)std::tolower(c);});
    return x.find(y)!=std::string::npos;
}
}
void drawCommandLauncherPanel(EditorState& state) {
    AXEditorLayout::prepareFloatingTool(state, 620.0f, 460.0f);
    if (!ImGui::Begin("Command Search / Window Launcher", &state.showCommandLauncher)) { ImGui::End(); return; }
    ImGui::TextDisabled("Ctrl+Space opens this launcher. AXEngine tools stay focused on game assembly/runtime, not Blender or DAW work.");
    ImGui::InputTextWithHint("##command", "Search windows/jobs, e.g. material, vfx, input, ai...", &state.commandSearch);
    ImGui::Separator();
    for (const auto& c : commands) {
        if (!state.commandSearch.empty() && !containsNoCase(c.name, state.commandSearch) && !containsNoCase(c.desc, state.commandSearch)) continue;
        if (ImGui::Button(c.name, ImVec2(220,0))) { state.*(c.flag)=true; state.activeToolContext=c.name; state.log(std::string("Opened tool: ")+c.name); }
        ImGui::SameLine(); ImGui::TextWrapped("%s", c.desc);
    }
    ImGui::End();
}
