#include "AssetPreviewPanel.h"
#include "PanelLayout.h"
#include "preview/EditorPreviewRenderer.h"

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <cctype>
#include <cstdio>
#include <cmath>

namespace {

static std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static bool isGlbPath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".glb" || e == ".gltf";
}
static bool isImagePath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".webp";
}
static bool isCodePath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".cpp" || e == ".h" || e == ".hpp" || e == ".c" || e == ".cc";
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
            ImVec2 l(b.x - dx*9.0f - dy*4.0f, b.y - dy*9.0f + dx*4.0f);
            ImVec2 r(b.x - dx*9.0f + dy*4.0f, b.y - dy*9.0f - dx*4.0f);
            dl->AddTriangleFilled(b, l, r, col);
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
    float rightX = std::cos(yaw), rightZ = -std::sin(yaw);
    float s = speed * dt;
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) s *= 3.0f;
    if (ImGui::IsKeyDown(ImGuiKey_W)) { cameraPos[0] += fwdX * s; cameraPos[1] += fwdY * s; cameraPos[2] += fwdZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_S)) { cameraPos[0] -= fwdX * s; cameraPos[1] -= fwdY * s; cameraPos[2] -= fwdZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_A)) { cameraPos[0] -= rightX * s; cameraPos[2] -= rightZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_D)) { cameraPos[0] += rightX * s; cameraPos[2] += rightZ * s; }
    if (ImGui::IsKeyDown(ImGuiKey_Q)) cameraPos[1] -= s;
    if (ImGui::IsKeyDown(ImGuiKey_E)) cameraPos[1] += s;
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

static void drawSocketDiamond(ImDrawList* dl, const ImVec2& c, bool selected) {
    ImU32 fill = selected ? IM_COL32(255, 210, 80, 120) : IM_COL32(170, 80, 255, 100);
    ImU32 line = selected ? IM_COL32(255, 245, 190, 255) : IM_COL32(190, 120, 255, 230);
    float r = selected ? 11.0f : 8.0f;
    ImVec2 pts[4] = { ImVec2(c.x, c.y-r), ImVec2(c.x+r, c.y), ImVec2(c.x, c.y+r), ImVec2(c.x-r, c.y) };
    dl->AddConvexPolyFilled(pts, 4, fill);
    dl->AddPolyline(pts, 4, line, ImDrawFlags_Closed, selected ? 2.0f : 1.4f);
    dl->AddLine(ImVec2(c.x-r*0.65f,c.y), ImVec2(c.x+r*0.65f,c.y), line, 1.2f);
    dl->AddLine(ImVec2(c.x,c.y-r*0.65f), ImVec2(c.x,c.y+r*0.65f), line, 1.2f);
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

static std::filesystem::path resolveAssetPath(EditorState& state) {
    std::filesystem::path p(state.assetFilePath.empty() ? state.assetGlbPath : state.assetFilePath);
    if (!p.is_absolute()) p = state.projectRoot / p;
    if (std::filesystem::exists(p)) return p;
    std::filesystem::path fallback = state.projectRoot / "assets/Man1_Cinema.glb";
    if (std::filesystem::exists(fallback)) return fallback;
    return p;
}

static void ensureBoneTransforms(EditorState& state) {
    if (state.assetBoneTransforms.size() == state.assetPreviewBones.size()) return;
    state.assetBoneTransforms.resize(state.assetPreviewBones.size());
}

static const EditorPreviewRenderer::PreviewNode* findPreviewNodeByIndex(const EditorPreviewRenderer& renderer, int index) {
    for (const auto& n : renderer.previewNodes()) if (n.index == index) return &n;
    return nullptr;
}

} // namespace

