#include "ProjectBrowserPanel.h"
#include "PanelLayout.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <system_error>
#include "../scene/AXSceneIO.h"

namespace {
static std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static std::string genericRel(const std::filesystem::path& p, const std::filesystem::path& root) {
    std::error_code ec;
    auto rel = std::filesystem::relative(p, root, ec);
    if (ec) return p.generic_string();
    return rel.generic_string();
}

static bool isGlbPath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".glb" || e == ".gltf";
}
static bool isImagePath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".webp" || e == ".bmp" || e == ".tga";
}
static bool isAxScenePath(const std::filesystem::path& p) {
    return lowerCopy(p.extension().string()) == ".axscene";
}
static bool isCodePath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".cpp" || e == ".h" || e == ".hpp" || e == ".c" || e == ".cc" || e == ".vert" || e == ".frag" || e == ".glsl";
}
static bool isJsonLikePath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    std::string n = lowerCopy(p.filename().string());
    return e == ".json" || n.find(".axmat") != std::string::npos || n.find(".axrender") != std::string::npos ||
           n.find(".axattack") != std::string::npos || n.find(".axvfx") != std::string::npos ||
           n.find(".axinput") != std::string::npos || n.find(".axenemy") != std::string::npos ||
           n.find(".axui") != std::string::npos || n.find(".axcombo") != std::string::npos ||
           n.find(".axproject") != std::string::npos;
}
static bool isAudioPath(const std::filesystem::path& p) {
    std::string e = lowerCopy(p.extension().string());
    return e == ".wav" || e == ".ogg" || e == ".mp3" || e == ".flac";
}
static bool isRelevantProjectFile(const std::filesystem::path& p) {
    return isGlbPath(p) || isImagePath(p) || isAxScenePath(p) || isCodePath(p) || isJsonLikePath(p) || isAudioPath(p);
}
static bool isHiddenOrBuildFolder(const std::filesystem::path& p) {
    const std::string name = lowerCopy(p.filename().string());
    return name == ".git" || name == ".cache" || name == "build" || name == "cmakefiles" || name == ".idea" || name == ".vscode";
}
static std::string fileKind(const std::filesystem::path& p) {
    if (isAxScenePath(p)) return "AXScene";
    if (isGlbPath(p)) return "GLB";
    if (isImagePath(p)) return "Image";
    if (isAudioPath(p)) return "Audio";
    if (isCodePath(p)) return "Code/Shader";
    if (isJsonLikePath(p)) return "AX Data";
    return "File";
}
static std::string iconPrefix(const std::filesystem::path& p, bool isDir) {
    if (isDir) return "[DIR] ";
    if (isAxScenePath(p)) return "[SCENE] ";
    if (isGlbPath(p)) return "[GLB] ";
    if (isImagePath(p)) return "[IMG] ";
    if (isAudioPath(p)) return "[AUD] ";
    if (isCodePath(p)) return "[CODE] ";
    if (isJsonLikePath(p)) return "[AX] ";
    return "[FILE] ";
}
static bool matchesFilter(const std::filesystem::path& p, const std::string& filter) {
    if (filter.empty()) return true;
    const std::string f = lowerCopy(filter);
    const std::string rel = lowerCopy(p.generic_string());
    return rel.find(f) != std::string::npos;
}
static bool matchesMode(const std::filesystem::path& p, int mode) {
    switch (mode) {
        case 1: return isGlbPath(p);
        case 2: return isAxScenePath(p);
        case 3: return isCodePath(p);
        case 4: return isJsonLikePath(p);
        case 5: return isImagePath(p);
        case 6: return isAudioPath(p);
        default: return isRelevantProjectFile(p);
    }
}
static std::string shellQuote(const std::string& in) {
    std::string out = "'";
    for (char c : in) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}
