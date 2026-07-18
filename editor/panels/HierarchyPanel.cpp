#include "HierarchyPanel.h"
#include "PanelLayout.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

static EditorComponent makeSimpleComponent(const std::string& type) {
    EditorComponent c;
    c.type = type;
    c.active = true;
    c.dynamic = type == "EnemyAI" || type == "SkinnedMeshRenderer" || type == "Camera";
    return c;
}

static void addObject(EditorState& state, const char* type, const char* source = "") {
    EditorObject obj;
    obj.name = std::string(type) + "_" + std::to_string(state.hierarchy.size());
    obj.type = type;
    obj.sourcePath = source;
    obj.components.push_back(makeSimpleComponent("Transform"));
    if (std::string(type) == "Enemy") {
        obj.components.push_back(makeSimpleComponent("SkinnedMeshRenderer"));
        obj.components.push_back(makeSimpleComponent("EnemyAI"));
    } else if (std::string(type) == "Camera") obj.components.push_back(makeSimpleComponent("Camera"));
    else if (std::string(type) == "Light") obj.components.push_back(makeSimpleComponent("DirectionalLight"));
    obj.basePosition[0] = obj.position[0]; obj.basePosition[1] = obj.position[1]; obj.basePosition[2] = obj.position[2];
    obj.baseRotation[0] = obj.rotation[0]; obj.baseRotation[1] = obj.rotation[1]; obj.baseRotation[2] = obj.rotation[2];
    obj.baseScale[0] = obj.scale[0]; obj.baseScale[1] = obj.scale[1]; obj.baseScale[2] = obj.scale[2];
    state.hierarchy.push_back(obj);
    selectSingle(state, static_cast<int>(state.hierarchy.size()) - 1);
    state.log("Added scene object: " + obj.name);
}

// True if `candidateAncestor` is `index` itself, or anywhere in `index`'s
// parent chain. Used to stop the user picking a parent that would create a
// cycle (an object can't become its own descendant).
static bool isSelfOrAncestor(const EditorState& state, int index, int candidateAncestor) {
    int cursor = index;
    int guard = (int)state.hierarchy.size() + 1;
    while (cursor >= 0 && cursor < (int)state.hierarchy.size() && guard-- > 0) {
        if (cursor == candidateAncestor) return true;
        cursor = state.hierarchy[cursor].parentIndex;
    }
    return false;
}

static void deleteObject(EditorState& state, int index) {
    if (index < 0 || index >= (int)state.hierarchy.size()) return;
    const std::string name = state.hierarchy[index].name;
    int deletedParent = state.hierarchy[index].parentIndex;

    state.hierarchy.erase(state.hierarchy.begin() + index);

    for (auto& obj : state.hierarchy) {
        if (obj.parentIndex == index) obj.parentIndex = deletedParent;
        if (obj.parentIndex > index) obj.parentIndex -= 1;
    }
    fixupSelectionAfterErase(state, index);

    state.log("Deleted scene object: " + name);
}

static void deleteSelected(EditorState& state) {
    std::vector<int> toDelete = state.selectedObjects;
    if (toDelete.empty() && state.selectedObject >= 0) toDelete.push_back(state.selectedObject);
    // Highest index first so earlier indices in the list stay valid as we go.
    std::sort(toDelete.begin(), toDelete.end(), std::greater<int>());
    for (int idx : toDelete) deleteObject(state, idx);
}

// Reparents every currently-selected object onto `targetIndex` (or to root
// if targetIndex < 0). Objects that would form a cycle with the target
// (the target itself, or one of the target's own ancestors) are skipped
// rather than silently corrupting the hierarchy.
static void reparentSelectedTo(EditorState& state, int targetIndex) {
    for (int idx : state.selectedObjects) {
        if (idx == targetIndex) continue;
        if (targetIndex >= 0 && isSelfOrAncestor(state, targetIndex, idx)) continue;
        state.hierarchy[idx].parentIndex = targetIndex;
    }
    state.log("Reparented " + std::to_string(state.selectedObjects.size()) + " object(s)");
}

static void drawObjectRow(EditorState& state, int index, const std::vector<std::vector<int>>& childrenOf, int& shiftAnchor) {
    const auto& obj = state.hierarchy[index];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (childrenOf[index].empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected(state, index)) flags |= ImGuiTreeNodeFlags_Selected;
    std::string label = obj.name + "  <" + obj.type + ">";

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(index)), flags, "%s", label.c_str());
    if (ImGui::IsItemClicked()) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.KeyShift) rangeSelect(state, shiftAnchor, index);
        else if (io.KeyCtrl) toggleSelect(state, index);
        else selectSingle(state, index);
        shiftAnchor = state.selectedObject;
    }

    if (!childrenOf[index].empty()) {
        if (open) {
            for (int child : childrenOf[index]) drawObjectRow(state, child, childrenOf, shiftAnchor);
            ImGui::TreePop();
        }
    }
}

