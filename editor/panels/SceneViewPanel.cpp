#include "SceneViewPanel.h"
#include "PanelLayout.h"
#include "preview/EditorPreviewRenderer.h"

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <cmath>
#include <cctype>
#include <cstdio>

namespace {

static std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static bool isGlbPath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".glb" || e == ".gltf";
}

static bool looksLikeSceneGlb(const std::filesystem::path& p) {
    const std::string s = lowerCopy(p.string());
    return s.find("scene") != std::string::npos || s.find("level") != std::string::npos || s.find("map") != std::string::npos;
}


static void drawAxisGizmo2D(ImDrawList* dl, const ImVec2& origin, const ImVec2& xEnd, const ImVec2& yEnd, const ImVec2& zEnd) {
    auto arrow = [&](const ImVec2& a, const ImVec2& b, ImU32 col, const char* label) {
        dl->AddLine(a, b, col, 2.5f);
        float dx = b.x - a.x, dy = b.y - a.y;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len > 0.001f) {
            dx /= len; dy /= len;
            ImVec2 left(b.x - dx*9.0f - dy*4.0f, b.y - dy*9.0f + dx*4.0f);
            ImVec2 right(b.x - dx*9.0f + dy*4.0f, b.y - dy*9.0f - dx*4.0f);
            dl->AddTriangleFilled(b, left, right, col);
        }
        dl->AddText(ImVec2(b.x + 3.0f, b.y + 3.0f), col, label);
    };
    arrow(origin, xEnd, IM_COL32(235, 70, 70, 255), "X");
    arrow(origin, yEnd, IM_COL32(75, 220, 90, 255), "Y");
    arrow(origin, zEnd, IM_COL32(70, 135, 255, 255), "Z");
}

static void updateFlyCamera(float yawDeg, float pitchDeg, float* cameraPos, float speed, bool active) {
    if (!active) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    float dt = std::max(io.DeltaTime, 1.0f / 120.0f);
    float yaw = yawDeg * 3.14159265f / 180.0f;
    float pitch = pitchDeg * 3.14159265f / 180.0f;
    float fwdX = -std::sin(yaw) * std::cos(pitch);
    float fwdY = -std::sin(pitch);
    float fwdZ = -std::cos(yaw) * std::cos(pitch);
    float rightX = std::cos(yaw);
    float rightZ = -std::sin(yaw);
    float s = speed * dt;
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) s *= 3.0f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) { cameraPos[0] += fwdX * s; cameraPos[1] += fwdY * s; cameraPos[2] += fwdZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_S)) { cameraPos[0] -= fwdX * s; cameraPos[1] -= fwdY * s; cameraPos[2] -= fwdZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_A)) { cameraPos[0] -= rightX * s; cameraPos[2] -= rightZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_D)) { cameraPos[0] += rightX * s; cameraPos[2] += rightZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_Q)) { cameraPos[1] -= s; }
    if (ImGui::IsKeyDown(ImGuiKey_E)) { cameraPos[1] += s; }
}

static void dollyCamera(float yawDeg, float pitchDeg, float* cameraPos, float amount) {
    float yaw = yawDeg * 3.14159265f / 180.0f;
    float pitch = pitchDeg * 3.14159265f / 180.0f;
    float fwdX = -std::sin(yaw) * std::cos(pitch);
    float fwdY = -std::sin(pitch);
    float fwdZ = -std::cos(yaw) * std::cos(pitch);
    cameraPos[0] += fwdX * amount;
    cameraPos[1] += fwdY * amount;
    cameraPos[2] += fwdZ * amount;
}

static const char* toolName(EditorTransformTool tool) {
    switch (tool) {
        case EditorTransformTool::Translate: return "Translate";
        case EditorTransformTool::Rotate: return "Rotate";
        case EditorTransformTool::Scale: return "Scale";
        case EditorTransformTool::LotRotScale: return "LocRotScale";
    }
    return "Unknown";
}

static void drawTransformToolButtons(EditorState& state) {
    if (ImGui::RadioButton("Loc", state.transformTool == EditorTransformTool::Translate)) state.transformTool = EditorTransformTool::Translate;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rot", state.transformTool == EditorTransformTool::Rotate)) state.transformTool = EditorTransformTool::Rotate;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", state.transformTool == EditorTransformTool::Scale)) state.transformTool = EditorTransformTool::Scale;
    ImGui::SameLine();
    if (ImGui::RadioButton("LocRotScale", state.transformTool == EditorTransformTool::LotRotScale)) state.transformTool = EditorTransformTool::LotRotScale;
}