void drawAssetPreviewPanel(EditorState& state) {
    static EditorPreviewRenderer assetRenderer;
    static std::filesystem::path loadedPath;
    static std::string lastError;

    ensureBoneTransforms(state);
    AXEditorLayout::placeAssetPreview(state);
    ImGui::Begin("Asset Preview", &state.showAssetPreview,
                 AXEditorLayout::panelFlags(state, ImGuiWindowFlags_MenuBar));

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Viewport")) {
            ImGui::Checkbox("Mesh", &state.assetShowMesh);
            ImGui::Checkbox("Skeleton", &state.assetShowSkeleton);
            ImGui::Checkbox("Sockets", &state.assetShowSockets);
            ImGui::Checkbox("Selection outline", &state.assetShowSelectionOutline);
            ImGui::Checkbox("Transform arrows", &state.assetShowAxisGizmo);
            ImGui::Checkbox("Sky", &state.assetShowSky);
            ImGui::DragFloat("Fly speed", &state.assetFlySpeed, 0.1f, 0.1f, 100.0f);
            if (ImGui::MenuItem("Frame Camera Position")) {
                state.assetCameraDistance = assetRenderer.loaded() ? assetRenderer.suggestedCameraDistance() : 6.5f;
                assetRenderer.framedCameraPosition(state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance, state.assetFrameModel, state.assetCameraFocus);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Asset Type")) {
            ImGui::Text("Current: %s", state.assetPreviewKind.c_str());
            ImGui::TextDisabled("GLB = rig/mesh explorer");
            ImGui::TextDisabled("Image = texture/material preview");
            ImGui::TextDisabled("Code = external editor");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (state.assetAutoOpenSelectedGlb && !state.selectedPath.empty() && !looksLikeSceneGlb(state.selectedPath) && (isGlbPath(state.selectedPath) || isImagePath(state.selectedPath))) {
        std::string rel = std::filesystem::relative(state.selectedPath, state.projectRoot).string();
        if (state.assetFilePath != rel) {
            state.assetFilePath = rel;
            if (isGlbPath(state.selectedPath)) { state.assetGlbPath = rel; state.assetPreviewKind = "GLB"; }
            else { state.assetPreviewKind = "Image"; }
            loadedPath.clear();
            state.assetSelectedPreviewNode = -1;
            state.log("Asset Preview auto-opened selected asset: " + rel);
        }
    }

    std::filesystem::path assetPath = resolveAssetPath(state);
    if (!state.selectedPath.empty()) {
        ImGui::TextWrapped("Selected: %s", state.selectedPath.filename().string().c_str());
    } else {
        ImGui::Text("Select a GLB in Project Browser, or use fallback character.");
    }

    if (ImGui::Button("Open Selected As Asset") && !state.selectedPath.empty() && (isGlbPath(state.selectedPath) || isImagePath(state.selectedPath))) {
        state.assetFilePath = std::filesystem::relative(state.selectedPath, state.projectRoot).string();
        if (isGlbPath(state.selectedPath)) { state.assetGlbPath = state.assetFilePath; state.assetPreviewKind = "GLB"; }
        else { state.assetPreviewKind = "Image"; }
        loadedPath.clear();
        state.assetSelectedPreviewNode = -1;
        state.log("Asset Preview target set to: " + state.assetFilePath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Send Selected To Scene") && !state.selectedPath.empty() && isGlbPath(state.selectedPath)) {
        state.sceneGlbPath = std::filesystem::relative(state.selectedPath, state.projectRoot).string();
        state.sceneMarkerSourceGlb.clear();
        state.log("Scene Preview target set from Asset Preview: " + state.sceneGlbPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload GLB")) { loadedPath.clear(); state.assetSelectedPreviewNode = -1; }
    ImGui::SameLine();
    if (ImGui::Button("+ Helper Bone")) {
        state.assetPreviewBones.push_back("HelperBone_" + std::to_string(state.assetPreviewBones.size()));
        state.assetBoneTransforms.push_back({});
        state.selectedAssetBone = (int)state.assetPreviewBones.size() - 1;
        state.log("Asset Preview added helper bone.");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Socket")) {
        state.assetPreviewBones.push_back("Socket_" + std::to_string(state.assetPreviewBones.size()));
        state.assetBoneTransforms.push_back({});
        state.selectedAssetBone = (int)state.assetPreviewBones.size() - 1;
        state.log("Asset Preview added socket.");
    }

    drawTransformToolButtons(state);
    ImGui::SameLine(); ImGui::Checkbox("Real Render", &state.assetUseRealRender);
    ImGui::SameLine(); ImGui::Checkbox("Mesh", &state.assetShowMesh);
    ImGui::SameLine(); ImGui::Checkbox("Skeleton", &state.assetShowSkeleton);
    ImGui::SameLine(); ImGui::Checkbox("Sockets", &state.assetShowSockets);
    ImGui::SameLine(); ImGui::Checkbox("Sky", &state.assetShowSky);
    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float sideWidth = 380.0f;
    ImVec2 canvasSize(std::max(220.0f, avail.x - sideWidth - 8.0f), std::max(220.0f, avail.y));
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 q(p.x + canvasSize.x, p.y + canvasSize.y);

    if (state.assetUseRealRender && std::filesystem::exists(assetPath) && state.assetPreviewKind == "GLB") {
        if (loadedPath != assetPath || !assetRenderer.loaded()) {
            if (assetRenderer.loadGlb(assetPath, &lastError)) {
                loadedPath = assetPath;
                state.assetCameraDistance = assetRenderer.suggestedCameraDistance();
                assetRenderer.framedCameraPosition(state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance, state.assetFrameModel, state.assetCameraFocus);
                state.assetSelectedPreviewNode = -1;
                state.log("Asset Preview loaded GLB: " + assetPath.string());
            } else {
                loadedPath.clear();
                state.log("Asset Preview failed GLB load: " + lastError);
            }
        }
        unsigned int tex = assetRenderer.renderToTexture((int)canvasSize.x, (int)canvasSize.y,
            state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance,
            true, state.assetShowSky, state.assetShowSkeleton, state.assetFrameModel, state.assetCameraFocus);
        ImGui::Image((ImTextureID)(intptr_t)tex, canvasSize, ImVec2(0, 1), ImVec2(1, 0));
    } else if (std::filesystem::exists(assetPath) && state.assetPreviewKind == "Image") {
        if (loadedPath != assetPath || !assetRenderer.imageLoaded()) {
            if (assetRenderer.loadImage(assetPath, &lastError)) {
                loadedPath = assetPath;
                state.log("Asset Preview loaded image: " + assetPath.string());
            } else {
                loadedPath.clear();
                state.log("Asset Preview failed image load: " + lastError);
            }
        }
        ImDrawList* bg = ImGui::GetWindowDrawList();
        bg->AddRectFilled(p, q, IM_COL32(18, 18, 22, 255));
        if (assetRenderer.imageLoaded()) {
            float imgW = (float)assetRenderer.imageWidth();
            float imgH = (float)assetRenderer.imageHeight();
            float scale = std::min(canvasSize.x / std::max(imgW, 1.0f), canvasSize.y / std::max(imgH, 1.0f));
            scale = std::min(scale, 1.0f);
            ImVec2 size(imgW * scale, imgH * scale);
            ImVec2 pos(p.x + (canvasSize.x - size.x) * 0.5f, p.y + (canvasSize.y - size.y) * 0.5f);
            ImGui::SetCursorScreenPos(pos);
            ImGui::Image((ImTextureID)(intptr_t)assetRenderer.imageTexture(), size);
            ImGui::SetCursorScreenPos(p);
            ImGui::InvisibleButton("ImagePreviewCanvas", canvasSize);
        } else {
            ImGui::InvisibleButton("ImagePreviewCanvas", canvasSize);
        }
    } else {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, q, IM_COL32(17, 18, 23, 255));
        // No asset selected yet. Use Project Browser to open a GLB/image in Asset Preview.
        ImGui::InvisibleButton("AssetPreviewFallbackCanvas", canvasSize);
    }

    const bool canvasHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGuiIO& io = ImGui::GetIO();

    // Same viewport capture rule as Scene Preview. ImGui::Image is hoverable,
    // but not reliably active for RMB/MMB camera control.
    static bool assetViewportCaptured = false;
    if (canvasHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                          ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
        assetViewportCaptured = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        assetViewportCaptured = false;
    }

    if (canvasHovered && io.MouseWheel != 0.0f) {
        float dolly = io.MouseWheel * std::max(0.05f, state.assetCameraDistance * 0.08f);
        dollyCamera(state.assetCameraYaw, state.assetCameraPitch, state.assetCameraFocus, dolly);
        state.assetCameraDistance = std::clamp(state.assetCameraDistance - dolly, 0.6f, 500.0f);
    }

    const bool rotatingViewport = assetViewportCaptured &&
        (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle));

    if (rotatingViewport) {
        state.assetCameraYaw += io.MouseDelta.x * 0.25f;
        state.assetCameraPitch = std::clamp(state.assetCameraPitch - io.MouseDelta.y * 0.20f, -80.0f, 85.0f);
    }

    updateFlyCamera(state.assetCameraYaw, state.assetCameraPitch, state.assetCameraFocus, state.assetFlySpeed,
        (canvasHovered || assetViewportCaptured) && state.assetPreviewKind == "GLB");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(p, q, IM_COL32(75, 80, 95, 255));
    // Keep the viewport clean: help text lives in side panels/menus, not over the render canvas.

    if (state.assetPreviewKind == "GLB" && state.assetShowSkeleton && assetRenderer.loaded()) {
        for (const auto& n : assetRenderer.previewNodes()) {
            if (!n.isJoint) continue;
            float sx=0, sy=0;
            if (!assetRenderer.worldToScreen(n.position[0], n.position[1], n.position[2],
                    (int)canvasSize.x, (int)canvasSize.y,
                    state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance,
                    state.assetFrameModel, sx, sy, state.assetCameraFocus)) continue;
            ImVec2 sp(p.x + sx, p.y + sy);
            bool selected = state.assetSelectedPreviewNode == n.index;
            ImU32 col = selected ? IM_COL32(255,210,80,255) : IM_COL32(50,220,255,220);
            dl->AddCircleFilled(sp, selected ? 5.5f : 3.5f, col, 16);
            if (selected && state.assetShowSelectionOutline) {
                dl->AddCircle(sp, 11.0f, IM_COL32(255,255,255,240), 24, 2.0f);
                dl->AddCircle(sp, 17.0f, IM_COL32(255,210,80,180), 32, 1.5f);
                dl->AddText(ImVec2(sp.x + 8, sp.y - 8), IM_COL32(245,245,245,255), n.name.c_str());
            }
            if (selected && state.assetShowAxisGizmo) {
                float gx=0, gy=0, bx=0, by=0, ux=0, uy=0;
                assetRenderer.worldToScreen(n.position[0] + 0.35f, n.position[1], n.position[2], (int)canvasSize.x, (int)canvasSize.y, state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance, state.assetFrameModel, gx, gy, state.assetCameraFocus);
                assetRenderer.worldToScreen(n.position[0], n.position[1] + 0.35f, n.position[2], (int)canvasSize.x, (int)canvasSize.y, state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance, state.assetFrameModel, ux, uy, state.assetCameraFocus);
                assetRenderer.worldToScreen(n.position[0], n.position[1], n.position[2] + 0.35f, (int)canvasSize.x, (int)canvasSize.y, state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance, state.assetFrameModel, bx, by, state.assetCameraFocus);
                drawAxisGizmo2D(dl, sp, ImVec2(p.x + gx, p.y + gy), ImVec2(p.x + ux, p.y + uy), ImVec2(p.x + bx, p.y + by));
            }
            const float dx = io.MousePos.x - sp.x;
            const float dy = io.MousePos.y - sp.y;
            if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dx*dx + dy*dy < 100.0f) {
                state.assetSelectedPreviewNode = n.index;
            }
        }
    }

    if (state.assetShowSockets && state.assetPreviewKind == "GLB") {
        for (int i = 0; i < (int)state.assetBoneTransforms.size(); ++i) {
            const PreviewTransform& t = state.assetBoneTransforms[i];
            float sx=0, sy=0;
            if (!assetRenderer.worldToScreen(t.position[0], t.position[1], t.position[2],
                    (int)canvasSize.x, (int)canvasSize.y,
                    state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance,
                    state.assetFrameModel, sx, sy, state.assetCameraFocus)) continue;
            ImVec2 sp(p.x + sx, p.y + sy);
            bool selected = state.selectedAssetBone == i;
            drawSocketDiamond(dl, sp, selected);
            if (selected) dl->AddText(ImVec2(sp.x + 10, sp.y - 10), IM_COL32(245,245,245,255), state.assetPreviewBones[i].c_str());
            float dx = io.MousePos.x - sp.x;
            float dy = io.MousePos.y - sp.y;
            if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dx*dx + dy*dy < 140.0f) {
                state.selectedAssetBone = i;
                state.assetSelectedPreviewNode = -1;
            }
        }
    }

    ImGui::SameLine();
    ImGui::BeginChild("AssetPreviewTools", ImVec2(sideWidth, canvasSize.y), true);
    ImGui::Text("Asset Target");
    ImGui::TextWrapped("%s", assetPath.string().c_str());
    ImGui::Text("Type: %s", state.assetPreviewKind.c_str());
    ImGui::TextWrapped("Loaded: %s", (assetRenderer.loaded() || assetRenderer.imageLoaded()) ? assetRenderer.loadedName().c_str() : "none");
    if (!lastError.empty()) ImGui::TextWrapped("Last error: %s", lastError.c_str());
    if (state.assetPreviewKind == "Image") {
        ImGui::Text("Image: %d x %d", assetRenderer.imageWidth(), assetRenderer.imageHeight());
    } else {
        ImGui::Text("Nodes: %d", assetRenderer.nodeCount());
        ImGui::Text("Bones/Joints: %d", assetRenderer.boneCount());
        ImGui::Text("Primitives: %d", assetRenderer.primitiveCount());
        ImGui::Text("Triangles: %d", assetRenderer.triangleCount());
    }
    ImGui::DragFloat("Yaw##assetcam", &state.assetCameraYaw, 0.5f);
    ImGui::DragFloat("Pitch##assetcam", &state.assetCameraPitch, 0.5f, -80.0f, 85.0f);
    float oldAssetDollyDistance = state.assetCameraDistance;
    if (ImGui::DragFloat("Dolly Distance##assetcam", &state.assetCameraDistance, 0.1f, 0.6f, 500.0f)) {
        float delta = oldAssetDollyDistance - state.assetCameraDistance;
        dollyCamera(state.assetCameraYaw, state.assetCameraPitch, state.assetCameraFocus, delta);
    }
    ImGui::DragFloat3("Camera Position", state.assetCameraFocus, 0.05f);
    ImGui::Checkbox("Frame Model Bounds", &state.assetFrameModel);
    if (ImGui::Button("Frame Asset")) {
        state.assetCameraYaw = -20.0f;
        state.assetCameraPitch = 12.0f;
        state.assetFrameModel = true;
        state.assetCameraDistance = assetRenderer.loaded() ? assetRenderer.suggestedCameraDistance() : 6.5f;
        assetRenderer.framedCameraPosition(state.assetCameraYaw, state.assetCameraPitch, state.assetCameraDistance, state.assetFrameModel, state.assetCameraFocus);
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Asset Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (state.assetPreviewKind == "Image") {
            static bool srgb = true; static bool mipmaps = true; static int wrap = 0;
            ImGui::Checkbox("TextureImporter: sRGB", &srgb);
            ImGui::Checkbox("Generate Mipmaps", &mipmaps);
            ImGui::Combo("Wrap Mode", &wrap, "Repeat\0Clamp\0Mirror\0");
            ImGui::TextDisabled("Will later generate .axtexture.json / MaterialRegistry entry.");
        } else if (state.assetPreviewKind == "GLB") {
            ImGui::BulletText("Mesh / SkinnedMesh import settings");
            ImGui::BulletText("Skeleton node browser");
            ImGui::BulletText("Sockets / helper bones metadata");
        } else {
            ImGui::TextDisabled("Select a GLB or PNG/JPG/WebP asset.");
        }
    }

    if (state.assetPreviewKind == "GLB" && ImGui::CollapsingHeader("GLB Skeleton / Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("PreviewNodeList", ImVec2(0, 190), true);
        for (const auto& n : assetRenderer.previewNodes()) {
            if (!n.isJoint && !n.hasMesh) continue;
            bool selected = state.assetSelectedPreviewNode == n.index;
            std::string label = (n.isJoint ? "[Bone] " : "[Mesh] ") + n.name + " <" + n.type + ">";
            if (ImGui::Selectable(label.c_str(), selected)) state.assetSelectedPreviewNode = n.index;
        }
        ImGui::EndChild();
    }

    const auto* selectedNode = findPreviewNodeByIndex(assetRenderer, state.assetSelectedPreviewNode);
    if (state.assetPreviewKind == "GLB" && selectedNode && ImGui::CollapsingHeader("Selected GLB Node", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Name: %s", selectedNode->name.c_str());
        ImGui::Text("Type: %s", selectedNode->type.c_str());
        ImGui::Text("Parent node index: %d", selectedNode->parentIndex);
        float loc[3] = {selectedNode->position[0], selectedNode->position[1], selectedNode->position[2]};
        float rot[3] = {selectedNode->rotation[0], selectedNode->rotation[1], selectedNode->rotation[2]};
        float scl[3] = {selectedNode->scale[0], selectedNode->scale[1], selectedNode->scale[2]};
        ImGui::InputFloat3("GLB World Location", loc, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat3("GLB Approx Rotation", rot, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::InputFloat3("GLB World Scale", scl, "%.3f", ImGuiInputTextFlags_ReadOnly);
        ImGui::TextWrapped("Runtime note: these are imported GLB transform values. To author sockets/offsets, use helper metadata below so AXEditor writes readable config instead of corrupting the source GLB.");
    }

    if (state.assetPreviewKind == "GLB" && ImGui::CollapsingHeader("GLB Materials")) {
        if (assetRenderer.materialNames().empty()) ImGui::TextDisabled("No materials loaded.");
        for (const auto& m : assetRenderer.materialNames()) ImGui::BulletText("%s", m.c_str());
    }

    if (state.assetPreviewKind == "GLB" && ImGui::CollapsingHeader("Editable Helper Bones / Sockets", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Delete Selected Helper") && state.selectedAssetBone >= 0 && state.selectedAssetBone < (int)state.assetPreviewBones.size()) {
            std::string name = state.assetPreviewBones[state.selectedAssetBone];
            state.assetPreviewBones.erase(state.assetPreviewBones.begin() + state.selectedAssetBone);
            state.assetBoneTransforms.erase(state.assetBoneTransforms.begin() + state.selectedAssetBone);
            state.selectedAssetBone = -1;
            state.log("Asset Preview deleted helper/socket: " + name);
        }
        ImGui::BeginChild("HelperList", ImVec2(0, 120), true);
        for (int i = 0; i < (int)state.assetPreviewBones.size(); ++i) {
            bool selected = state.selectedAssetBone == i;
            if (ImGui::Selectable(state.assetPreviewBones[i].c_str(), selected)) state.selectedAssetBone = i;
        }
        ImGui::EndChild();
        if (state.selectedAssetBone >= 0 && state.selectedAssetBone < (int)state.assetBoneTransforms.size()) {
            PreviewTransform& t = state.assetBoneTransforms[state.selectedAssetBone];
            ImGui::Text("Selected helper: %s", state.assetPreviewBones[state.selectedAssetBone].c_str());
            ImGui::DragFloat3("Loc", t.position, 0.02f);
            ImGui::DragFloat3("Rot", t.rotation, 0.5f);
            ImGui::DragFloat3("Scale", t.scale, 0.02f, 0.01f, 100.0f);
        }
    }
    ImGui::EndChild();

    ImGui::End();
}