static void openInOsEditor(EditorState& state, const std::filesystem::path& p) {
    if (p.empty()) return;
#if defined(_WIN32)
    std::string cmd = "start \"\" " + shellQuote(p.string());
#elif defined(__APPLE__)
    std::string cmd = "open " + shellQuote(p.string()) + " >/dev/null 2>&1 &";
#else
    std::string cmd = "xdg-open " + shellQuote(p.string()) + " >/dev/null 2>&1 &";
#endif
    int code = std::system(cmd.c_str());
    state.log("Project Explorer: opened with OS default app: " + p.string() + " exit=" + std::to_string(code));
}
static bool looksLikeSceneGlb(const std::filesystem::path& p) {
    const std::string s = lowerCopy(p.string());
    return s.find("scene") != std::string::npos || s.find("level") != std::string::npos || s.find("map") != std::string::npos;
}
static void openSelectedAxScene(EditorState& state) {
    if (state.selectedPath.empty() || !isAxScenePath(state.selectedPath)) return;
    std::string error;
    if (AXSceneIO::readSceneFile(state.selectedPath, state, &error)) {
        state.showSceneView = true;
        state.log("Project Explorer: opened AXScene: " + state.selectedPath.string());
    } else {
        state.log("Project Explorer: failed to open AXScene: " + error);
    }
}

static void openSelectedAsScene(EditorState& state) {
    if (state.selectedPath.empty()) return;
    if (isAxScenePath(state.selectedPath)) {
        openSelectedAxScene(state);
        return;
    }
    if (!isGlbPath(state.selectedPath)) return;
    state.sceneGlbPath = genericRel(state.selectedPath, state.projectRoot);
    state.sceneMarkerSourceGlb.clear();
    state.showSceneView = true;
    state.sceneDirty = true;
    state.sceneNeedsPreviewRebuild = true;
    state.log("Project Explorer: opened GLB as current scene target: " + state.sceneGlbPath);
}
static void openSelectedAsAsset(EditorState& state) {
    if (state.selectedPath.empty() || !(isGlbPath(state.selectedPath) || isImagePath(state.selectedPath))) return;
    state.assetFilePath = genericRel(state.selectedPath, state.projectRoot);
    if (isGlbPath(state.selectedPath)) {
        state.assetGlbPath = state.assetFilePath;
        state.assetPreviewKind = "GLB";
    } else {
        state.assetPreviewKind = "Image";
    }
    state.assetSelectedPreviewNode = -1;
    state.showAssetPreview = true;
    state.log("Project Explorer: opened asset in Asset Preview: " + state.assetFilePath);
}

static EditorComponent browserMakeComponent(const std::string& type, const std::string& assetPath = "") {
    EditorComponent c;
    c.type = type;
    c.active = true;
    c.dynamic = type == "SkinnedMeshRenderer";
    if (type == "StaticMeshRenderer") {
        c.properties.push_back({"mesh", assetPath});
        c.properties.push_back({"castShadows", "true"});
        c.properties.push_back({"receiveShadows", "true"});
    } else if (type == "SkinnedMeshRenderer") {
        c.properties.push_back({"model", assetPath});
        c.properties.push_back({"defaultAnimation", "Idle"});
    }
    return c;
}

static void addSelectedGlbToCurrentScene(EditorState& state) {
    if (state.selectedPath.empty() || !isGlbPath(state.selectedPath)) {
        state.log("Import GLB To Scene: select a .glb/.gltf first.");
        return;
    }
    std::string rel = genericRel(state.selectedPath, state.projectRoot);
    EditorObject obj;
    obj.name = state.selectedPath.stem().string();
    obj.type = looksLikeSceneGlb(state.selectedPath) ? "StaticScene" : "StaticMesh";
    obj.sourcePath = rel;
    obj.isStatic = true;
    obj.position[0] = (float)(state.hierarchy.size() % 5) * 2.0f;
    obj.position[1] = 0.0f;
    obj.position[2] = (float)(state.hierarchy.size() / 5) * 2.0f;
    obj.components.push_back(browserMakeComponent("StaticMeshRenderer", rel));
    state.hierarchy.push_back(obj);
    selectSingle(state, (int)state.hierarchy.size() - 1);
    state.sceneDirty = true;
    state.sceneNeedsPreviewRebuild = true;
    state.showSceneView = true;
    state.log("Imported GLB into AXScene object: " + rel);
}

struct ScannedFile {
    std::filesystem::path path;
    std::string rel;
    std::string kind;
};