static EditorComponent makeComponent(const std::string& type) {
    EditorComponent c;
    c.type = type;
    c.active = true;
    c.dynamic = false;

    if (type == "Transform") {
        c.dynamic = true;
    } else if (type == "StaticMeshRenderer") {
        c.properties.push_back({"mesh", "from GLB node"});
        c.properties.push_back({"materialOverride", ""});
        c.properties.push_back({"castShadows", "true"});
    } else if (type == "SkinnedMeshRenderer") {
        c.dynamic = true;
        c.properties.push_back({"model", ""});
        c.properties.push_back({"defaultAnimation", "Idle"});
    } else if (type == "Camera") {
        c.dynamic = true;
        c.properties.push_back({"fov", "60"});
        c.properties.push_back({"near", "0.05"});
        c.properties.push_back({"far", "1000"});
    } else if (type == "DirectionalLight") {
        c.properties.push_back({"color", "1,1,1"});
        c.properties.push_back({"intensity", "1.0"});
        c.properties.push_back({"castsShadows", "true"});
    } else if (type == "BoxCollider" || type == "MeshCollider") {
        c.properties.push_back({"blocksPlayer", "true"});
        c.properties.push_back({"blocksCamera", "true"});
    } else if (type == "EnemyAI") {
        c.dynamic = true;
        c.properties.push_back({"archetype", "Basic"});
        c.properties.push_back({"hp", "160"});
        c.properties.push_back({"agroRange", "12"});
    } else if (type == "GunnerAim") {
        c.dynamic = true;
        c.properties.push_back({"muzzleBone", "Muzzle"});
        c.properties.push_back({"aimPitchUp", "65"});
        c.properties.push_back({"aimPitchDown", "-45"});
        c.properties.push_back({"aimYaw", "75"});
    } else if (type == "ProjectileEmitter") {
        c.dynamic = true;
        c.properties.push_back({"projectile", "Bullet"});
        c.properties.push_back({"cooldown", "1.2"});
        c.properties.push_back({"speed", "28"});
    } else if (type == "AttackCollider") {
        c.dynamic = true;
        c.properties.push_back({"attackFile", "assets/combat/attacks/SwordAttack1.axattack"});
        c.properties.push_back({"triggeredBy", "ComboMove"});
        c.properties.push_back({"debugDraw", "true"});
    } else if (type == "Hurtbox") {
        c.dynamic = true;
        c.properties.push_back({"center", "0,1,0"});
        c.properties.push_back({"halfExtents", "0.5,1,0.5"});
        c.properties.push_back({"faction", "Enemy"});
    } else if (type == "CombatActor") {
        c.dynamic = true;
        c.properties.push_back({"faction", "Player"});
        c.properties.push_back({"poise", "100"});
        c.properties.push_back({"invulnerable", "false"});
    }
    return c;
}

static void addDefaultComponents(EditorObject& obj) {
    obj.components.clear();
    obj.components.push_back(makeComponent("Transform"));
    if (obj.type == "Enemy") {
        obj.components.push_back(makeComponent("SkinnedMeshRenderer"));
        obj.components.push_back(makeComponent("EnemyAI"));
        if (lowerCopy(obj.name).find("gun") != std::string::npos || lowerCopy(obj.name).find("rifle") != std::string::npos) {
            obj.components.push_back(makeComponent("GunnerAim"));
            obj.components.push_back(makeComponent("ProjectileEmitter"));
        }
    } else if (obj.type == "Light") {
        obj.components.push_back(makeComponent("DirectionalLight"));
    } else if (obj.type == "Camera") {
        obj.components.push_back(makeComponent("Camera"));
    } else if (obj.type == "StaticScene" || obj.type == "MeshNode") {
        obj.components.push_back(makeComponent("StaticMeshRenderer"));
    } else if (obj.type == "Collider") {
        obj.components.push_back(makeComponent("MeshCollider"));
    }
}

static std::filesystem::path resolveProjectPath(const EditorState& state, const std::string& rel) {
    std::filesystem::path p(rel);
    if (p.is_absolute()) return p;
    return state.projectRoot / p;
}

static std::filesystem::path resolveScenePath(const EditorState& state) {
    std::filesystem::path p(state.sceneGlbPath);
    if (!p.is_absolute()) p = state.projectRoot / p;
    if (std::filesystem::exists(p)) return p;
    std::filesystem::path fallback = state.projectRoot / "assets/Scene1.glb";
    if (std::filesystem::exists(fallback)) return fallback;
    return p;
}

static void addObject(EditorState& state, const char* type, const char* source = "") {
    EditorObject obj;
    obj.name = std::string(type) + "_" + std::to_string(state.hierarchy.size());
    obj.type = type;
    obj.sourcePath = source;
    obj.position[0] = static_cast<float>(state.hierarchy.size() % 5) * 2.0f - 4.0f;
    obj.position[2] = static_cast<float>(state.hierarchy.size() / 5) * 2.0f + 4.0f;
    addDefaultComponents(obj);
    state.hierarchy.push_back(obj);
    selectSingle(state, static_cast<int>(state.hierarchy.size()) - 1);
    state.log("Scene added object: " + obj.name);
}

struct EffectiveTransform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotationY = 0.0f;
    float scale[3] = {1.0f, 1.0f, 1.0f};
};

