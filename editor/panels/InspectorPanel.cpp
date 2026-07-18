#include "InspectorPanel.h"
#include "PanelLayout.h"
#include <imgui.h>
#include <cstring>

static void editString(const char* label, std::string& value) {
    char buffer[256] = {};
    std::strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    if (ImGui::InputText(label, buffer, sizeof(buffer))) {
        value = buffer;
    }
}

void drawInspectorPanel(EditorState& state) {
    AXEditorLayout::placeInspector(state);
    ImGui::Begin("Inspector", &state.showInspector, AXEditorLayout::panelFlags(state));

    const int selCount = (int)state.selectedObjects.size();

    if (selCount > 1) {
        ImGui::Text("%d objects selected", selCount);
        ImGui::TextDisabled("Multi-edit (applies as a delta to all selected)");
        static float moveDelta[3] = {0.0f, 0.0f, 0.0f};
        static float rotateDelta[3] = {0.0f, 0.0f, 0.0f};
        static float scaleRatio[3] = {1.0f, 1.0f, 1.0f};

        bool moved = ImGui::DragFloat3("Move by", moveDelta, 0.05f);
        bool rotated = ImGui::DragFloat3("Rotate by", rotateDelta, 0.25f);
        bool scaled = ImGui::DragFloat3("Scale by (x)", scaleRatio, 0.01f, 0.01f, 100.0f);

        if (moved) for (int idx : state.selectedObjects) {
            state.hierarchy[idx].position[0] += moveDelta[0];
            state.hierarchy[idx].position[1] += moveDelta[1];
            state.hierarchy[idx].position[2] += moveDelta[2];
        }
        if (rotated) for (int idx : state.selectedObjects) {
            state.hierarchy[idx].rotation[0] += rotateDelta[0];
            state.hierarchy[idx].rotation[1] += rotateDelta[1];
            state.hierarchy[idx].rotation[2] += rotateDelta[2];
        }
        if (scaled) for (int idx : state.selectedObjects) {
            state.hierarchy[idx].scale[0] *= scaleRatio[0];
            state.hierarchy[idx].scale[1] *= scaleRatio[1];
            state.hierarchy[idx].scale[2] *= scaleRatio[2];
        }
        if (moved || rotated || scaled) {
            moveDelta[0]=moveDelta[1]=moveDelta[2]=0.0f;
            rotateDelta[0]=rotateDelta[1]=rotateDelta[2]=0.0f;
            scaleRatio[0]=scaleRatio[1]=scaleRatio[2]=1.0f;
        }

        ImGui::Separator();
        ImGui::TextWrapped("Selected objects:");
        for (int idx : state.selectedObjects) {
            if (idx >= 0 && idx < (int)state.hierarchy.size()) ImGui::BulletText("%s", state.hierarchy[idx].name.c_str());
        }
    } else if (state.selectedObject >= 0 && state.selectedObject < static_cast<int>(state.hierarchy.size())) {
        EditorObject& obj = state.hierarchy[state.selectedObject];
        editString("Name", obj.name);
        editString("Type", obj.type);
        editString("Source", obj.sourcePath);
        ImGui::DragFloat3("Position", obj.position, 0.05f);
        ImGui::DragFloat3("Rotation", obj.rotation, 0.25f);
        ImGui::DragFloat3("Scale", obj.scale, 0.01f, 0.01f, 100.0f);

        ImGui::Separator();
        if (obj.type == "Enemy") {
            ImGui::Text("Enemy authoring preview");
            ImGui::BulletText("Archetype comes from generated EnemyRegistry later.");
            ImGui::BulletText("Gun enemies will use aim bones / aim clips here.");
        } else if (obj.type == "StaticScene") {
            ImGui::Text("Scene GLB object");
            ImGui::BulletText("Later: extract hierarchy, colliders, markers, materials.");
        }
    } else if (!state.selectedPath.empty()) {
        ImGui::TextWrapped("Selected file:\n%s", state.selectedPath.string().c_str());
        if (state.selectedPath.extension() == ".glb") {
            ImGui::Separator();
            ImGui::Text("GLB Asset Actions");
            if (ImGui::Button("Open in Asset Preview")) {
                state.showAssetPreview = true;
                state.log("Opened in Asset Preview: " + state.selectedPath.string());
            }
            if (ImGui::Button("Inspect Materials / Bones")) {
                state.showAssetPreview = true;
                state.log("Asset Preview will inspect rig/material metadata later.");
            }
        }
    } else {
        ImGui::TextWrapped("Select an object or file to inspect.");
    }

    ImGui::End();
}