void drawHierarchyPanel(EditorState& state) {
    AXEditorLayout::placeHierarchy(state);
    ImGui::Begin("Hierarchy", &state.showHierarchy, AXEditorLayout::panelFlags(state));

    if (ImGui::Button("+ Empty")) addObject(state, "Empty");
    ImGui::SameLine();
    if (ImGui::Button("+ Enemy")) addObject(state, "Enemy", "assets/enemies/Gunner.glb");
    ImGui::SameLine();
    if (ImGui::Button("+ Camera")) addObject(state, "Camera");
    ImGui::SameLine();
    if (ImGui::Button("Delete") && (!state.selectedObjects.empty() || state.selectedObject >= 0)) {
        deleteSelected(state);
    }

    ImGui::Separator();
    ImGui::TextDisabled("Ctrl+Click: add/remove.  Shift+Click: range select.");

    std::vector<std::vector<int>> childrenOf(state.hierarchy.size());
    std::vector<bool> isRoot(state.hierarchy.size(), true);
    for (int i = 0; i < (int)state.hierarchy.size(); ++i) {
        int p = state.hierarchy[i].parentIndex;
        if (p >= 0 && p < (int)state.hierarchy.size() && p != i && !isSelfOrAncestor(state, p, i)) {
            childrenOf[p].push_back(i);
            isRoot[i] = false;
        }
    }
    static int shiftAnchor = -1;
    for (int i = 0; i < (int)state.hierarchy.size(); ++i) {
        if (isRoot[i]) drawObjectRow(state, i, childrenOf, shiftAnchor);
    }

    ImGui::Separator();

    const int selCount = (int)state.selectedObjects.size();
    if (selCount == 0) {
        ImGui::TextDisabled("No object selected.");
        ImGui::End();
        return;
    }

    // --- Parent picker: works for one or many selected objects at once ---
    bool sameParent = true;
    int firstParent = state.hierarchy[state.selectedObjects[0]].parentIndex;
    for (int idx : state.selectedObjects) if (state.hierarchy[idx].parentIndex != firstParent) { sameParent = false; break; }
    std::string currentLabel = "Multiple";
    if (sameParent) currentLabel = (firstParent >= 0 && firstParent < (int)state.hierarchy.size()) ? state.hierarchy[firstParent].name : "None";

    ImGui::Text("%d object(s) selected", selCount);
    if (ImGui::BeginCombo("Parent", currentLabel.c_str())) {
        if (ImGui::Selectable("None")) reparentSelectedTo(state, -1);
        for (int i = 0; i < (int)state.hierarchy.size(); ++i) {
            if (std::find(state.selectedObjects.begin(), state.selectedObjects.end(), i) != state.selectedObjects.end()) continue;
            bool wouldCycle = false;
            for (int idx : state.selectedObjects) if (isSelfOrAncestor(state, i, idx)) { wouldCycle = true; break; }
            if (wouldCycle) continue;
            if (ImGui::Selectable(state.hierarchy[i].name.c_str())) reparentSelectedTo(state, i);
        }
        ImGui::EndCombo();
    }

    if (selCount == 1) {
        EditorObject& obj = state.hierarchy[state.selectedObjects[0]];
        ImGui::TextWrapped("%s", obj.name.c_str());
        ImGui::DragFloat3("Location", obj.position, 0.05f);
        ImGui::DragFloat3("Rotation", obj.rotation, 0.25f);
        ImGui::DragFloat3("Scale", obj.scale, 0.01f, 0.01f, 100.0f);
        ImGui::Text("Components: %d", (int)obj.components.size());
    } else {
        // Multi-edit: these fields don't show any one object's absolute
        // value (they'd disagree). Instead each drag is a *nudge* applied
        // to every selected object - move by X, rotate by X degrees, scale
        // by a ratio - matching how Unreal/Unity's multi-select transform
        // behaves. Fields reset to the neutral value after being applied.
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
        if (ImGui::IsItemDeactivatedAfterEdit() || moved || rotated || scaled) {
            moveDelta[0]=moveDelta[1]=moveDelta[2]=0.0f;
            rotateDelta[0]=rotateDelta[1]=rotateDelta[2]=0.0f;
            scaleRatio[0]=scaleRatio[1]=scaleRatio[2]=1.0f;
        }
    }

    ImGui::End();
}
