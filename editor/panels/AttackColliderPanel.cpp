#include "AttackColliderPanel.h"
#include "PanelLayout.h"
#include "runtime/AXAttackColliderSystem.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <vector>

namespace {

struct EditableCollider {
    char name[96] = "MainSlash";
    int shape = 0;      // Box, Sphere, Capsule, Cylinder
    int direction = 0;  // Forward, Up, Down, Left, Right, Back, Custom
    float offset[3] = {0.0f, 1.1f, 1.8f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float size[3] = {3.2f, 1.4f, 2.0f};
    float radius = 1.0f;
    float height = 2.0f;
    float activeFrom = 0.14f;
    float activeTo = 0.26f;
    float damage = 18.0f;
    float hitstop = 0.05f;
    float knockback[3] = {0.0f, 1.2f, 5.0f};
    bool launch = false;
    bool knockdown = false;
    bool hitOncePerTarget = true;
};

static std::vector<EditableCollider> g_colliders;
static std::filesystem::path g_loadedPath;
static bool g_initialized = false;

static const char* kShapes[] = {"Box", "Sphere", "Capsule", "Cylinder"};
static const char* kDirections[] = {"Forward", "Up", "Down", "Left", "Right", "Back", "Custom"};

static int shapeIndex(ax::AttackColliderShape shape) {
    switch (shape) {
        case ax::AttackColliderShape::Sphere: return 1;
        case ax::AttackColliderShape::Capsule: return 2;
        case ax::AttackColliderShape::Cylinder: return 3;
        case ax::AttackColliderShape::Box: default: return 0;
    }
}

static int directionIndex(ax::AttackColliderDirection d) {
    switch (d) {
        case ax::AttackColliderDirection::Up: return 1;
        case ax::AttackColliderDirection::Down: return 2;
        case ax::AttackColliderDirection::Left: return 3;
        case ax::AttackColliderDirection::Right: return 4;
        case ax::AttackColliderDirection::Back: return 5;
        case ax::AttackColliderDirection::Custom: return 6;
        case ax::AttackColliderDirection::Forward: default: return 0;
    }
}

static ax::AttackColliderShape toShape(int i) {
    switch (i) {
        case 1: return ax::AttackColliderShape::Sphere;
        case 2: return ax::AttackColliderShape::Capsule;
        case 3: return ax::AttackColliderShape::Cylinder;
        case 0: default: return ax::AttackColliderShape::Box;
    }
}

static ax::AttackColliderDirection toDirection(int i) {
    switch (i) {
        case 1: return ax::AttackColliderDirection::Up;
        case 2: return ax::AttackColliderDirection::Down;
        case 3: return ax::AttackColliderDirection::Left;
        case 4: return ax::AttackColliderDirection::Right;
        case 5: return ax::AttackColliderDirection::Back;
        case 6: return ax::AttackColliderDirection::Custom;
        case 0: default: return ax::AttackColliderDirection::Forward;
    }
}

static std::filesystem::path absoluteAttackPath(const EditorState& state) {
    std::filesystem::path p = state.attackEditorPath;
    if (!p.is_absolute()) p = state.projectRoot / p;
    return p;
}

static void fromRuntime(const ax::AttackDefinition& attack, EditorState& state) {
    state.attackEditorName = attack.name;
    state.attackEditorDuration = attack.duration;
    g_colliders.clear();
    for (const auto& c : attack.colliders) {
        EditableCollider e;
        std::snprintf(e.name, sizeof(e.name), "%s", c.name.c_str());
        e.shape = shapeIndex(c.shape);
        e.direction = directionIndex(c.direction);
        e.offset[0] = c.offset.x; e.offset[1] = c.offset.y; e.offset[2] = c.offset.z;
        e.rotation[0] = c.rotation.x; e.rotation[1] = c.rotation.y; e.rotation[2] = c.rotation.z;
        e.size[0] = c.size.x; e.size[1] = c.size.y; e.size[2] = c.size.z;
        e.radius = c.radius;
        e.height = c.height;
        e.activeFrom = c.activeFrom;
        e.activeTo = c.activeTo;
        e.damage = c.damage;
        e.hitstop = c.hitstop;
        e.knockback[0] = c.knockback.x; e.knockback[1] = c.knockback.y; e.knockback[2] = c.knockback.z;
        e.launch = c.launch;
        e.knockdown = c.knockdown;
        e.hitOncePerTarget = c.hitOncePerTarget;
        g_colliders.push_back(e);
    }
    if (g_colliders.empty()) g_colliders.push_back(EditableCollider{});
    state.attackEditorSelectedCollider = std::clamp(state.attackEditorSelectedCollider, 0, (int)g_colliders.size() - 1);
}

static ax::AttackDefinition toRuntime(const EditorState& state) {
    ax::AttackDefinition attack;
    attack.name = state.attackEditorName;
    attack.duration = state.attackEditorDuration;
    for (const auto& e : g_colliders) {
        ax::AttackColliderVolume c;
        c.name = e.name;
        c.shape = toShape(e.shape);
        c.direction = toDirection(e.direction);
        c.offset = {e.offset[0], e.offset[1], e.offset[2]};
        c.rotation = {e.rotation[0], e.rotation[1], e.rotation[2]};
        c.size = {e.size[0], e.size[1], e.size[2]};
        c.radius = e.radius;
        c.height = e.height;
        c.activeFrom = e.activeFrom;
        c.activeTo = e.activeTo;
        c.damage = e.damage;
        c.hitstop = e.hitstop;
        c.knockback = {e.knockback[0], e.knockback[1], e.knockback[2]};
        c.launch = e.launch;
        c.knockdown = e.knockdown;
        c.hitOncePerTarget = e.hitOncePerTarget;
        attack.colliders.push_back(c);
    }
    return attack;
}

static void loadAttack(EditorState& state) {
    ax::AttackColliderSystem system;
    ax::AttackDefinition attack;
    std::string error;
    auto path = absoluteAttackPath(state);
    if (system.loadAttackFile(path.string(), attack, &error)) {
        fromRuntime(attack, state);
        g_loadedPath = state.attackEditorPath;
        state.attackEditorDirty = false;
        state.attackEditorStatus = "Loaded " + state.attackEditorPath.string();
        state.log("Attack Collider Editor loaded: " + state.attackEditorPath.string());
        return;
    }
    g_colliders.clear();
    g_colliders.push_back(EditableCollider{});
    state.attackEditorName = path.stem().string().empty() ? "NewAttack" : path.stem().string();
    state.attackEditorDuration = 0.62f;
    state.attackEditorDirty = true;
    state.attackEditorStatus = error + " - created unsaved default.";
    state.log(state.attackEditorStatus);
}

static void saveAttack(EditorState& state) {
    ax::AttackColliderSystem system;
    std::string error;
    auto path = absoluteAttackPath(state);
    std::filesystem::create_directories(path.parent_path());
    if (system.saveAttackFile(path.string(), toRuntime(state), &error)) {
        state.attackEditorDirty = false;
        state.attackEditorStatus = "Saved " + state.attackEditorPath.string();
        state.log("Saved attack collider asset: " + state.attackEditorPath.string());
    } else {
        state.attackEditorStatus = error;
        state.log(error);
    }
}

static void ensureLoaded(EditorState& state) {
    if (!g_initialized || g_loadedPath != state.attackEditorPath) {
        g_initialized = true;
        loadAttack(state);
    }
}

static void drawTimeline(EditorState& state) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = std::max(240.0f, ImGui::GetContentRegionAvail().x);
    float h = 64.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(30, 34, 42, 255));
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(90, 100, 118, 255));
    const float dur = std::max(0.01f, state.attackEditorDuration);
    for (size_t i = 0; i < g_colliders.size(); ++i) {
        const auto& c = g_colliders[i];
        float x0 = p.x + std::clamp(c.activeFrom / dur, 0.0f, 1.0f) * w;
        float x1 = p.x + std::clamp(c.activeTo / dur, 0.0f, 1.0f) * w;
        float y0 = p.y + 10.0f + (float)(i % 3) * 14.0f;
        ImU32 col = (int)i == state.attackEditorSelectedCollider ? IM_COL32(255, 185, 70, 230) : IM_COL32(210, 95, 65, 190);
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(std::max(x0 + 2.0f, x1), y0 + 10.0f), col, 2.0f);
    }
    float sx = p.x + std::clamp(state.attackEditorScrubTime / dur, 0.0f, 1.0f) * w;
    dl->AddLine(ImVec2(sx, p.y), ImVec2(sx, p.y + h), IM_COL32(120, 210, 255, 255), 2.0f);
    ImGui::InvisibleButton("##attackTimeline", ImVec2(w, h));
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        float t = (ImGui::GetIO().MousePos.x - p.x) / w;
        state.attackEditorScrubTime = std::clamp(t, 0.0f, 1.0f) * dur;
    }
}