static void scanProjectFiles(EditorState& state, std::vector<ScannedFile>& out, int mode, const std::string& filter) {
    out.clear();
    std::error_code ec;
    if (!std::filesystem::exists(state.projectRoot, ec)) return;

    std::filesystem::recursive_directory_iterator it(
        state.projectRoot,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    );
    std::filesystem::recursive_directory_iterator end;

    for (; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        const std::filesystem::directory_entry& entry = *it;
        const std::filesystem::path p = entry.path();

        if (entry.is_directory(ec)) {
            if (isHiddenOrBuildFolder(p)) it.disable_recursion_pending();
            continue;
        }
        if (!entry.is_regular_file(ec)) continue;
        if (!matchesMode(p, mode)) continue;
        const std::string rel = genericRel(p, state.projectRoot);
        if (!matchesFilter(std::filesystem::path(rel), filter)) continue;
        out.push_back({p, rel, fileKind(p)});
    }

    std::sort(out.begin(), out.end(), [](const ScannedFile& a, const ScannedFile& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        return lowerCopy(a.rel) < lowerCopy(b.rel);
    });
}

static void drawFileActions(EditorState& state, const std::filesystem::path& p) {
    state.selectedPath = p;
    if (isAxScenePath(p) && ImGui::MenuItem("Open AXScene")) openSelectedAxScene(state);
    if (isGlbPath(p) && ImGui::MenuItem("Open as Scene Target")) openSelectedAsScene(state);
    if (isGlbPath(p) && ImGui::MenuItem("Import GLB To Current AXScene")) addSelectedGlbToCurrentScene(state);
    if ((isGlbPath(p) || isImagePath(p)) && ImGui::MenuItem("Open in Asset Preview")) openSelectedAsAsset(state);
    if (isCodePath(p) || isJsonLikePath(p) || isAudioPath(p) || isImagePath(p)) {
        if (ImGui::MenuItem("Open With OS Default App")) openInOsEditor(state, p);
    }
}

