#include "AudioPreviewPanel.h"
#include "PanelLayout.h"
#include "EditorState.h"
#include "EditorPanelUtil.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <cmath>
#include <vector>

namespace { static std::string status="Ready"; static std::string filter; static bool spectrum=true; static float marker=0.0f; }
void drawAudioPreviewPanel(EditorState& state){AXEditorLayout::prepareFloatingTool(state, 700.0f, 520.0f); if(!ImGui::Begin("Audio Preview",&state.showAudioPreview)){ImGui::End();return;} ImGui::TextWrapped("Audio is preview/event placement only. Composition/mixing stays in your DAW."); ImGui::InputText("Audio file",&state.audioPreviewPath); if(ImGui::Button("Open / Play in OS")){axed::openInOs(axed::resolvePath(state,state.audioPreviewPath)); status="Opened "+state.audioPreviewPath;} ImGui::SameLine(); if(ImGui::Button("Set marker")) marker=0.35f; ImGui::DragFloat("Volume",&state.audioPreviewVolume,.01f,0,1); ImGui::Checkbox("Loop",&state.audioPreviewLoop); ImGui::SameLine(); ImGui::Checkbox("Spectrum mode",&spectrum); ImGui::TextDisabled("%s",status.c_str());
 std::error_code ec; auto pth=axed::resolvePath(state,state.audioPreviewPath); uintmax_t sz=std::filesystem::exists(pth,ec)?std::filesystem::file_size(pth,ec):0; ImDrawList*dl=ImGui::GetWindowDrawList(); ImVec2 p=ImGui::GetCursorScreenPos(); ImVec2 szv(ImGui::GetContentRegionAvail().x,170); ImGui::InvisibleButton("wave",szv); dl->AddRectFilled(p,ImVec2(p.x+szv.x,p.y+szv.y),IM_COL32(22,24,28,255)); unsigned seed=(unsigned)(sz?sz:1234567); for(int i=0;i<128;i++){seed=1664525u*seed+1013904223u; float n=((seed>>8)&1023)/1023.0f; float x=p.x+szv.x*i/127.0f; float h=(0.08f+0.85f*n)*szv.y*(spectrum?0.80f:0.42f); if(spectrum) dl->AddRectFilled(ImVec2(x,p.y+szv.y-h),ImVec2(x+3,p.y+szv.y),IM_COL32(80,190,255,220)); else dl->AddLine(ImVec2(x,p.y+szv.y*.5f-h),ImVec2(x,p.y+szv.y*.5f+h),IM_COL32(80,190,255,255));} float mx=p.x+marker*szv.x; dl->AddLine(ImVec2(mx,p.y),ImVec2(mx,p.y+szv.y),IM_COL32(255,200,70,255),2); dl->AddText(ImVec2(p.x+8,p.y+8),IM_COL32(210,210,210,255),("File bytes: "+std::to_string((unsigned long long)sz)).c_str());
 ImGui::Separator(); ImGui::InputTextWithHint("Find audio","wav, ogg, mp3...",&filter); if(ImGui::BeginChild("audiofiles",ImVec2(0,190),true)){auto files=axed::scanFiles(state,{".wav",".ogg",".mp3",".flac"}); for(auto&f:files){auto rel=axed::relativePath(state,f); if(!filter.empty()&&!axed::containsNoCase(rel,filter))continue; if(ImGui::Selectable(rel.c_str(),rel==state.audioPreviewPath)) state.audioPreviewPath=rel;}} ImGui::EndChild(); ImGui::End();}
