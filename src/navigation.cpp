#include "navigation.h"
#include "navigation_math.h"
#include "world_probe.h"
#include "audio_cues.h"
#include "platform.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace lis::navigation {
namespace {
std::mutex mutex;
world::Snapshot latest;
ULONGLONG stamp = 0;
std::vector<world::Landmark> landmarks;
bool configured = false, audioReady = false, obstacles = false, beacon = false;
bool wasActive = false, diagnostics = false, previewPending = false, warnedIncomplete = false;
int preview = -1;
ULONGLONG nextObstacle = 0, nextBeacon = 0, previewAt = 0, previewUntil = 0, logAt = 0;
std::string selected;
constexpr int Keys[] = {VK_PRIOR,VK_NEXT,VK_HOME,VK_END,VK_F1,VK_F2,VK_F3};
bool down[7]{};
std::string Utf8(const std::wstring& text) {
    int n = WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    std::string s(n,'\0');
    if(n) WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),s.data(),n,nullptr,nullptr);
    return s;
}
float Distance(const world::Snapshot& s, const world::Target& t) {
    return std::hypot(t.position.x-s.player.x,t.position.y-s.player.y);
}
float Bearing(const world::Snapshot& s, const world::Target& t) {
    return navmath::Bearing(t.position.x-s.player.x,t.position.y-s.player.y,s.cameraYaw);
}
std::wstring Describe(const world::Snapshot& s, const world::Target& t) {
    const float distance=Distance(s,t);
    std::wstring text=t.label+L", "+navmath::Direction(Bearing(s,t))+L", "+navmath::Distance(distance,s.radius);
    if(t.position.z-s.player.z > s.halfHeight*2) text+=L", above";
    else if(s.player.z-t.position.z > s.halfHeight*2) text+=L", below";
    return text;
}
audio::Cue CueFor(world::Kind kind) {
    if(kind==world::Kind::Person) return audio::Cue::Person;
    if(kind==world::Kind::Destination) return audio::Cue::Destination;
    return audio::Cue::Object;
}
const wchar_t* PreviewName(int i) {
    const wchar_t* names[]={L"Obstacle",L"Person. Detection not implemented yet",L"Object. Detection not implemented yet",L"Destination"};
    return names[i];
}
}
void Initialize(HMODULE module) {
    wchar_t path[32768]{}; GetModuleFileNameW(module,path,ARRAYSIZE(path));
    const auto ini=std::filesystem::path(path).parent_path()/L"LISAccess"/L"navigation.ini";
    obstacles=GetPrivateProfileIntW(L"Navigation",L"ObstacleCues",0,ini.c_str())!=0;
    diagnostics=GetPrivateProfileIntW(L"Navigation",L"Diagnostics",0,ini.c_str())!=0;
    wchar_t volumeText[64]{};
    GetPrivateProfileStringW(L"Navigation",L"Volume",L"0.75",volumeText,ARRAYSIZE(volumeText),ini.c_str());
    float volume=.75f;
    try { float v=std::stof(volumeText); if(std::isfinite(v)) volume=std::clamp(v,0.f,1.f); } catch (...) {}
    wchar_t entries[8192]{};
    const DWORD size=GetPrivateProfileSectionW(L"Landmarks",entries,ARRAYSIZE(entries),ini.c_str());
    if(size<ARRAYSIZE(entries)-2) {
        for(const wchar_t* p=entries;*p;p+=wcslen(p)+1) {
            const std::wstring entry(p);const size_t equal=entry.find(L'=');
            if(equal!=std::wstring::npos && equal>0 && equal+1<entry.size() && landmarks.size()<16)
                landmarks.push_back({Utf8(entry.substr(equal+1)),entry.substr(0,equal)});
        }
    } else Log("event=navigation_config_error reason=landmarks_too_long");
    audioReady=audio::Initialize(module);
    if(audioReady) audio::SetMasterGain(volume);
    Log("event=navigation_initialize audio="+std::to_string(audioReady)+" landmarks="+std::to_string(landmarks.size())+" obstacles="+std::to_string(obstacles));
}
void Tick() {
    // EXPERIMENTAL (2026-10-12): local collision cues and bearing-only landmarks;
    // listening/play tests must decide whether this navigation design is useful.
    if(!configured) {world::SetLandmarks(landmarks);configured=true;}
    world::Snapshot sample;
    world::Sample(sample);
    std::lock_guard<std::mutex> lock(mutex);
    latest=std::move(sample);stamp=GetTickCount64();
}
void Pump(bool foreground, bool readableMenu) {
    const auto now=GetTickCount64();
    bool pressed[7]{};
    const bool modified=(GetAsyncKeyState(VK_CONTROL)&0x8000)||(GetAsyncKeyState(VK_MENU)&0x8000)||(GetAsyncKeyState(VK_SHIFT)&0x8000);
    for(size_t i=0;i<7;++i) {
        const bool value=(GetAsyncKeyState(Keys[i])&0x8000)!=0;
        pressed[i]=foreground && !modified && value && !down[i];down[i]=value;
    }
    world::Snapshot s;ULONGLONG at=0;
    {std::lock_guard<std::mutex> lock(mutex);s=latest;at=stamp;}
    const bool fresh=at && now-at<=450;
    const bool active=foreground && fresh && s.active && !readableMenu;
    if(!foreground || (!active && !readableMenu)) {
        audio::SetEnabled(false);previewPending=false;
        if(wasActive) beacon=false;
        if(foreground && !readableMenu && (pressed[4]||pressed[5]||pressed[0]||pressed[1]||pressed[2]||pressed[3]))
            Speak(L"Navigation unavailable here. No collision guidance is available.");
        wasActive=false;
        return;
    }
    if(!active && wasActive) beacon=false;
    wasActive=active;
    if(pressed[6]) {
        preview=(preview+1)%4;
        audio::StopAll();
        if(!audio::Available(static_cast<audio::Cue>(preview))) {
            Speak(L"That cue is missing or invalid. Check the navigation audio files.");previewPending=false;
        } else {
            Speak(PreviewName(preview));previewAt=now+(preview==1||preview==2 ? 2200 : 700);previewPending=true;
        }
    }
    // Preview is explicit, never automatic, and is the only audio allowed in menus.
    if(previewPending) {
        if(now>=previewAt) {
            audio::SetEnabled(true);audio::Play(static_cast<audio::Cue>(preview),0,1);previewPending=false;
            nextObstacle=nextBeacon=previewUntil=now+700;
        }
        return;
    }
    if(now<previewUntil) return;
    if(!active) { if(now>=previewAt+700) audio::SetEnabled(false);return; }
    if(pressed[4]) Speak(L"Navigation prototype. Use the game's movement controls and mouse to turn the camera. F2 toggles obstacle cues. Page Down and Page Up select destinations. Home describes the selected target. End toggles its beacon. Lower-pitched beacon pulses mean behind you. F3 previews sounds. The beacon gives direction, not a clear route. R remains rewind.");
    if(pressed[5]) {
        obstacles=!obstacles;
        if(obstacles && !audio::Available(audio::Cue::Obstacle)) {obstacles=false;Speak(L"Obstacle sound is unavailable.");}
        else Speak(obstacles ? L"Obstacle cues on. They do not guarantee a safe path." : L"Obstacle cues off.");
        nextObstacle=0;warnedIncomplete=false;
    }
    std::sort(s.targets.begin(),s.targets.end(),[&](const auto& a,const auto& b){return Distance(s,a)<Distance(s,b);});
    auto current=std::find_if(s.targets.begin(),s.targets.end(),[&](const auto& t){return t.stableId==selected;});
    if(!selected.empty() && current==s.targets.end()) {
        selected.clear();if(beacon) Speak(L"Target is no longer available. Beacon off.");beacon=false;
    }
    if(pressed[0]||pressed[1]) {
        if(s.targets.empty()) Speak(L"No supported navigation targets here.");
        else {
            int index=current==s.targets.end() ? (pressed[0] ? 0 : -1) : static_cast<int>(current-s.targets.begin());
            index=(index+(pressed[0] ? -1 : 1)+static_cast<int>(s.targets.size()))%static_cast<int>(s.targets.size());
            current=s.targets.begin()+index;selected=current->stableId;
            Speak(Describe(s,*current));nextBeacon=now+650;
        }
    }
    if(pressed[2]) Speak(current==s.targets.end() ? L"No target selected. Use Page Down to select one." : Describe(s,*current));
    if(pressed[3]) {
        if(current==s.targets.end()) Speak(L"No target selected. Use Page Down to select one.");
        else if(!audio::Available(CueFor(current->kind))) Speak(L"The target sound is unavailable.");
        else {beacon=!beacon;Speak(beacon ? L"Beacon on. Direction only, not a clear route." : L"Beacon off.");nextBeacon=now+800;}
    }
    audio::SetEnabled(audioReady && (obstacles||beacon));
    if(diagnostics && now>=logAt) {
        std::string detail="event=navigation_sample x="+std::to_string(s.player.x)+" y="+std::to_string(s.player.y)+" z="+std::to_string(s.player.z)+" yaw="+std::to_string(s.cameraYaw)+" targets="+std::to_string(s.targets.size());
        for(size_t i=0;i<s.probes.size();++i) detail+=" p"+std::to_string(i)+"="+std::to_string(s.probes[i].known)+","+std::to_string(s.probes[i].hit)+","+std::to_string(s.probes[i].distance);
        Log(detail+" drop="+std::to_string(s.dropKnown)+","+std::to_string(s.dropAhead));logAt=now+1000;
    }
    if(obstacles && now>=nextObstacle) {
        const world::Probe* nearest=nullptr;
        bool allKnown=true;
        for(const auto& p:s.probes) {
            allKnown &= p.known;
            if(p.known && p.hit && (!nearest||p.distance<nearest->distance)) nearest=&p;
        }
        const bool uncertain=!allKnown || !s.dropKnown;
        if(uncertain && !warnedIncomplete) {Speak(L"Obstacle readout incomplete. Do not treat silence as clear.");warnedIncomplete=true;}
        if(s.dropKnown && s.dropAhead) {audio::Play(audio::Cue::Obstacle,0,1,.6f);nextObstacle=now+500;}
        else if(nearest) {audio::Play(audio::Cue::Obstacle,navmath::Pan(nearest->angle),1);nextObstacle=now+navmath::Interval(nearest->distance,s.radius*6);}
        else nextObstacle=now+200;
    }
    if(beacon && current!=s.targets.end() && now>=nextBeacon) {
        const float bearing=Bearing(s,*current);
        audio::Play(CueFor(current->kind),navmath::Pan(bearing),.85f,navmath::Behind(bearing) ? .72f : 1.f);
        nextBeacon=now+navmath::Interval(Distance(s,*current),s.radius*50);
    }
}
void Shutdown() {audio::Shutdown();}
}