static void drawFlatProjectList(EditorState& state, int mode, const std::string& filter) {
    static std::vector<ScannedFile> files;
    static std::string lastRoot;
    static std::string lastFilter;
    static int lastMode = -1;
    static int frameCountdown = 0;

    const std::string root = state.projectRoot.string();
    if (root != lastRoot || filter != lastFilter || mode != lastMode || frameCountdown <= 0) {
        scanProjectFiles(state, files, mode, filter);
        lastRoot = root;
        lastFilter = filter;
        lastMode = mode;
        frameCountdown = 45;
    } else {
        --frameCountdown;
    }

    ImGui::Text("Visible project files: %d", (int)files.size());
    ImGui::Separator();

    if (ImGui::BeginTable("ProjectExplorerFlatTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 220))) {
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableHeadersRow();

        for (const ScannedFile& f : files) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(f.kind.c_str());

            ImGui::TableSetColumnIndex(1);
            const bool selected = state.selectedPath == f.path;
            if (ImGui::Selectable(f.rel.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                state.selectedPath = f.path;
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                state.selectedPath = f.path;
                if (isAxScenePath(f.path)) openSelectedAxScene(state);
                else if (isGlbPath(f.path) || isImagePath(f.path)) openSelectedAsAsset(state);
                else openInOsEditor(state, f.path);
            }
            if (ImGui::BeginPopupContextItem()) {
                drawFileActions(state, f.path);
                ImGui::EndPopup();
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(f.rel.c_str());
            if (isAxScenePath(f.path)) {
                if (ImGui::SmallButton("Open Scene")) { state.selectedPath = f.path; openSelectedAxScene(state); }
            } else if (isGlbPath(f.path)) {
                if (ImGui::SmallButton("Preview")) { state.selectedPath = f.path; openSelectedAsAsset(state); }
                ImGui::SameLine();
                if (ImGui::SmallButton("Import")) { state.selectedPath = f.path; addSelectedGlbToCurrentScene(state); }
            } else if (isImagePath(f.path)) {
                if (ImGui::SmallButton("Preview")) { state.selectedPath = f.path; openSelectedAsAsset(state); }
            } else if (isCodePath(f.path) || isJsonLikePath(f.path) || isAudioPath(f.path)) {
                if (ImGui::SmallButton("Open")) { state.selectedPath = f.path; openInOsEditor(state, f.path); }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

static void drawDirectoryTree(EditorState& state, const std::filesystem::path& path, int depth = 0) {
    if (depth > 10) return;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return;

    std::vector<std::filesystem::directory_entry> entries;
    for (std::filesystem::directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (isHiddenOrBuildFolder(it->path())) continue;
        entries.push_back(*it);
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        std::error_code ecA, ecB;
        const bool ad = a.is_directory(ecA);
        const bool bd = b.is_directory(ecB);
        if (ad != bd) return ad > bd;
        return lowerCopy(a.path().filename().string()) < lowerCopy(b.path().filename().string());
    });

    for (const auto& entry : entries) {
        const auto p = entry.path();
        std::error_code ec2;
        const bool isDir = entry.is_directory(ec2);
        if (!isDir && !isRelevantProjectFile(p)) continue;

        ImGuiTreeNodeFlags flags = isDir ? ImGuiTreeNodeFlags_OpenOnArrow : ImGuiTreeNodeFlags_Leaf;
        if (depth < 1) flags |= ImGuiTreeNodeFlags_DefaultOpen;
        if (state.selectedPath == p) flags |= ImGuiTreeNodeFlags_Selected;

        const std::string label = iconPrefix(p, isDir) + p.filename().string();
        bool open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::IsItemClicked()) {
            state.selectedPath = p;
        }
        if (!isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            state.selectedPath = p;
            if (isAxScenePath(p)) openSelectedAxScene(state);
            else if (isGlbPath(p) || isImagePath(p)) openSelectedAsAsset(state);
            else openInOsEditor(state, p);
        }
        if (!isDir && ImGui::BeginPopupContextItem()) {
            drawFileActions(state, p);
            ImGui::EndPopup();
        }
        if (open) {
            if (isDir) drawDirectoryTree(state, p, depth + 1);
            ImGui::TreePop();
        }
    }
}

} // namespace

void drawProjectBrowserPanel(EditorState& state) {
    AXEditorLayout::placeProjectBrowser(state);
    ImGui::Begin("Project Browser", &state.showProjectBrowser, AXEditorLayout::panelFlags(state));
    ImGui::TextWrapped("Root: %s", state.projectRoot.string().c_str());
    ImGui::TextDisabled("This panel now scans the whole project root, including assets, generated registries, shaders, and AX data files.");
    ImGui::Separator();

    static char filterBuf[256] = "";
    static int mode = 0;
    static bool showTree = true;

    if (ImGui::Button("Refresh")) state.log("Project Explorer: refresh requested.");
    ImGui::SameLine();
    if (ImGui::Button("Open AXScene")) openSelectedAxScene(state);
    ImGui::SameLine();
    if (ImGui::Button("Open As Asset")) openSelectedAsAsset(state);
    ImGui::SameLine();
    if (ImGui::Button("Import GLB To Scene")) addSelectedGlbToCurrentScene(state);

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##ProjectExplorerSearch", "Search project files: glb, scene, script, material, attack, vfx...", filterBuf, sizeof(filterBuf));

    ImGui::TextUnformatted("Show:");
    ImGui::SameLine(); if (ImGui::RadioButton("All", mode == 0)) mode = 0;
    ImGui::SameLine(); if (ImGui::RadioButton("GLB", mode == 1)) mode = 1;
    ImGui::SameLine(); if (ImGui::RadioButton("Scenes", mode == 2)) mode = 2;
    ImGui::SameLine(); if (ImGui::RadioButton("Scripts/Shaders", mode == 3)) mode = 3;
    ImGui::SameLine(); if (ImGui::RadioButton("AX Data", mode == 4)) mode = 4;
    ImGui::SameLine(); if (ImGui::RadioButton("Images", mode == 5)) mode = 5;
    ImGui::SameLine(); if (ImGui::RadioButton("Audio", mode == 6)) mode = 6;

    ImGui::Checkbox("Show folder tree below flat results", &showTree);

    if (!state.selectedPath.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("Selected: %s", state.selectedPath.string().c_str());
        ImGui::Text("Type: %s", fileKind(state.selectedPath).c_str());
    }
    ImGui::Separator();

    drawFlatProjectList(state, mode, filterBuf);

    if (showTree) {
        ImGui::Separator();
        if (ImGui::TreeNodeEx("Full Project Folder Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
            drawDirectoryTree(state, state.projectRoot, 0);
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