// Resolves the world transform an object should actually render at, given
// its own edits AND its parent chain's edits. Composition rule per link:
// child's effective position = parent's effective position
//     + (child's baseline offset from parent's baseline, rotated/scaled by
//        however much the parent chain has moved since baseline)
//     + child's own edit delta from its own baseline.
// Rotation only tracks yaw (matches the rest of this tool - GLB node
// rotation was always decomposed to yaw only). `visiting` guards against a
// bad/cyclic parentIndex (shouldn't happen given the UI prevents it, but a
// stale/hand-edited scene could still produce one).
static EffectiveTransform computeEffectiveTransform(const EditorState& state, int index, std::vector<int>& visiting) {
    const EditorObject& obj = state.hierarchy[index];
    float ownDeltaPos[3] = { obj.position[0]-obj.basePosition[0], obj.position[1]-obj.basePosition[1], obj.position[2]-obj.basePosition[2] };
    float ownDeltaYaw = obj.rotation[1] - obj.baseRotation[1];
    float ownScaleRatio[3] = {
        obj.baseScale[0] > 0.0001f ? obj.scale[0] / obj.baseScale[0] : 1.0f,
        obj.baseScale[1] > 0.0001f ? obj.scale[1] / obj.baseScale[1] : 1.0f,
        obj.baseScale[2] > 0.0001f ? obj.scale[2] / obj.baseScale[2] : 1.0f
    };

    bool hasValidParent = obj.parentIndex >= 0 && obj.parentIndex < (int)state.hierarchy.size()
        && std::find(visiting.begin(), visiting.end(), obj.parentIndex) == visiting.end();

    EffectiveTransform result;
    if (!hasValidParent) {
        result.position[0] = obj.basePosition[0] + ownDeltaPos[0];
        result.position[1] = obj.basePosition[1] + ownDeltaPos[1];
        result.position[2] = obj.basePosition[2] + ownDeltaPos[2];
        result.rotationY = obj.baseRotation[1] + ownDeltaYaw;
        result.scale[0] = obj.baseScale[0] * ownScaleRatio[0];
        result.scale[1] = obj.baseScale[1] * ownScaleRatio[1];
        result.scale[2] = obj.baseScale[2] * ownScaleRatio[2];
        return result;
    }

    visiting.push_back(index);
    EffectiveTransform parentEff = computeEffectiveTransform(state, obj.parentIndex, visiting);
    visiting.pop_back();

    const EditorObject& parent = state.hierarchy[obj.parentIndex];
    float localOffset[3] = {
        obj.basePosition[0] - parent.basePosition[0],
        obj.basePosition[1] - parent.basePosition[1],
        obj.basePosition[2] - parent.basePosition[2]
    };
    float deltaParentYaw = parentEff.rotationY - parent.baseRotation[1];
    float rad = deltaParentYaw * 3.14159265f / 180.0f;
    float cosv = std::cos(rad), sinv = std::sin(rad);
    float rotatedOffset[3] = {
        localOffset[0]*cosv + localOffset[2]*sinv,
        localOffset[1],
        -localOffset[0]*sinv + localOffset[2]*cosv
    };
    float parentScaleRatio[3] = {
        parent.baseScale[0] > 0.0001f ? parentEff.scale[0] / parent.baseScale[0] : 1.0f,
        parent.baseScale[1] > 0.0001f ? parentEff.scale[1] / parent.baseScale[1] : 1.0f,
        parent.baseScale[2] > 0.0001f ? parentEff.scale[2] / parent.baseScale[2] : 1.0f
    };

    result.position[0] = parentEff.position[0] + rotatedOffset[0]*parentScaleRatio[0] + ownDeltaPos[0];
    result.position[1] = parentEff.position[1] + rotatedOffset[1]*parentScaleRatio[1] + ownDeltaPos[1];
    result.position[2] = parentEff.position[2] + rotatedOffset[2]*parentScaleRatio[2] + ownDeltaPos[2];
    result.rotationY = obj.baseRotation[1] + deltaParentYaw + ownDeltaYaw;
    result.scale[0] = obj.baseScale[0] * ownScaleRatio[0] * parentScaleRatio[0];
    result.scale[1] = obj.baseScale[1] * ownScaleRatio[1] * parentScaleRatio[1];
    result.scale[2] = obj.baseScale[2] * ownScaleRatio[2] * parentScaleRatio[2];
    return result;
}

