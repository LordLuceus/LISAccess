#include "features/navigation/navigation.h"
#include "features/navigation/math.h"
#include "features/navigation/description.h"
#include "features/world/world.h"
#include "features/interactions/actions.h"
#include "features/camera/facing.h"
#include "features/prompts/prompts.h"
#include "features/dialogue/dialogue.h"
#include "features/rewind/rewind.h"
#include "output/audio_cues.h"
#include "output/speech.h"
#include "platform/log.h"
#include "input/devices.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace lis::navigation {
using namespace detail;
namespace {
std::mutex mutex;
world::Snapshot latest;
ULONGLONG stamp = 0;
std::vector<world::Landmark> landmarks;
bool configured = false, audioReady = false, obstacles = false, beacon = false;
bool wasActive = false, diagnostics = false, previewPending = false, warnedIncomplete = false;
bool warnedInteractions=false,warnedExits=false;
int preview = -1;
ULONGLONG nextObstacle = 0, nextBeacon = 0, previewAt = 0, logAt = 0;
std::string selected;
input::Devices devices;
int pendingAction=-1;
std::string pendingFocus,pendingFace,seenFocus,spokenFocus;
std::string aimingAt,faceReply;
bool faceSubmitted=false;
ULONGLONG aimDeadline=0;
std::wstring feedback;
ULONGLONG focusSince=0;
std::string Utf8(const std::wstring& text) {
    int n = WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    std::string s(n,'\0');
    if(n) WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),s.data(),n,nullptr,nullptr);
    return s;
}
}
void Initialize(HMODULE module) {
    devices.Initialize();
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
    int action=-1;std::string expected,face;
    {std::lock_guard<std::mutex> lock(mutex);action=pendingAction;pendingAction=-1;expected.swap(pendingFocus);face.swap(pendingFace);}
    std::wstring reply;bool submitted=false;
    if(action>=0) {
        reply=world::ActivateFocused(expected,static_cast<unsigned>(action));
        sample.focusedId.clear();sample.focusedLabel.clear();sample.actions={};
    }
    else if(!face.empty())reply=world::FaceTarget(face,submitted);
    std::lock_guard<std::mutex> lock(mutex);
    if(!face.empty()){faceReply=face;faceSubmitted=submitted;}
    if(!reply.empty())feedback=std::move(reply);
    latest=std::move(sample);stamp=GetTickCount64();
}
void Pump(bool foreground, bool readableMenu) {
    const auto now=GetTickCount64();
    auto pressed=devices.ReadKeyboard(foreground,now);
    world::Snapshot s;ULONGLONG at=0;
    std::wstring reply;std::string completedFace;bool submitted=false;
    {std::lock_guard<std::mutex> lock(mutex);s=latest;at=stamp;reply.swap(feedback);completedFace.swap(faceReply);submitted=faceSubmitted;}
    if(completedFace==aimingAt&&!submitted)aimingAt.clear();
    if(foreground&&!reply.empty())Speak(reply);
    const bool fresh=at && now-at<=450;
    const bool active=foreground && fresh && s.active && !s.interactionBusy && !readableMenu;
    const auto padPresses=devices.ReadController(s.controllerIndex,active||(foreground&&!readableMenu&&(dialogue::HasContext()||rewind::HasContext())),now,foreground&&!readableMenu);
    std::optional<bool> commandGamepad;
    if(std::any_of(padPresses.begin(),padPresses.end(),[](bool value){return value;})){s.gamepad=true;commandGamepad=true;}
    else if(std::any_of(pressed.begin(),pressed.end(),[](bool value){return value;})){s.gamepad=false;commandGamepad=false;}
    for(size_t i=0;i<input::Count;++i)pressed[i]=pressed[i]||padPresses[i];
    if(rewind::HandleInput(pressed,foreground,readableMenu,commandGamepad.value_or(false))||dialogue::HandleInput(pressed,foreground,readableMenu,commandGamepad)) {
        audio::CancelPreview();audio::SetEnabled(false);previewPending=false;beacon=false;wasActive=false;
        seenFocus.clear();spokenFocus.clear();aimingAt.clear();
        return;
    }
    if(foreground&&!readableMenu&&pressed[static_cast<size_t>(input::Command::RepeatPrompt)])prompts::RequestRepeat();
    if(foreground&&s.interactionsFailed&&!warnedInteractions){Speak(L"Interaction scanning failed and has been disabled. Navigation is still available.");warnedInteractions=true;}
    if(foreground&&s.exitsFailed&&!warnedExits){Speak(L"Exit-area scanning failed and has been disabled. Other scanner targets remain available.");warnedExits=true;}
    if(!foreground || (!active && !readableMenu)) {
        audio::CancelPreview();audio::SetEnabled(false);previewPending=false;
        if(wasActive) beacon=false;
        seenFocus.clear();spokenFocus.clear();
        aimingAt.clear();
        if(foreground && !readableMenu && (pressed[4]||pressed[5]||pressed[0]||pressed[1]||pressed[2]||pressed[3]))
            Speak(L"Navigation unavailable here. No collision guidance is available.");
        wasActive=false;
        return;
    }
    if(!active && wasActive) beacon=false;
    wasActive=active;
    if(active) {
        if(!aimingAt.empty()) {
            if(s.focusedId==aimingAt)aimingAt.clear();
            else if(now>=aimDeadline){Speak(L"That target did not become focused. Try moving closer or aiming manually.");aimingAt.clear();}
        }
        std::string signature=s.focusedId;
        if(!signature.empty())signature+='|'+Utf8(FocusText(s));
        if(signature!=seenFocus){seenFocus=signature;focusSince=now;if(signature.empty())spokenFocus.clear();}
        if(!seenFocus.empty()&&spokenFocus!=seenFocus&&now-focusSince>=200&&std::none_of(pressed.begin(),pressed.end(),[](bool value){return value;})) {
            Speak(FocusText(s));spokenFocus=seenFocus;
        }
        for(unsigned slot=0;slot<4;++slot)if(pressed[8+slot]) {
            if(!aimingAt.empty()||pressed[7])Speak(L"Wait for the target's focused-object announcement before choosing an action.");
            else if(s.focusedId.empty()||s.actions[slot].label.empty())Speak(L"That action is not available. Face an object to hear its actions.");
            else {std::lock_guard<std::mutex> lock(mutex);pendingFocus=s.focusedId;pendingAction=static_cast<int>(slot);}
            break;
        }
    }
    if(pressed[6]) {
        preview=NextPreview(preview);
        audio::StopAll();
        if(!audio::Available(static_cast<audio::Cue>(preview))) {
            Speak(L"That cue is missing or invalid. Check the audio files.");previewPending=false;
        } else {
            Speak(PreviewName(preview));previewAt=now+700;previewPending=true;
        }
    }
    // Preview is explicit, never automatic, and is the only audio allowed in menus.
    if(previewPending) {
        if(now>=previewAt) {
            if(!audio::Preview(static_cast<audio::Cue>(preview)))Speak(L"Sound preview is unavailable.");
            previewPending=false;nextObstacle=nextBeacon=now+700;
        }
        return;
    }
    if(audio::PreviewPlaying())return;
    audio::CancelPreview();
    if(!active) {audio::SetEnabled(false);return;}
    if(pressed[4]) Speak(L"Page Up and Page Down select scanner targets. Home describes. Shift Home toggles the beacon. Control Home repeats the latest prompt. F faces the selected target. Number keys 1 through 4 activate the focused object's announced actions. On a controller: D pad left and right select targets, up describes, down toggles the beacon. Click the left stick to face. Native Y, X, B and A keep their announced actions. F2 toggles obstacle cues; F3 previews sounds. Hold the left stick and press D pad down for obstacle cues, up for help, left for sound previews, or right to repeat the latest prompt. Beacons give direction, not a safe route. R remains rewind.");
    if(pressed[5]) {
        obstacles=!obstacles;
        if(obstacles && !audio::Available(audio::Cue::Obstacle)) {obstacles=false;Speak(L"Obstacle sound is unavailable.");}
        else Speak(obstacles ? L"Obstacle cues on. They do not guarantee a safe path." : L"Obstacle cues off.");
        nextObstacle=0;warnedIncomplete=false;
    }
    // Sample already provides the distance-sorted, disambiguated list. Selection
    // follows stableId across reordering rather than following the old index.
    auto current=std::find_if(s.targets.begin(),s.targets.end(),[&](const auto& t){return t.stableId==selected;});
    if(!selected.empty() && current==s.targets.end()) {
        selected.clear();if(beacon) Speak(L"Target is no longer available. Beacon off.");beacon=false;
    }
    if(pressed[0]||pressed[1]) {
        if(s.targets.empty()) Speak(s.interactionStatus=="ready" ? L"No supported navigation targets here." : L"Interaction scanning is unavailable. No navigation landmarks here.");
        else {
            int index=current==s.targets.end() ? (pressed[0] ? 0 : -1) : static_cast<int>(current-s.targets.begin());
            index=(index+(pressed[0] ? -1 : 1)+static_cast<int>(s.targets.size()))%static_cast<int>(s.targets.size());
            current=s.targets.begin()+index;selected=current->stableId;
            Speak(Describe(s,*current));nextBeacon=now+650;
        }
    }
    if(pressed[2]) {
        if(current!=s.targets.end())Speak(Describe(s,*current));
        else if(!s.focusedId.empty())Speak(FocusText(s));
        else if(s.interactionStatus!="ready")Speak(L"Interaction scanning is unavailable.");
        else Speak(s.gamepad ? L"No target selected. Use D pad left or right." : L"No target selected. Use Page Down to select one.");
    }
    if(pressed[7]) {
        if(current==s.targets.end())Speak(L"No target selected. Select one first.");
        else {
            if(world::NeedsInteractionFocus(current->kind)&&current->stableId!=s.focusedId){aimingAt=current->stableId;aimDeadline=now+1500;}
            std::lock_guard<std::mutex> lock(mutex);pendingAction=-1;pendingFocus.clear();pendingFace=current->stableId;
        }
    }
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
void Shutdown() {audio::Shutdown();devices.Shutdown();}
}
