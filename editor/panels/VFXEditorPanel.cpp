#include "VFXEditorPanel.h"
#include "PanelLayout.h"
#include "EditorState.h"
#include "EditorPanelUtil.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>

namespace {
struct VfxEvent { std::string type="Spawn"; float time=0.0f; std::string value=""; };
static std::vector<VfxEvent> g_events = {{"Spawn",0.0f,"mesh"},{"FadeOut",0.18f,"alpha"}};
static std::string g_status="Ready";
static std::string g_filter;
static int g_type=0;
static bool g_fade=true;
static bool g_additive=true;
static float g_color[4]={1.0f,0.35f,0.08f,1.0f};
static float g_uvFrames[2]={4,2};
static float g_uvFps=48.0f;

static std::string json(EditorState& s){ std::ostringstream o; o<<"{\n  \"type\": \"AXVFX\",\n  \"version\": 2,\n  \"assetId\": \""<<axed::stableIdFor(s.vfxEditorPath)<<"\",\n  \"name\": \""<<axed::escapeJson(std::filesystem::path(s.vfxEditorPath).stem().string())<<"\",\n  \"vfxType\": \""<<(g_type==0?"SlashMesh":g_type==1?"ImpactSpark":g_type==2?"Shockwave":"Afterimage")<<"\",\n  \"mesh\": \""<<axed::escapeJson(s.vfxMesh)<<"\",\n  \"material\": \""<<axed::escapeJson(s.vfxMaterial)<<"\",\n  \"lifetime\": "<<s.vfxLifetime<<",\n  \"scale\": ["<<s.vfxScale[0]<<", "<<s.vfxScale[1]<<", "<<s.vfxScale[2]<<"],\n  \"color\": ["<<g_color[0]<<", "<<g_color[1]<<", "<<g_color[2]<<", "<<g_color[3]<<"],\n  \"billboard\": "<<(s.vfxBillboard?"true":"false")<<",\n  \"bloom\": "<<(s.vfxBloom?"true":"false")<<",\n  \"additive\": "<<(g_additive?"true":"false")<<",\n  \"fade\": "<<(g_fade?"true":"false")<<",\n  \"spriteSheet\": { \"framesX\": "<<g_uvFrames[0]<<", \"framesY\": "<<g_uvFrames[1]<<", \"fps\": "<<g_uvFps<<" },\n  \"events\": [\n"; for(size_t i=0;i<g_events.size();++i){auto&e=g_events[i]; o<<"    { \"time\": "<<e.time<<", \"type\": \""<<axed::escapeJson(e.type)<<"\", \"value\": \""<<axed::escapeJson(e.value)<<"\" }"<<(i+1<g_events.size()?",":"")<<"\n";} o<<"  ]\n}\n"; return o.str();}
static void load(EditorState& s){std::string t,err; if(!axed::readTextFile(axed::resolvePath(s,s.vfxEditorPath),t,&err)){g_status=err+" - editing new VFX.";return;} s.vfxMesh=axed::getString(t,"mesh",s.vfxMesh); s.vfxMaterial=axed::getString(t,"material",s.vfxMaterial); s.vfxLifetime=axed::getFloat(t,"lifetime",s.vfxLifetime); axed::getFloat3(t,"scale",s.vfxScale); axed::getFloat4(t,"color",g_color); s.vfxBillboard=axed::getBool(t,"billboard",s.vfxBillboard); s.vfxBloom=axed::getBool(t,"bloom",s.vfxBloom); g_additive=axed::getBool(t,"additive",g_additive); g_fade=axed::getBool(t,"fade",g_fade); g_status="Loaded "+s.vfxEditorPath;}
static void save(EditorState& s){std::string err; if(axed::writeTextFile(axed::resolvePath(s,s.vfxEditorPath),json(s),&err)){g_status="Saved "+s.vfxEditorPath; s.log(g_status);}else{g_status=err; s.log(err);}}
}
void drawVFXEditorPanel(EditorState& state){
 AXEditorLayout::prepareFloatingTool(state, 760.0f, 640.0f); if(!ImGui::Begin("VFX Editor",&state.showVFXEditor)){ImGui::End();return;}
 ImGui::InputText("VFX file",&state.vfxEditorPath); if(ImGui::Button("Load"))load(state); ImGui::SameLine(); if(ImGui::Button("Save"))save(state); ImGui::SameLine(); if(ImGui::Button("Open material")) axed::openInOs(axed::resolvePath(state,state.vfxMaterial)); ImGui::TextDisabled("%s",g_status.c_str());
 ImGui::Columns(2,"vfxcols",true);
 const char* types[]={"SlashMesh","ImpactSpark","Shockwave","Afterimage"}; ImGui::Combo("VFX type",&g_type,types,4);
 ImGui::InputText("Mesh GLB",&state.vfxMesh); ImGui::InputText("Material",&state.vfxMaterial); ImGui::DragFloat("Lifetime",&state.vfxLifetime,0.01f,0.01f,10.0f,"%.2fs"); ImGui::DragFloat3("Scale",state.vfxScale,0.05f,0.01f,100.0f); ImGui::ColorEdit4("Tint",g_color); ImGui::Checkbox("Billboard",&state.vfxBillboard); ImGui::SameLine(); ImGui::Checkbox("Bloom",&state.vfxBloom); ImGui::Checkbox("Additive",&g_additive); ImGui::SameLine(); ImGui::Checkbox("Fade",&g_fade); ImGui::DragFloat2("Sheet frames",g_uvFrames,1,1,64,"%.0f"); ImGui::DragFloat("Sheet FPS",&g_uvFps,1,1,240);
 ImGui::Separator(); ImGui::Text("Events"); if(ImGui::Button("+ Event"))g_events.push_back({"Custom",state.vfxLifetime,""}); ImGui::SameLine(); if(ImGui::Button("Sort"))std::sort(g_events.begin(),g_events.end(),[](auto&a,auto&b){return a.time<b.time;});
 for(size_t i=0;i<g_events.size();++i){ImGui::PushID((int)i); ImGui::DragFloat("T",&g_events[i].time,0.01f,0,state.vfxLifetime); ImGui::SameLine(); ImGui::InputText("Type",&g_events[i].type); ImGui::SameLine(); if(ImGui::Button("X")){g_events.erase(g_events.begin()+i); ImGui::PopID(); break;} ImGui::InputText("Value",&g_events[i].value); ImGui::PopID();}
 ImGui::NextColumn();
 ImVec2 p=ImGui::GetCursorScreenPos(); ImVec2 sz(ImGui::GetContentRegionAvail().x,260); ImGui::InvisibleButton("vfxpreview",sz); ImDrawList* dl=ImGui::GetWindowDrawList(); dl->AddRectFilled(p,ImVec2(p.x+sz.x,p.y+sz.y),IM_COL32(15,17,22,255)); ImVec2 c(p.x+sz.x*0.5f,p.y+sz.y*0.55f); float sc=std::min(sz.x,sz.y)*0.22f; ImU32 col=ImGui::ColorConvertFloat4ToU32(ImVec4(g_color[0],g_color[1],g_color[2],g_color[3])); if(g_type==0){dl->AddBezierCubic(ImVec2(c.x-sc*1.8f,c.y+sc*.6f),ImVec2(c.x-sc*.6f,c.y-sc*.9f),ImVec2(c.x+sc*.8f,c.y-sc*.9f),ImVec2(c.x+sc*1.8f,c.y+sc*.5f),col,10);} else if(g_type==1){for(int i=0;i<12;i++){float a=i*0.52f; dl->AddLine(c,ImVec2(c.x+cosf(a)*sc*1.7f,c.y+sinf(a)*sc*1.7f),col,2);}} else if(g_type==2){dl->AddCircle(c,sc*1.5f,col,64,4); dl->AddCircle(c,sc*.8f,col,64,2);} else {for(int i=0;i<4;i++) dl->AddCircle(ImVec2(c.x-i*sc*.55f,c.y),sc*(1-i*.18f),IM_COL32(150,220,255,120-i*20),32,2);} dl->AddText(ImVec2(p.x+8,p.y+8),IM_COL32(210,210,210,255),"Runtime VFX preview placeholder: shape/timing/material intent.");
 ImGui::Text("Generated .axvfx"); std::string src=json(state); ImGui::InputTextMultiline("##vfxjson",&src,ImVec2(-1,260),ImGuiInputTextFlags_ReadOnly);
 ImGui::Columns(1); ImGui::End();}