static void syncHierarchyFromPreviewNodes(EditorState& state, const EditorPreviewRenderer& renderer, const std::filesystem::path& scenePath, bool force = false) {
    if (!state.sceneUseLoadedGlbNodesAsHierarchy || !renderer.loaded()) return;
    if (!force && state.sceneMarkerSourceGlb == scenePath.string()) return;

    state.sceneMarkerSourceGlb = scenePath.string();

    // The browse catalog is pure read-only convenience data - always safe to
    // wholesale-replace, since AXSceneIO never saves it.
    state.sceneBrowseNodes.clear();
    state.selectedBrowseNodes.clear();

    const auto& nodes = renderer.previewNodes();
    int mergedMarkers = 0;
    for (const auto& node : nodes) {
        if (node.name.empty()) continue;
        if (state.sceneDrawGlbMarkersOnly && !node.isMarker) continue;

        EditorObject obj;
        obj.name = node.name;
        obj.type = node.type;
        obj.sourcePath = std::filesystem::relative(scenePath, state.projectRoot).string();
        obj.parentIndex = -1; // GLB-native parenting is separate from authored grouping
        obj.isStatic = node.hasMesh && !node.isMarker;
        obj.position[0] = node.position[0]; obj.position[1] = node.position[1]; obj.position[2] = node.position[2];
        obj.rotation[0] = node.rotation[0]; obj.rotation[1] = node.rotation[1]; obj.rotation[2] = node.rotation[2];
        obj.scale[0] = node.scale[0]; obj.scale[1] = node.scale[1]; obj.scale[2] = node.scale[2];
        obj.basePosition[0] = obj.position[0]; obj.basePosition[1] = obj.position[1]; obj.basePosition[2] = obj.position[2];
        obj.baseRotation[0] = obj.rotation[0]; obj.baseRotation[1] = obj.rotation[1]; obj.baseRotation[2] = obj.rotation[2];
        obj.baseScale[0] = obj.scale[0]; obj.baseScale[1] = obj.scale[1]; obj.baseScale[2] = obj.scale[2];
        if (lowerCopy(obj.name).find("collider") != std::string::npos) obj.type = "Collider";
        addDefaultComponents(obj);

        // Every node goes into the read-only browse catalog...
        state.sceneBrowseNodes.push_back(obj);

        // ...but Blender-authored markers (ENEMY_*, PLAYER_START, VFX_*) are
        // real gameplay objects, not just browsing data. Merge them into the
        // authored hierarchy (additive only - never clear/replace it here),
        // so they get saved, while leaving anything already authored
        // (StaticScene, manually-added objects, previous NodeGroup/NodeRef
        // groupings) completely untouched.
        if (node.isMarker) {
            bool alreadyAuthored = false;
            for (const auto& existing : state.hierarchy) {
                if (existing.name == node.name) { alreadyAuthored = true; break; }
            }
            if (!alreadyAuthored) {
                state.hierarchy.push_back(obj);
                ++mergedMarkers;
            }
        }
    }
    state.log("Scene browse catalog: " + std::to_string(state.sceneBrowseNodes.size()) + " GLB nodes ("
        + std::to_string(mergedMarkers) + " new marker object(s) merged into scene).");
}

// Groups every currently-selected browse-catalog node under one new,
// authored "NodeGroup" object. Nodes that already have an authored NodeRef
// (from an earlier grouping) get re-parented instead of duplicated. This is
// what turns "browsing a GLB" into real, savable, runtime-visible scene
// data: the group and its members are ordinary EditorObjects in
// state.hierarchy from this point on, so AXSceneIO writes them, and
// Environment::loadSceneGltf (once wired) can apply their transforms.
static void groupSelectedBrowseNodes(EditorState& state) {
    if (state.selectedBrowseNodes.empty()) return;

    EditorObject group;
    group.name = "Group_" + std::to_string(state.hierarchy.size());
    group.type = "NodeGroup";
    group.parentIndex = -1;
    addDefaultComponents(group);
    state.hierarchy.push_back(group);
    int groupIndex = static_cast<int>(state.hierarchy.size()) - 1;

    int newRefs = 0, reparented = 0;
    for (int browseIdx : state.selectedBrowseNodes) {
        if (browseIdx < 0 || browseIdx >= (int)state.sceneBrowseNodes.size()) continue;
        const EditorObject& node = state.sceneBrowseNodes[browseIdx];

        int existing = -1;
        for (int i = 0; i < (int)state.hierarchy.size(); ++i) {
            if (state.hierarchy[i].type == "NodeRef" && state.hierarchy[i].name == node.name) { existing = i; break; }
        }
        if (existing >= 0) {
            state.hierarchy[existing].parentIndex = groupIndex;
            ++reparented;
        } else {
            EditorObject ref = node; // carries name/sourcePath/baseline exactly as baked in the GLB
            ref.type = "NodeRef";
            ref.parentIndex = groupIndex;
            state.hierarchy.push_back(ref);
            ++newRefs;
        }
    }
    state.sceneDirty = true;
    selectSingle(state, groupIndex);
    state.log("Grouped " + std::to_string(newRefs + reparented) + " node(s) into " + group.name
        + " (" + std::to_string(newRefs) + " new, " + std::to_string(reparented) + " re-parented).");
}

