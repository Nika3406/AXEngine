#pragma once
#include "../EditorState.h"
#include <imgui.h>
#include <algorithm>

namespace AXEditorLayout {
    inline float menuHeight() {
        return std::max(24.0f, ImGui::GetFrameHeight());
    }

    inline float effectiveUiScale(const EditorState& s) {
        const float user = std::clamp(s.uiScale, 0.75f, 2.0f);
        const float display = s.followDisplayScale ? std::clamp(s.displayScale, 0.75f, 2.5f) : 1.0f;
        return user * display;
    }

    inline float gap(const EditorState& s) {
        const float baseGap = s.layoutGap > 0.0f ? s.layoutGap : 6.0f;
        return std::clamp(baseGap * effectiveUiScale(s), 3.0f, 24.0f);
    }

    inline ImGuiCond placementCond(const EditorState& s) {
        return (s.adaptiveWorkspace || s.layoutLocked) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    }

    inline ImVec2 pos(float x, float y) { return ImVec2(x, y); }
    inline ImVec2 size(float w, float h) { return ImVec2(std::max(1.0f, w), std::max(1.0f, h)); }

    struct WorkspaceRects {
        ImVec2 documentPos;
        ImVec2 documentSize;
        ImVec2 rightPos;
        ImVec2 rightSize;
        ImVec2 bottomPos;
        ImVec2 bottomSize;
    };

    inline EditorWorkspaceLayoutMode resolvedLayoutMode(const EditorState& s) {
        if (s.workspaceLayoutMode != EditorWorkspaceLayoutMode::Auto)
            return s.workspaceLayoutMode;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float w = viewport ? viewport->WorkSize.x : static_cast<float>(s.windowWidth);
        const float h = viewport ? viewport->WorkSize.y : static_cast<float>(s.windowHeight);
        if (w < 1120.0f || h < 700.0f) return EditorWorkspaceLayoutMode::Compact;
        if (w >= 1900.0f && h >= 900.0f) return EditorWorkspaceLayoutMode::Wide;
        return EditorWorkspaceLayoutMode::Standard;
    }

    inline float toolbarHeight(const EditorState& s) {
        const bool compact = resolvedLayoutMode(s) == EditorWorkspaceLayoutMode::Compact ||
            (ImGui::GetMainViewport() && ImGui::GetMainViewport()->WorkSize.x < 1250.0f * effectiveUiScale(s));
        return compact ? std::max(64.0f, ImGui::GetFrameHeight() * 2.0f + 14.0f)
                       : std::max(34.0f, ImGui::GetFrameHeight() + 10.0f);
    }


