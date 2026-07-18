#include "ConsolePanel.h"
#include "PanelLayout.h"
#include <imgui.h>

void drawConsolePanel(EditorState& state) {
    AXEditorLayout::placeConsole(state);
    ImGui::Begin("Console", &state.showConsole, AXEditorLayout::panelFlags(state));
    if (ImGui::Button("Clear")) state.console.clear();
    ImGui::SameLine();
    if (ImGui::Button("Log Layout")) state.log("Adaptive workspace: Document | Right pane | Bottom pane");
    ImGui::Separator();
    for (const auto& line : state.console) {
        ImGui::TextWrapped("%s", line.text.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();
}
