#include "RenderMaterialPanel.h"
#include "PanelLayout.h"
#include "EditorState.h"
#include "EditorPanelUtil.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <sstream>

namespace {
static std::string g_source;
static std::string g_status = "No material loaded yet.";
static std::string g_filter;
static bool g_showSource = true;
static bool g_alphaBlend = false;
static bool g_doubleSided = false;
static bool g_uvScroll = false;
static float g_uvSpeed[2] = {0.0f, 0.0f};

static std::string materialJson(EditorState& s) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"type\": \"AXMaterial\",\n";
    o << "  \"version\": 2,\n";
    o << "  \"assetId\": \"" << axed::stableIdFor(s.materialEditorPath) << "\",\n";
    o << "  \"renderMode\": \"" << axed::escapeJson(s.materialRenderMode) << "\",\n";
    o << "  \"vertexShader\": \"" << axed::escapeJson(s.materialShaderVert) << "\",\n";
    o << "  \"fragmentShader\": \"" << axed::escapeJson(s.materialShaderFrag) << "\",\n";
    o << "  \"baseColor\": [" << s.materialBaseColor[0] << ", " << s.materialBaseColor[1] << ", " << s.materialBaseColor[2] << ", " << s.materialBaseColor[3] << "],\n";
    o << "  \"emission\": [" << s.materialEmission[0] << ", " << s.materialEmission[1] << ", " << s.materialEmission[2] << "],\n";
    o << "  \"roughness\": " << s.materialRoughness << ",\n";
    o << "  \"metallic\": " << s.materialMetallic << ",\n";
    o << "  \"cel\": " << (s.materialUseCel ? "true" : "false") << ",\n";
    o << "  \"outline\": " << (s.materialUseOutline ? "true" : "false") << ",\n";
    o << "  \"bloom\": " << (s.materialUseBloom ? "true" : "false") << ",\n";
    o << "  \"alphaBlend\": " << (g_alphaBlend ? "true" : "false") << ",\n";
    o << "  \"doubleSided\": " << (g_doubleSided ? "true" : "false") << ",\n";
    o << "  \"uvScroll\": " << (g_uvScroll ? "true" : "false") << ",\n";
    o << "  \"uvSpeed\": [" << g_uvSpeed[0] << ", " << g_uvSpeed[1] << "]\n";
    o << "}\n";
    return o.str();
}
static void load(EditorState& s) {
    std::string txt, err;
    auto p = axed::resolvePath(s, s.materialEditorPath);
    if (!axed::readTextFile(p, txt, &err)) { g_status = err + " - editing new file."; g_source = materialJson(s); return; }
    s.materialRenderMode = axed::getString(txt, "renderMode", s.materialRenderMode);
    s.materialShaderVert = axed::getString(txt, "vertexShader", s.materialShaderVert);
    s.materialShaderFrag = axed::getString(txt, "fragmentShader", s.materialShaderFrag);
    axed::getFloat4(txt, "baseColor", s.materialBaseColor);
    axed::getFloat3(txt, "emission", s.materialEmission);
    s.materialRoughness = axed::getFloat(txt, "roughness", s.materialRoughness);
    s.materialMetallic = axed::getFloat(txt, "metallic", s.materialMetallic);
    s.materialUseCel = axed::getBool(txt, "cel", s.materialUseCel);
    s.materialUseOutline = axed::getBool(txt, "outline", s.materialUseOutline);
    s.materialUseBloom = axed::getBool(txt, "bloom", s.materialUseBloom);
    g_alphaBlend = axed::getBool(txt, "alphaBlend", g_alphaBlend);
    g_doubleSided = axed::getBool(txt, "doubleSided", g_doubleSided);
    g_uvScroll = axed::getBool(txt, "uvScroll", g_uvScroll);
    axed::getFloat3(txt, "uvSpeed", g_uvSpeed);
    g_source = txt; g_status = "Loaded " + s.materialEditorPath;
}
static void save(EditorState& s) {
    g_source = materialJson(s);
    std::string err;
    if (axed::writeTextFile(axed::resolvePath(s, s.materialEditorPath), g_source, &err)) { g_status = "Saved " + s.materialEditorPath; s.log(g_status); }
    else { g_status = err; s.log(err); }
}
}