static void drawObjectDetails(EditorState& state, EditorObject& obj) {
    ImGui::TextDisabled("Details / Actor");
    char nameBuf[256] = {};
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", obj.name.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) obj.name = nameBuf;

    char typeBuf[128] = {};
    std::snprintf(typeBuf, sizeof(typeBuf), "%s", obj.type.c_str());
    if (ImGui::InputText("Type", typeBuf, sizeof(typeBuf))) obj.type = typeBuf;

    ImGui::Checkbox("Active", &obj.active);
    ImGui::SameLine();
    ImGui::Checkbox("Static", &obj.isStatic);
    ImGui::TextWrapped("Source: %s", obj.sourcePath.c_str());

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Location", obj.position, state.snapEnabled ? state.snapStep : 0.05f);
        ImGui::DragFloat3("Rotation", obj.rotation, 0.25f);
        ImGui::DragFloat3("Scale", obj.scale, 0.01f, 0.001f, 100.0f);
        if (ImGui::Button("Reset Location")) obj.position[0]=obj.position[1]=obj.position[2]=0.0f;
        ImGui::SameLine();
        if (ImGui::Button("Reset Rotation")) obj.rotation[0]=obj.rotation[1]=obj.rotation[2]=0.0f;
        ImGui::SameLine();
        if (ImGui::Button("Reset Scale")) obj.scale[0]=obj.scale[1]=obj.scale[2]=1.0f;
    }

    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("Add Component", "Choose...")) {
            const char* types[] = {"StaticMeshRenderer", "SkinnedMeshRenderer", "Camera", "DirectionalLight", "BoxCollider", "MeshCollider", "EnemyAI", "GunnerAim", "ProjectileEmitter", "ScriptComponent"};
            for (const char* t : types) {
                if (ImGui::Selectable(t)) obj.components.push_back(makeComponent(t));
            }
            ImGui::EndCombo();
        }
        for (int i = 0; i < (int)obj.components.size(); ++i) {
            EditorComponent& c = obj.components[i];
            ImGui::PushID(i);
            if (ImGui::TreeNodeEx(c.type.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Active", &c.active);
                ImGui::SameLine();
                ImGui::Checkbox("Dynamic", &c.dynamic);
                for (auto& prop : c.properties) {
                    char buf[256] = {};
                    std::snprintf(buf, sizeof(buf), "%s", prop.value.c_str());
                    if (ImGui::InputText(prop.name.c_str(), buf, sizeof(buf))) prop.value = buf;
                }
                if (i > 0 && ImGui::Button("Remove Component")) {
                    obj.components.erase(obj.components.begin() + i);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    if (state.detailsShowGeneratedCppPreview && ImGui::CollapsingHeader("Generated C++ Preview")) {
        const std::string cppPreview =
            std::string("SceneObject obj;\n") +
            "obj.name = \"" + obj.name + "\";\n" +
            "obj.type = \"" + obj.type + "\";\n" +
            "obj.sourcePath = \"" + obj.sourcePath + "\";\n" +
            "obj.position = { " + std::to_string(obj.position[0]) + "f, " + std::to_string(obj.position[1]) + "f, " + std::to_string(obj.position[2]) + "f };\n" +
            "obj.rotation = { " + std::to_string(obj.rotation[0]) + "f, " + std::to_string(obj.rotation[1]) + "f, " + std::to_string(obj.rotation[2]) + "f };\n" +
            "obj.scale = { " + std::to_string(obj.scale[0]) + "f, " + std::to_string(obj.scale[1]) + "f, " + std::to_string(obj.scale[2]) + "f };";
        ImGui::TextUnformatted(cppPreview.c_str());
    }
}

} // namespace

void drawSceneViewPanel(EditorState& state) {
    static EditorPreviewRenderer sceneRenderer;
    static std::filesystem::path loadedPath;
    static std::string lastError;

    AXEditorLayout::placeSceneView(state);
    ImGui::Begin("Scene Preview", &state.showSceneView, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Collections")) {
            ImGui::TextDisabled("GLB visibility filters");
            ImGui::Checkbox("OBJECT_* / normal nodes", &state.sceneShowCollectionObjects);
            ImGui::Checkbox("COLLIDER_*", &state.sceneShowCollectionColliders);
            ImGui::Checkbox("MAT_*", &state.sceneShowCollectionMat);
            ImGui::Checkbox("BLOOM_* / EMISSIVE / GLOW", &state.sceneShowCollectionBloom);
            ImGui::Separator();
            if (ImGui::MenuItem("Show gameplay-safe view")) {
                state.sceneShowCollectionObjects = true;
                state.sceneShowCollectionColliders = false;
                state.sceneShowCollectionMat = true;
                state.sceneShowCollectionBloom = true;
            }
            if (ImGui::MenuItem("Show collision authoring view")) {
                state.sceneShowCollectionObjects = false;
                state.sceneShowCollectionColliders = true;
                state.sceneShowCollectionMat = false;
                state.sceneShowCollectionBloom = false;
            }
            if (ImGui::MenuItem("Show everything")) {
                state.sceneShowCollectionObjects = true;
                state.sceneShowCollectionColliders = true;
                state.sceneShowCollectionMat = true;
                state.sceneShowCollectionBloom = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Viewport")) {
            ImGui::Checkbox("Node overlay", &state.sceneShowObjects);
            ImGui::Checkbox("Selection outline", &state.sceneShowSelectionOutline);
            ImGui::Checkbox("Transform arrows", &state.sceneShowAxisGizmo);
            ImGui::Checkbox("Sky", &state.sceneShowSky);
            ImGui::Checkbox("Grid", &state.showGrid);
            ImGui::DragFloat("Fly speed", &state.sceneFlySpeed, 0.2f, 0.1f, 200.0f);
            if (ImGui::MenuItem("Frame Camera Position")) {
                state.sceneCameraDistance = sceneRenderer.loaded() ? sceneRenderer.suggestedCameraDistance() : 80.0f;
                sceneRenderer.framedCameraPosition(state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance, state.sceneFrameRealScene, state.sceneCameraFocus);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty")) addObject(state, "Empty");
            if (ImGui::MenuItem("Enemy")) addObject(state, "Enemy", "assets/enemies/Gunner.glb");
            if (ImGui::MenuItem("Camera")) addObject(state, "Camera");
            if (ImGui::MenuItem("Light")) addObject(state, "Light");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (state.sceneAutoOpenSelectedSceneGlb && !state.selectedPath.empty() && isGlbPath(state.selectedPath) && looksLikeSceneGlb(state.selectedPath)) {
        std::string rel = std::filesystem::relative(state.selectedPath, state.projectRoot).string();
        if (state.sceneGlbPath != rel) {
            state.sceneGlbPath = rel;
            loadedPath.clear();
            state.sceneMarkerSourceGlb.clear();
            state.log("Scene Preview auto-opened selected scene GLB: " + rel);
        }
    }

    std::filesystem::path scenePath = resolveScenePath(state);

    if (ImGui::Button("Open Selected As Scene") && !state.selectedPath.empty() && isGlbPath(state.selectedPath)) {
        state.sceneGlbPath = std::filesystem::relative(state.selectedPath, state.projectRoot).string();
        loadedPath.clear();
        state.sceneMarkerSourceGlb.clear();
        state.log("Scene Preview target set to: " + state.sceneGlbPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload GLB")) { loadedPath.clear(); state.sceneMarkerSourceGlb.clear(); }
    ImGui::SameLine();
    if (ImGui::Button("Rebuild Hierarchy From GLB")) syncHierarchyFromPreviewNodes(state, sceneRenderer, scenePath, true);
    ImGui::SameLine();
    if (ImGui::Button("+ Empty")) addObject(state, "Empty");
    ImGui::SameLine();
    if (ImGui::Button("+ Enemy")) addObject(state, "Enemy", "assets/enemies/Gunner.glb");

    drawTransformToolButtons(state);
    ImGui::SameLine(); ImGui::Checkbox("Real Render", &state.sceneUseRealRender);
    ImGui::SameLine(); ImGui::Checkbox("Node Overlay", &state.sceneShowObjects);
    ImGui::SameLine(); ImGui::Checkbox("Hierarchy From GLB", &state.sceneUseLoadedGlbNodesAsHierarchy);
    ImGui::SameLine(); ImGui::Checkbox("Markers Only", &state.sceneDrawGlbMarkersOnly);
    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float sideWidth = 360.0f;
    ImVec2 canvasSize(std::max(220.0f, avail.x - sideWidth - 8.0f), std::max(220.0f, avail.y));
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 q(p.x + canvasSize.x, p.y + canvasSize.y);

    if (state.sceneUseRealRender && std::filesystem::exists(scenePath)) {
        if (loadedPath != scenePath || !sceneRenderer.loaded()) {
            if (sceneRenderer.loadGlb(scenePath, &lastError)) {
                loadedPath = scenePath;
                state.sceneCameraDistance = sceneRenderer.suggestedCameraDistance();
                sceneRenderer.framedCameraPosition(state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance, state.sceneFrameRealScene, state.sceneCameraFocus);
                state.log("Scene Preview loaded GLB: " + scenePath.string());
                syncHierarchyFromPreviewNodes(state, sceneRenderer, scenePath, true);
            } else {
                loadedPath.clear();
                state.log("Scene Preview failed GLB load: " + lastError);
            }
        }
        // Push live Inspector edits into the renderer every frame, composed
        // through each object's parent chain so moving/rotating/scaling a
        // parent drags its children along. Nodes that haven't been touched
        // (and have no edited ancestor) resolve to a zero delta inside
        // drawModel(), so this is cheap and doesn't require dirty-tracking.
        for (int i = 0; i < (int)state.hierarchy.size(); ++i) {
            std::vector<int> visiting;
            EffectiveTransform eff = computeEffectiveTransform(state, i, visiting);
            float rot[3] = { state.hierarchy[i].rotation[0], eff.rotationY, state.hierarchy[i].rotation[2] };
            sceneRenderer.setInstanceTransform(state.hierarchy[i].name, eff.position, rot, eff.scale);
        }

        unsigned int tex = sceneRenderer.renderToTexture((int)canvasSize.x, (int)canvasSize.y,
            state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance,
            state.showGrid, state.sceneShowSky, false, state.sceneFrameRealScene,
            state.sceneCameraFocus,
            state.sceneShowCollectionObjects,
            state.sceneShowCollectionColliders,
            state.sceneShowCollectionMat,
            state.sceneShowCollectionBloom);
        ImGui::Image((ImTextureID)(intptr_t)tex, canvasSize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, q, IM_COL32(17, 18, 23, 255));
        // Empty scene is valid. Use Project Browser > Import GLB To Scene to add visible assets.
        ImGui::InvisibleButton("ScenePreviewFallbackCanvas", canvasSize);
    }

    const bool canvasHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGuiIO& io = ImGui::GetIO();

    // Camera navigation should be anchored to the viewport, not to ImGui item activation.
    // ImGui::Image does not reliably become "active" for RMB/MMB drags, so capture
    // the viewport while the user holds RMB/MMB over it, like Unity/Unreal viewports.
    static bool sceneViewportCaptured = false;
    if (canvasHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                          ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
        sceneViewportCaptured = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        sceneViewportCaptured = false;
    }

    if (canvasHovered && io.MouseWheel != 0.0f) {
        float dolly = io.MouseWheel * std::max(0.25f, state.sceneCameraDistance * 0.08f);
        dollyCamera(state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraFocus, dolly);
        state.sceneCameraDistance = std::clamp(state.sceneCameraDistance - dolly, 0.8f, 2000.0f);
    }

    const bool rotatingViewport = sceneViewportCaptured &&
        (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle));

    if (rotatingViewport) {
        state.sceneCameraYaw += io.MouseDelta.x * 0.25f;
        state.sceneCameraPitch = std::clamp(state.sceneCameraPitch - io.MouseDelta.y * 0.20f, -80.0f, 85.0f);
    }

    // WASD/QE works while hovered or while RMB/MMB is captured.
    updateFlyCamera(state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraFocus, state.sceneFlySpeed,
        canvasHovered || sceneViewportCaptured);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(p, q, IM_COL32(75, 80, 95, 255));
    // Keep the viewport clean: help text lives in side panels/menus, not over the render canvas.

    if (state.sceneShowObjects && sceneRenderer.loaded()) {
        for (int i = 0; i < (int)state.hierarchy.size(); ++i) {
            EditorObject& obj = state.hierarchy[i];
            if (obj.type == "NodeGroup") continue; // pure hierarchy anchor, nothing to click on in-world
            float sx=0, sy=0;
            if (!sceneRenderer.worldToScreen(obj.position[0], obj.position[1], obj.position[2],
                    (int)canvasSize.x, (int)canvasSize.y,
                    state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance,
                    state.sceneFrameRealScene, sx, sy, state.sceneCameraFocus)) continue;
            ImVec2 sp(p.x + sx, p.y + sy);
            bool selected = isSelected(state, i);
            ImU32 col = selected ? IM_COL32(255,210,80,255) : (obj.type == "Enemy" ? IM_COL32(255,80,80,230) : IM_COL32(80,170,255,220));
            dl->AddCircleFilled(sp, selected ? 6.5f : 4.5f, col, 18);
            if (selected && state.sceneShowSelectionOutline) {
                dl->AddCircle(sp, 12.0f, IM_COL32(255,255,255,230), 24, 2.0f);
                dl->AddCircle(sp, 18.0f, IM_COL32(255,210,80,180), 32, 1.5f);
            }
            if (selected && state.sceneShowAxisGizmo) {
                float gx=0, gy=0, bx=0, by=0, ux=0, uy=0;
                sceneRenderer.worldToScreen(obj.position[0] + 1.5f, obj.position[1], obj.position[2], (int)canvasSize.x, (int)canvasSize.y, state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance, state.sceneFrameRealScene, gx, gy, state.sceneCameraFocus);
                sceneRenderer.worldToScreen(obj.position[0], obj.position[1] + 1.5f, obj.position[2], (int)canvasSize.x, (int)canvasSize.y, state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance, state.sceneFrameRealScene, ux, uy, state.sceneCameraFocus);
                sceneRenderer.worldToScreen(obj.position[0], obj.position[1], obj.position[2] + 1.5f, (int)canvasSize.x, (int)canvasSize.y, state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance, state.sceneFrameRealScene, bx, by, state.sceneCameraFocus);
                drawAxisGizmo2D(dl, sp, ImVec2(p.x + gx, p.y + gy), ImVec2(p.x + ux, p.y + uy), ImVec2(p.x + bx, p.y + by));
            }
            if (selected || obj.type == "Enemy" || obj.type == "PlayerStart") {
                dl->AddText(ImVec2(sp.x + 8, sp.y - 8), IM_COL32(245,245,245,240), obj.name.c_str());
            }
            const float dx = io.MousePos.x - sp.x;
            const float dy = io.MousePos.y - sp.y;
            if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dx*dx + dy*dy < 100.0f) {
                static int viewportShiftAnchor = -1;
                if (io.KeyShift) rangeSelect(state, viewportShiftAnchor, i);
                else if (io.KeyCtrl) toggleSelect(state, i);
                else selectSingle(state, i);
                viewportShiftAnchor = state.selectedObject;
            }
        }
    }

    ImGui::SameLine();
    ImGui::BeginChild("SceneDetails", ImVec2(sideWidth, canvasSize.y), true);
    ImGui::Text("Scene Target");
    ImGui::TextWrapped("%s", scenePath.string().c_str());
    ImGui::Text("Nodes: %d", sceneRenderer.nodeCount());
    ImGui::Text("Primitives: %d", sceneRenderer.primitiveCount());
    ImGui::Text("Triangles: %d", sceneRenderer.triangleCount());
    ImGui::DragFloat("Yaw##scenecam", &state.sceneCameraYaw, 0.5f);
    ImGui::DragFloat("Pitch##scenecam", &state.sceneCameraPitch, 0.5f, -80.0f, 85.0f);
    float oldSceneDollyDistance = state.sceneCameraDistance;
    if (ImGui::DragFloat("Dolly Distance##scenecam", &state.sceneCameraDistance, 0.25f, 0.8f, 2000.0f)) {
        float delta = oldSceneDollyDistance - state.sceneCameraDistance;
        dollyCamera(state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraFocus, delta);
    }
    ImGui::DragFloat3("Camera Position", state.sceneCameraFocus, 0.1f);
    if (ImGui::Button("Frame Scene")) {
        state.sceneCameraDistance = sceneRenderer.loaded() ? sceneRenderer.suggestedCameraDistance() : 80.0f;
        state.sceneFrameRealScene = true;
        sceneRenderer.framedCameraPosition(state.sceneCameraYaw, state.sceneCameraPitch, state.sceneCameraDistance, state.sceneFrameRealScene, state.sceneCameraFocus);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Selection")) clearSelection(state);
    ImGui::Separator();

    ImGui::Text("GLB Nodes (browse only - not saved)");
    ImGui::TextDisabled("Ctrl/Shift-click to multi-select, then Group Selected.");
    ImGui::BeginChild("SceneBrowseList", ImVec2(0, 140), true);
    static int browseShiftAnchor = -1;
    for (int i = 0; i < (int)state.sceneBrowseNodes.size(); ++i) {
        const EditorObject& node = state.sceneBrowseNodes[i];
        bool selected = std::find(state.selectedBrowseNodes.begin(), state.selectedBrowseNodes.end(), i) != state.selectedBrowseNodes.end();
        std::string label = node.name + "  <" + node.type + ">";
        if (ImGui::Selectable(label.c_str(), selected)) {
            const ImGuiIO& browseIo = ImGui::GetIO();
            if (browseIo.KeyShift && browseShiftAnchor >= 0) {
                int a = std::min(browseShiftAnchor, i), b = std::max(browseShiftAnchor, i);
                for (int j = a; j <= b; ++j) {
                    if (std::find(state.selectedBrowseNodes.begin(), state.selectedBrowseNodes.end(), j) == state.selectedBrowseNodes.end())
                        state.selectedBrowseNodes.push_back(j);
                }
            } else if (browseIo.KeyCtrl) {
                auto it = std::find(state.selectedBrowseNodes.begin(), state.selectedBrowseNodes.end(), i);
                if (it != state.selectedBrowseNodes.end()) state.selectedBrowseNodes.erase(it);
                else state.selectedBrowseNodes.push_back(i);
            } else {
                state.selectedBrowseNodes.clear();
                state.selectedBrowseNodes.push_back(i);
            }
            browseShiftAnchor = i;
        }
    }
    ImGui::EndChild();
    if (ImGui::Button("Group Selected") && !state.selectedBrowseNodes.empty()) groupSelectedBrowseNodes(state);
    ImGui::SameLine();
    ImGui::TextDisabled("(%d selected)", (int)state.selectedBrowseNodes.size());
    ImGui::Separator();

    ImGui::Text("Scene Objects (authored - saved to .axscene)");
    ImGui::BeginChild("SceneNodeList", ImVec2(0, 160), true);
    for (int i = 0; i < (int)state.hierarchy.size(); ++i) {
        EditorObject& obj = state.hierarchy[i];
        bool selected = isSelected(state, i);
        std::string label = obj.name + "  <" + obj.type + ">";
        if (obj.parentIndex >= 0 && obj.parentIndex < (int)state.hierarchy.size()) {
            label += "  (in " + state.hierarchy[obj.parentIndex].name + ")";
        }
        if (ImGui::Selectable(label.c_str(), selected)) {
            const ImGuiIO& nodeListIo = ImGui::GetIO();
            if (nodeListIo.KeyCtrl) toggleSelect(state, i);
            else selectSingle(state, i);
        }
    }
    ImGui::EndChild();
    ImGui::Separator();

    if (state.selectedObject >= 0 && state.selectedObject < (int)state.hierarchy.size()) {
        drawObjectDetails(state, state.hierarchy[state.selectedObject]);
    } else {
        ImGui::TextDisabled("No object/node selected.");
        ImGui::TextWrapped("Open a scene GLB, rebuild from GLB nodes, then select a node from the viewport overlay or hierarchy.");
    }
    ImGui::EndChild();

    ImGui::End();
}