    inline WorkspaceRects workspaceRects(const EditorState& s) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, menuHeight());
        const ImVec2 workSize = viewport ? viewport->WorkSize : ImVec2((float)s.windowWidth, (float)s.windowHeight - menuHeight());
        const float g = gap(s);
        const float toolbar = toolbarHeight(s);
        const float top = workPos.y + toolbar + g;
        const float totalH = std::max(1.0f, workSize.y - toolbar - 2.0f * g);
        const float totalW = std::max(1.0f, workSize.x - 2.0f * g);
        const auto mode = resolvedLayoutMode(s);

        float rightRatio = 0.235f;
        float bottomRatio = 0.285f;
        if (mode == EditorWorkspaceLayoutMode::Compact) {
            rightRatio = 0.255f;
            bottomRatio = 0.30f;
        } else if (mode == EditorWorkspaceLayoutMode::Wide) {
            rightRatio = 0.215f;
            bottomRatio = 0.255f;
        }

        // Clamp pane minimums against the space that actually exists. This is
        // what prevents the managed layout from extending beyond a small VM,
        // split-screen window, or aggressively scaled HiDPI desktop.
        const float documentMinW = mode == EditorWorkspaceLayoutMode::Compact ? 300.0f : 420.0f;
        const float rightPreferredMin = mode == EditorWorkspaceLayoutMode::Compact ? 180.0f : 220.0f;
        const float rightMax = std::max(1.0f, totalW - documentMinW - g);
        const float rightMin = std::min(rightPreferredMin, rightMax);
        const float rightUpper = std::max(rightMin, std::min(460.0f, rightMax));
        const float rightW = std::clamp(totalW * rightRatio, rightMin, rightUpper);
        const float documentW = std::max(1.0f, totalW - rightW - g);

        const float documentMinH = mode == EditorWorkspaceLayoutMode::Compact ? 160.0f : 240.0f;
        const float bottomPreferredMin = mode == EditorWorkspaceLayoutMode::Compact ? 120.0f : 150.0f;
        const float bottomMax = std::max(1.0f, totalH - documentMinH - g);
        const float bottomMin = std::min(bottomPreferredMin, bottomMax);
        const float bottomUpper = std::max(bottomMin, std::min(330.0f, bottomMax));
        const float bottomH = std::clamp(totalH * bottomRatio, bottomMin, bottomUpper);
        const float documentH = std::max(1.0f, totalH - bottomH - g);

        WorkspaceRects r;
        r.documentPos = ImVec2(workPos.x + g, top);
        r.documentSize = ImVec2(documentW, documentH);
        r.rightPos = ImVec2(r.documentPos.x + documentW + g, top);
        r.rightSize = ImVec2(rightW, totalH);
        r.bottomPos = ImVec2(r.documentPos.x, r.documentPos.y + documentH + g);
        r.bottomSize = ImVec2(documentW, bottomH);
        return r;
    }

    inline void placeAdaptiveDocument(const EditorState& s) {
        const auto r = workspaceRects(s);
        ImGui::SetNextWindowPos(r.documentPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(r.documentSize, ImGuiCond_Always);
    }

    inline void placeAdaptiveRight(const EditorState& s) {
        const auto r = workspaceRects(s);
        ImGui::SetNextWindowPos(r.rightPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(r.rightSize, ImGuiCond_Always);
    }

    inline void placeAdaptiveBottom(const EditorState& s) {
        const auto r = workspaceRects(s);
        ImGui::SetNextWindowPos(r.bottomPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(r.bottomSize, ImGuiCond_Always);
    }

    // Legacy/manual geometry. Used only when adaptiveWorkspace=false.
    inline void layoutMetrics(const EditorState& s, float& g, float& y, float& leftW, float& rightW,
                              float& topH, float& bottomY, float& sceneW, float& assetW) {
        g = gap(s);
        y = menuHeight() + g;
        const float usableH = std::max(240.0f, static_cast<float>(s.windowHeight) - y - g);
        leftW = std::clamp(static_cast<float>(s.windowWidth) * s.layoutLeftWidthRatio, 140.0f, 520.0f);
        rightW = std::clamp(static_cast<float>(s.windowWidth) * s.layoutRightWidthRatio, 200.0f, 680.0f);
        topH = std::clamp(usableH * s.layoutTopHeightRatio, 260.0f, std::max(260.0f, usableH - 150.0f));
        bottomY = y + topH + g;
        const float middleW = std::max(420.0f, static_cast<float>(s.windowWidth) - leftW - rightW - 4.0f * g);
        sceneW = std::clamp(middleW * s.layoutSceneAssetSplitRatio, 260.0f, std::max(260.0f, middleW - 260.0f));
        assetW = std::max(260.0f, middleW - sceneW - g);
    }

    inline void placeConsole(const EditorState& s) {
        if (s.adaptiveWorkspace) { placeAdaptiveBottom(s); return; }
        float g,y,leftW,rightW,topH,bottomY,sceneW,assetW; layoutMetrics(s,g,y,leftW,rightW,topH,bottomY,sceneW,assetW);
        ImGui::SetNextWindowPos(pos(g, y), placementCond(s));
        ImGui::SetNextWindowSize(size(leftW, static_cast<float>(s.windowHeight) - y - g), placementCond(s));
    }

    inline void placeSceneView(const EditorState& s) {
        if (s.adaptiveWorkspace) { placeAdaptiveDocument(s); return; }
        float g,y,leftW,rightW,topH,bottomY,sceneW,assetW; layoutMetrics(s,g,y,leftW,rightW,topH,bottomY,sceneW,assetW);
        ImGui::SetNextWindowPos(pos(g + leftW + g, y), placementCond(s));
        ImGui::SetNextWindowSize(size(sceneW, topH), placementCond(s));
    }

    inline void placeAssetPreview(const EditorState& s) {
        if (s.adaptiveWorkspace) { placeAdaptiveDocument(s); return; }
        float g,y,leftW,rightW,topH,bottomY,sceneW,assetW; layoutMetrics(s,g,y,leftW,rightW,topH,bottomY,sceneW,assetW);
        const float x = g + leftW + g + sceneW + g;
        ImGui::SetNextWindowPos(pos(x, y), placementCond(s));
        ImGui::SetNextWindowSize(size(assetW, topH), placementCond(s));
    }

    inline void placeProjectBrowser(const EditorState& s) {
        if (s.adaptiveWorkspace) { placeAdaptiveBottom(s); return; }
        float g,y,leftW,rightW,topH,bottomY,sceneW,assetW; layoutMetrics(s,g,y,leftW,rightW,topH,bottomY,sceneW,assetW);
        ImGui::SetNextWindowPos(pos(g + leftW + g, bottomY), placementCond(s));
        ImGui::SetNextWindowSize(size(static_cast<float>(s.windowWidth) - leftW - rightW - 4.0f * g,
                                      static_cast<float>(s.windowHeight) - bottomY - g), placementCond(s));
    }

    inline void placeHierarchy(const EditorState& s) {
        if (s.adaptiveWorkspace) { placeAdaptiveRight(s); return; }
        float g,y,leftW,rightW,topH,bottomY,sceneW,assetW; layoutMetrics(s,g,y,leftW,rightW,topH,bottomY,sceneW,assetW);
        ImGui::SetNextWindowPos(pos(static_cast<float>(s.windowWidth) - rightW - g, bottomY), placementCond(s));
        ImGui::SetNextWindowSize(size(rightW, static_cast<float>(s.windowHeight) - bottomY - g), placementCond(s));
    }

    inline void placeInspector(const EditorState& s) {
        if (s.adaptiveWorkspace) { placeAdaptiveRight(s); return; }
        ImGui::SetNextWindowPos(pos(static_cast<float>(s.windowWidth) * 0.70f, menuHeight() + 40.0f), placementCond(s));
        ImGui::SetNextWindowSize(size(static_cast<float>(s.windowWidth) * 0.28f, static_cast<float>(s.windowHeight) * 0.45f), placementCond(s));
    }

    inline void placeAttackCollider(const EditorState& s) {
        if (s.adaptiveWorkspace) { placeAdaptiveDocument(s); return; }
        ImGui::SetNextWindowPos(ImVec2(80.0f, menuHeight() + 70.0f), placementCond(s));
        ImGui::SetNextWindowSize(ImVec2(760.0f, 760.0f), placementCond(s));
    }

    inline void prepareFloatingTool(const EditorState& s, float preferredW, float preferredH) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0, 0);
        const ImVec2 workSize = viewport ? viewport->WorkSize : ImVec2((float)s.windowWidth, (float)s.windowHeight);
        const float margin = 24.0f;
        const ImVec2 maxSize(std::max(1.0f, workSize.x - margin * 2.0f),
                             std::max(1.0f, workSize.y - margin * 2.0f));
        const ImVec2 minSize(std::min(420.0f, maxSize.x), std::min(300.0f, maxSize.y));
        ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
        ImGui::SetNextWindowSize(ImVec2(std::min(preferredW, maxSize.x), std::min(preferredH, maxSize.y)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x * 0.5f, workPos.y + workSize.y * 0.5f),
                                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    inline ImGuiWindowFlags panelFlags(const EditorState& s, ImGuiWindowFlags extra = 0) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | extra;
        if (s.adaptiveWorkspace || s.layoutLocked)
            flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        return flags;
    }
}