void drawRenderMaterialPanel(EditorState& state) {
    AXEditorLayout::prepareFloatingTool(state, 760.0f, 720.0f);
    if (!ImGui::Begin("Render / Material Editor", &state.showRenderMaterialEditor)) { ImGui::End(); return; }
    ImGui::TextWrapped("Material/shader workbench. GLB material names are lookup keys; AXMaterial decides shader, bloom, transparency, and style.");
    ImGui::InputText("Material file", &state.materialEditorPath);
    if (ImGui::Button("Load")) load(state); ImGui::SameLine();
    if (ImGui::Button("Save")) save(state); ImGui::SameLine();
    if (ImGui::Button("Open .vert")) axed::openInOs(axed::resolvePath(state, state.materialShaderVert)); ImGui::SameLine();
    if (ImGui::Button("Open .frag")) axed::openInOs(axed::resolvePath(state, state.materialShaderFrag));
    ImGui::TextDisabled("%s", g_status.c_str());
    ImGui::Separator();
    ImGui::Columns(2, "matcols", true);
    ImGui::Text("Material Controls");
    ImGui::InputText("Vertex shader", &state.materialShaderVert);
    ImGui::InputText("Fragment shader", &state.materialShaderFrag);
    if (ImGui::BeginCombo("Render mode", state.materialRenderMode.c_str())) {
        const char* modes[] = {"Realistic", "Stylized", "Cartoon", "Anime Action", "Dark Gothic", "Unlit Debug", "VFX Preview"};
        for (auto* m : modes) if (ImGui::Selectable(m, state.materialRenderMode==m)) state.materialRenderMode=m;
        ImGui::EndCombo();
    }
    ImGui::ColorEdit4("Base color", state.materialBaseColor);
    ImGui::ColorEdit3("Emission", state.materialEmission);
    ImGui::DragFloat("Roughness", &state.materialRoughness, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Metallic", &state.materialMetallic, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("Cel / toon banding", &state.materialUseCel);
    ImGui::Checkbox("Outline", &state.materialUseOutline);
    ImGui::Checkbox("Bloom source", &state.materialUseBloom);
    ImGui::Checkbox("Alpha blend", &g_alphaBlend);
    ImGui::Checkbox("Double sided", &g_doubleSided);
    ImGui::Checkbox("UV scroll", &g_uvScroll);
    ImGui::DragFloat2("UV speed", g_uvSpeed, 0.01f, -20.0f, 20.0f);
    ImGui::Separator();
    ImGui::InputTextWithHint("Filter assets", "slash, bloom, shader...", &g_filter);
    if (ImGui::BeginChild("matfiles", ImVec2(0, 190), true)) {
        auto files = axed::scanFiles(state, {".axmat", ".json", ".vert", ".frag"});
        int shown = 0;
        for (auto& f : files) {
            auto rel = axed::relativePath(state, f);
            if (!g_filter.empty() && !axed::containsNoCase(rel, g_filter)) continue;
            if (ImGui::Selectable(rel.c_str(), rel == state.materialEditorPath)) {
                if (f.extension() == ".vert") state.materialShaderVert = rel;
                else if (f.extension() == ".frag") state.materialShaderFrag = rel;
                else { state.materialEditorPath = rel; load(state); }
            }
            if (++shown > 200) break;
        }
    }
    ImGui::EndChild();
    ImGui::NextColumn();
    ImGui::Text("Preview / Source");
    ImVec2 p = ImGui::GetCursorScreenPos(); ImVec2 sz(ImGui::GetContentRegionAvail().x, 140); ImGui::InvisibleButton("matpreview", sz);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 base = ImGui::ColorConvertFloat4ToU32(ImVec4(state.materialBaseColor[0], state.materialBaseColor[1], state.materialBaseColor[2], state.materialBaseColor[3]));
    dl->AddRectFilled(p, ImVec2(p.x+sz.x, p.y+sz.y), IM_COL32(24,26,31,255));
    dl->AddRectFilled(ImVec2(p.x+20,p.y+24), ImVec2(p.x+sz.x-20,p.y+sz.y-24), base, 8.0f);
    if (state.materialUseBloom) dl->AddRect(ImVec2(p.x+16,p.y+20), ImVec2(p.x+sz.x-16,p.y+sz.y-20), IM_COL32(255,220,110,255), 8.0f, 0, 3.0f);
    if (state.materialUseOutline) dl->AddRect(ImVec2(p.x+19,p.y+23), ImVec2(p.x+sz.x-19,p.y+sz.y-23), IM_COL32(0,0,0,255), 8.0f, 0, 4.0f);
    ImGui::Checkbox("Show generated source", &g_showSource);
    if (g_showSource) {
        g_source = materialJson(state);
        ImGui::InputTextMultiline("##materialjson", &g_source, ImVec2(-1, 350), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::Columns(1);
    ImGui::End();
}