struct PreviewVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static PreviewVec3 pv(float x, float y, float z) { return {x, y, z}; }
static PreviewVec3 add(PreviewVec3 a, PreviewVec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static PreviewVec3 sub(PreviewVec3 a, PreviewVec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static PreviewVec3 mul(PreviewVec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float dot(PreviewVec3 a, PreviewVec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static PreviewVec3 cross(PreviewVec3 a, PreviewVec3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
static float length(PreviewVec3 a) { return std::sqrt(std::max(0.000001f, dot(a, a))); }
static PreviewVec3 normalize(PreviewVec3 a) { return mul(a, 1.0f / length(a)); }
static float radians(float deg) { return deg * 0.01745329251994329577f; }

static PreviewVec3 rotateEuler(PreviewVec3 p, const float rotDeg[3]) {
    float cx = std::cos(radians(rotDeg[0])), sx = std::sin(radians(rotDeg[0]));
    float cy = std::cos(radians(rotDeg[1])), sy = std::sin(radians(rotDeg[1]));
    float cz = std::cos(radians(rotDeg[2])), sz = std::sin(radians(rotDeg[2]));
    // X then Y then Z, matching a simple authoring preview rather than engine matrix order.
    PreviewVec3 v = {p.x, p.y * cx - p.z * sx, p.y * sx + p.z * cx};
    v = {v.x * cy + v.z * sy, v.y, -v.x * sy + v.z * cy};
    v = {v.x * cz - v.y * sz, v.x * sz + v.y * cz, v.z};
    return v;
}

struct PreviewCamera3D {
    ImVec2 origin;
    ImVec2 size;
    PreviewVec3 pos;
    PreviewVec3 target;
    PreviewVec3 right;
    PreviewVec3 up;
    PreviewVec3 forward;
    float focal = 420.0f;
};

static bool projectPoint(const PreviewCamera3D& cam, PreviewVec3 world, ImVec2& out) {
    PreviewVec3 v = sub(world, cam.pos);
    float z = dot(v, cam.forward);
    if (z < 0.05f) return false;
    float x = dot(v, cam.right);
    float y = dot(v, cam.up);
    ImVec2 center(cam.origin.x + cam.size.x * 0.5f, cam.origin.y + cam.size.y * 0.53f);
    out.x = center.x + (x / z) * cam.focal;
    out.y = center.y - (y / z) * cam.focal;
    return true;
}

static void drawLine3D(ImDrawList* dl, const PreviewCamera3D& cam, PreviewVec3 a, PreviewVec3 b, ImU32 color, float thickness = 1.5f) {
    ImVec2 pa, pb;
    if (projectPoint(cam, a, pa) && projectPoint(cam, b, pb)) dl->AddLine(pa, pb, color, thickness);
}

static void drawPolyline3D(ImDrawList* dl, const PreviewCamera3D& cam, const std::vector<PreviewVec3>& pts, ImU32 color, float thickness = 1.5f, bool closed = true) {
    if (pts.size() < 2) return;
    for (size_t i = 0; i + 1 < pts.size(); ++i) drawLine3D(dl, cam, pts[i], pts[i + 1], color, thickness);
    if (closed) drawLine3D(dl, cam, pts.back(), pts.front(), color, thickness);
}

static PreviewVec3 colliderLocalToWorld(const EditableCollider& c, PreviewVec3 local) {
    return add(pv(c.offset[0], c.offset[1], c.offset[2]), rotateEuler(local, c.rotation));
}

static void drawGrid3D(ImDrawList* dl, const PreviewCamera3D& cam) {
    ImU32 minor = IM_COL32(70, 76, 88, 100);
    ImU32 major = IM_COL32(100, 110, 130, 150);
    for (int i = -10; i <= 10; ++i) {
        ImU32 col = (i == 0) ? major : minor;
        drawLine3D(dl, cam, pv((float)i, 0.0f, -10.0f), pv((float)i, 0.0f, 10.0f), col, i == 0 ? 2.0f : 1.0f);
        drawLine3D(dl, cam, pv(-10.0f, 0.0f, (float)i), pv(10.0f, 0.0f, (float)i), col, i == 0 ? 2.0f : 1.0f);
    }
    drawLine3D(dl, cam, pv(0, 0, 0), pv(2.5f, 0, 0), IM_COL32(255, 90, 90, 230), 2.0f);
    drawLine3D(dl, cam, pv(0, 0, 0), pv(0, 2.5f, 0), IM_COL32(90, 255, 120, 230), 2.0f);
    drawLine3D(dl, cam, pv(0, 0, 0), pv(0, 0, 3.2f), IM_COL32(90, 160, 255, 250), 3.0f);
}

static void drawPlayer3D(ImDrawList* dl, const PreviewCamera3D& cam) {
    // Simple player reference: feet, body, facing direction (+Z), shoulder width.
    ImU32 body = IM_COL32(105, 175, 255, 230);
    ImU32 ghost = IM_COL32(105, 175, 255, 90);
    drawLine3D(dl, cam, pv(-0.35f, 0.0f, 0.0f), pv(0.35f, 0.0f, 0.0f), body, 2.0f);
    drawLine3D(dl, cam, pv(0.0f, 0.0f, 0.0f), pv(0.0f, 1.75f, 0.0f), body, 3.0f);
    drawLine3D(dl, cam, pv(-0.65f, 1.15f, 0.0f), pv(0.65f, 1.15f, 0.0f), ghost, 2.0f);
    drawLine3D(dl, cam, pv(0.0f, 1.05f, 0.0f), pv(0.0f, 1.05f, 2.2f), IM_COL32(90, 160, 255, 255), 3.0f);
    drawLine3D(dl, cam, pv(0.0f, 1.05f, 2.2f), pv(-0.28f, 1.05f, 1.82f), IM_COL32(90, 160, 255, 255), 2.0f);
    drawLine3D(dl, cam, pv(0.0f, 1.05f, 2.2f), pv(0.28f, 1.05f, 1.82f), IM_COL32(90, 160, 255, 255), 2.0f);
}

static void drawBoxCollider3D(ImDrawList* dl, const PreviewCamera3D& cam, const EditableCollider& c, ImU32 edge, float thickness) {
    float hx = c.size[0] * 0.5f, hy = c.size[1] * 0.5f, hz = c.size[2] * 0.5f;
    PreviewVec3 pts[8] = {
        colliderLocalToWorld(c, pv(-hx,-hy,-hz)), colliderLocalToWorld(c, pv( hx,-hy,-hz)),
        colliderLocalToWorld(c, pv( hx, hy,-hz)), colliderLocalToWorld(c, pv(-hx, hy,-hz)),
        colliderLocalToWorld(c, pv(-hx,-hy, hz)), colliderLocalToWorld(c, pv( hx,-hy, hz)),
        colliderLocalToWorld(c, pv( hx, hy, hz)), colliderLocalToWorld(c, pv(-hx, hy, hz))
    };
    int edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for (auto& e : edges) drawLine3D(dl, cam, pts[e[0]], pts[e[1]], edge, thickness);
}

static std::vector<PreviewVec3> makeCircle(const EditableCollider& c, PreviewVec3 center, PreviewVec3 axisA, PreviewVec3 axisB, float radius, int steps = 48) {
    std::vector<PreviewVec3> pts;
    pts.reserve((size_t)steps);
    for (int i = 0; i < steps; ++i) {
        float a = (float)i / (float)steps * 6.28318530718f;
        PreviewVec3 local = add(center, add(mul(axisA, std::cos(a) * radius), mul(axisB, std::sin(a) * radius)));
        pts.push_back(colliderLocalToWorld(c, local));
    }
    return pts;
}

static void drawSphereCollider3D(ImDrawList* dl, const PreviewCamera3D& cam, const EditableCollider& c, ImU32 edge, float thickness) {
    float r = c.radius;
    drawPolyline3D(dl, cam, makeCircle(c, pv(0,0,0), pv(1,0,0), pv(0,1,0), r), edge, thickness);
    drawPolyline3D(dl, cam, makeCircle(c, pv(0,0,0), pv(1,0,0), pv(0,0,1), r), edge, thickness);
    drawPolyline3D(dl, cam, makeCircle(c, pv(0,0,0), pv(0,1,0), pv(0,0,1), r), edge, thickness);
}

static void drawCylinderCollider3D(ImDrawList* dl, const PreviewCamera3D& cam, const EditableCollider& c, ImU32 edge, float thickness, bool capsule) {
    float r = c.radius;
    float hh = c.height * 0.5f;
    drawPolyline3D(dl, cam, makeCircle(c, pv(0, -hh, 0), pv(1,0,0), pv(0,0,1), r), edge, thickness);
    drawPolyline3D(dl, cam, makeCircle(c, pv(0,  hh, 0), pv(1,0,0), pv(0,0,1), r), edge, thickness);
    for (int i = 0; i < 8; ++i) {
        float a = (float)i / 8.0f * 6.28318530718f;
        float x = std::cos(a) * r, z = std::sin(a) * r;
        drawLine3D(dl, cam, colliderLocalToWorld(c, pv(x, -hh, z)), colliderLocalToWorld(c, pv(x, hh, z)), edge, thickness);
    }
    if (capsule) {
        drawPolyline3D(dl, cam, makeCircle(c, pv(0, -hh, 0), pv(1,0,0), pv(0,1,0), r), edge, thickness);
        drawPolyline3D(dl, cam, makeCircle(c, pv(0,  hh, 0), pv(1,0,0), pv(0,1,0), r), edge, thickness);
        drawPolyline3D(dl, cam, makeCircle(c, pv(0, -hh, 0), pv(0,0,1), pv(0,1,0), r), edge, thickness);
        drawPolyline3D(dl, cam, makeCircle(c, pv(0,  hh, 0), pv(0,0,1), pv(0,1,0), r), edge, thickness);
    }
}

static void drawCollider3D(ImDrawList* dl, const PreviewCamera3D& cam, const EditableCollider& c, ImU32 edge, float thickness) {
    if (c.shape == 1) drawSphereCollider3D(dl, cam, c, edge, thickness);
    else if (c.shape == 2) drawCylinderCollider3D(dl, cam, c, edge, thickness, true);
    else if (c.shape == 3) drawCylinderCollider3D(dl, cam, c, edge, thickness, false);
    else drawBoxCollider3D(dl, cam, c, edge, thickness);
}

static void drawVolumePreview(EditorState& state) {
    static float yaw = -35.0f;
    static float pitch = 24.0f;
    static float distance = 9.5f;
    static PreviewVec3 target = {0.0f, 1.0f, 1.5f};

    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = std::max(360.0f, ImGui::GetContentRegionAvail().x);
    const float availableH = ImGui::GetContentRegionAvail().y;
    float h = std::clamp(availableH * 0.52f, 220.0f, 430.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(23, 26, 32, 255));
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(95, 105, 125, 255));

    float cy = std::cos(radians(yaw)), sy = std::sin(radians(yaw));
    float cp = std::cos(radians(pitch)), sp = std::sin(radians(pitch));
    PreviewCamera3D cam;
    cam.origin = p;
    cam.size = ImVec2(w, h);
    cam.target = target;
    cam.pos = add(target, pv(sy * cp * distance, sp * distance, cy * cp * distance));
    cam.forward = normalize(sub(target, cam.pos));
    cam.right = normalize(cross(cam.forward, pv(0, 1, 0)));
    cam.up = normalize(cross(cam.right, cam.forward));
    cam.focal = std::min(w, h) * 1.15f;

    drawGrid3D(dl, cam);
    drawPlayer3D(dl, cam);

    for (size_t i = 0; i < g_colliders.size(); ++i) {
        const auto& c = g_colliders[i];
        bool active = state.attackEditorScrubTime >= c.activeFrom && state.attackEditorScrubTime <= c.activeTo;
        if (state.attackEditorPreviewActiveOnly && !active) continue;
        ImU32 edge = active ? IM_COL32(255, 105, 70, 255) : IM_COL32(255, 195, 80, 160);
        float thickness = active ? 2.6f : 1.6f;
        if ((int)i == state.attackEditorSelectedCollider) {
            edge = IM_COL32(90, 210, 255, 255);
            thickness = 3.0f;
        }
        drawCollider3D(dl, cam, c, edge, thickness);
        ImVec2 label;
        if (projectPoint(cam, pv(c.offset[0], c.offset[1], c.offset[2]), label)) {
            dl->AddCircleFilled(label, 4.0f, edge, 16);
            dl->AddText(ImVec2(label.x + 7.0f, label.y - 8.0f), edge, c.name);
        }
    }

    dl->AddText(ImVec2(p.x + 10.0f, p.y + 9.0f), IM_COL32(220, 230, 245, 230), "3D Attack Collision Preview");
    dl->AddText(ImVec2(p.x + 10.0f, p.y + 28.0f), IM_COL32(155, 165, 180, 230), "RMB/MMB drag: orbit  |  Wheel: zoom  |  blue arrow: player +Z attack forward");

    ImGui::InvisibleButton("##attackVolumePreview3D", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    if (hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle))) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        yaw += d.x * 0.35f;
        pitch += d.y * 0.25f;
        pitch = std::clamp(pitch, -80.0f, 80.0f);
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        distance *= (ImGui::GetIO().MouseWheel > 0.0f) ? 0.88f : 1.14f;
        distance = std::clamp(distance, 2.5f, 45.0f);
    }
    if (ImGui::BeginPopupContextItem("AttackPreview3DContext")) {
        if (ImGui::MenuItem("Reset 3D camera")) { yaw = -35.0f; pitch = 24.0f; distance = 9.5f; target = {0.0f, 1.0f, 1.5f}; }
        ImGui::EndPopup();
    }
}

static void createPreset(EditorState& state, const char* name, int shape, float ox, float oy, float oz, float sx, float sy, float sz, float start, float end) {
    g_colliders.clear();
    EditableCollider e;
    std::snprintf(e.name, sizeof(e.name), "%s", name);
    e.shape = shape;
    e.offset[0] = ox; e.offset[1] = oy; e.offset[2] = oz;
    e.size[0] = sx; e.size[1] = sy; e.size[2] = sz;
    e.radius = std::max(sx, sz) * 0.5f;
    e.height = sy;
    e.activeFrom = start;
    e.activeTo = end;
    g_colliders.push_back(e);
    state.attackEditorName = name;
    state.attackEditorSelectedCollider = 0;
    state.attackEditorScrubTime = start;
    state.attackEditorDirty = true;
}

} // namespace

void drawAttackColliderPanel(EditorState& state) {
    ensureLoaded(state);

    AXEditorLayout::placeAttackCollider(state);
    if (!ImGui::Begin("Attack Collider Editor", &state.showAttackColliderEditor,
                      AXEditorLayout::panelFlags(state, ImGuiWindowFlags_AlwaysVerticalScrollbar))) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("One authored attack volume per move. This is for designed hack-and-slash hit zones, not precise blade tracing.");
    ImGui::Separator();

    char pathBuf[512] = {};
    std::snprintf(pathBuf, sizeof(pathBuf), "%s", state.attackEditorPath.string().c_str());
    if (ImGui::InputText("Asset Path", pathBuf, sizeof(pathBuf))) {
        state.attackEditorPath = pathBuf;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) loadAttack(state);
    ImGui::SameLine();
    if (ImGui::Button("Save")) saveAttack(state);
    ImGui::SameLine();
    if (state.attackEditorDirty) ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "dirty");
    else ImGui::TextDisabled("saved");

    ImGui::InputText("Attack Name", &state.attackEditorName);
    if (ImGui::DragFloat("Duration", &state.attackEditorDuration, 0.005f, 0.01f, 10.0f, "%.3f s")) state.attackEditorDirty = true;
    if (ImGui::DragFloat("Scrub Time", &state.attackEditorScrubTime, 0.005f, 0.0f, state.attackEditorDuration, "%.3f s")) state.attackEditorDirty = true;
    ImGui::Checkbox("Preview active colliders only", &state.attackEditorPreviewActiveOnly);

    drawTimeline(state);
    drawVolumePreview(state);

    ImGui::Separator();
    const bool widePresetRow = ImGui::GetContentRegionAvail().x > 760.0f;
    if (ImGui::Button("Preset: Horizontal Slash")) createPreset(state, "SwordAttack1", 0, 0.0f, 1.1f, 1.8f, 3.2f, 1.4f, 2.0f, 0.14f, 0.26f);
    ImGui::SameLine();
    if (ImGui::Button("Preset: Thrust")) createPreset(state, "Stinger", 0, 0.0f, 1.0f, 3.2f, 0.9f, 1.0f, 5.8f, 0.18f, 0.36f);
    if (widePresetRow) ImGui::SameLine();
    if (ImGui::Button("Preset: Launcher")) createPreset(state, "HighTime", 0, 0.0f, 1.8f, 1.2f, 2.2f, 3.2f, 1.4f, 0.20f, 0.34f);
    ImGui::SameLine();
    if (ImGui::Button("Preset: Spin / Slam")) createPreset(state, "SpinSlam", 3, 0.0f, 1.0f, 0.0f, 4.2f, 1.4f, 4.2f, 0.25f, 0.48f);

    ImGui::Separator();
    ImGui::Text("Colliders");
    if (ImGui::Button("Add Collider")) {
        g_colliders.push_back(EditableCollider{});
        state.attackEditorSelectedCollider = (int)g_colliders.size() - 1;
        state.attackEditorDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove") && g_colliders.size() > 1) {
        int idx = std::clamp(state.attackEditorSelectedCollider, 0, (int)g_colliders.size() - 1);
        g_colliders.erase(g_colliders.begin() + idx);
        state.attackEditorSelectedCollider = std::clamp(idx, 0, (int)g_colliders.size() - 1);
        state.attackEditorDirty = true;
    }

    ImGui::BeginChild("AttackColliderList", ImVec2(170, 220), true);
    for (int i = 0; i < (int)g_colliders.size(); ++i) {
        if (ImGui::Selectable(g_colliders[i].name, state.attackEditorSelectedCollider == i)) state.attackEditorSelectedCollider = i;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("AttackColliderDetails", ImVec2(0, 220), true);
    if (!g_colliders.empty()) {
        int idx = std::clamp(state.attackEditorSelectedCollider, 0, (int)g_colliders.size() - 1);
        EditableCollider& c = g_colliders[idx];
        bool changed = false;
        changed |= ImGui::InputText("Name", c.name, sizeof(c.name));
        changed |= ImGui::Combo("Shape", &c.shape, kShapes, IM_ARRAYSIZE(kShapes));
        changed |= ImGui::Combo("Direction", &c.direction, kDirections, IM_ARRAYSIZE(kDirections));
        changed |= ImGui::DragFloat3("Offset", c.offset, 0.05f);
        changed |= ImGui::DragFloat3("Rotation", c.rotation, 0.25f);
        if (c.shape == 0) changed |= ImGui::DragFloat3("Size", c.size, 0.05f, 0.01f, 100.0f);
        if (c.shape == 1 || c.shape == 2 || c.shape == 3) changed |= ImGui::DragFloat("Radius", &c.radius, 0.02f, 0.01f, 100.0f);
        if (c.shape == 2 || c.shape == 3) changed |= ImGui::DragFloat("Height", &c.height, 0.05f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat("Active From", &c.activeFrom, 0.005f, 0.0f, state.attackEditorDuration, "%.3f s");
        changed |= ImGui::DragFloat("Active To", &c.activeTo, 0.005f, 0.0f, state.attackEditorDuration, "%.3f s");
        if (c.activeTo < c.activeFrom) c.activeTo = c.activeFrom;
        changed |= ImGui::DragFloat("Damage", &c.damage, 0.25f, 0.0f, 9999.0f);
        changed |= ImGui::DragFloat("Hitstop", &c.hitstop, 0.002f, 0.0f, 1.0f, "%.3f s");
        changed |= ImGui::DragFloat3("Knockback", c.knockback, 0.05f);
        changed |= ImGui::Checkbox("Launch", &c.launch);
        ImGui::SameLine(); changed |= ImGui::Checkbox("Knockdown", &c.knockdown);
        changed |= ImGui::Checkbox("Hit once per target", &c.hitOncePerTarget);
        if (changed) state.attackEditorDirty = true;
    }
    ImGui::EndChild();

    ImGui::TextWrapped("Runtime use: combat.requestAttack(name) -> AXAttackColliderSystem loads this .axattack, enables colliders during active frames, tests enemy Hurtbox components, then applies damage / knockback / hitstop.");
    ImGui::TextDisabled("%s", state.attackEditorStatus.c_str());
    ImGui::End();
}
