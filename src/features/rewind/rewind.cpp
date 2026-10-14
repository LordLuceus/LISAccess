#include "features/rewind/rewind.h"
#include "features/rewind/native.h"
#include "features/prompts/prompts.h"
#include "output/audio_cues.h"
#include "output/speech.h"
#include "platform/log.h"
#include <Windows.h>
#include <mutex>

namespace lis::rewind {
namespace {
std::mutex mutex;
Tracker tracker;
Frame latest;
Feedback pending;
uint64_t stamp=0,cueUntil=0,pendingAt=0;
DWORD thread=0;
bool faulted=false,faultReported=false;
std::wstring explicitSpeech;
bool Fresh(){const auto now=GetTickCount64();return stamp&&now>=stamp&&now-stamp<=250;}
bool Context(){return !faulted&&Fresh()&&latest.context&&(Rewinding(latest)||tracker.Running());}
}
void Tick() {
    if(!thread)thread=GetCurrentThreadId();if(thread!=GetCurrentThreadId()||faulted)return;
    Frame frame;bool ok=false;
    try{ok=Sample(frame);}catch(...){ok=false;}
    const auto now=GetTickCount64();
    std::lock_guard<std::mutex> lock(mutex);
    if(!ok){faulted=true;tracker.Reset();pending={};return;}
    const bool wasRunning=tracker.Running();
    const auto result=tracker.Observe(frame,now);
    if(!frame.context||!frame.valid||(Rewinding(frame)&&!frame.visible)||(!wasRunning&&tracker.Running()))pending={};
    if(result.cue==Cue::Crossed||pending.cue==Cue::None)pending.cue=result.cue;
    if(!result.speech.empty())pending.speech=result.speech;
    if(result.cue!=Cue::None||!result.speech.empty())pendingAt=now;
    latest=frame;stamp=now;
}
bool HasContext(){std::lock_guard<std::mutex> lock(mutex);return Context();}
bool HandleInput(const input::Presses& pressed,bool foreground,bool readableMenu,bool gamepad) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!foreground||readableMenu||!Context())return false;
    if(pressed[static_cast<size_t>(input::Command::Describe)]||pressed[static_cast<size_t>(input::Command::RepeatPrompt)])
        explicitSpeech=latest.state==3?L"Timeline moving forward. Rewind readout will follow.":tracker.Describe();
    if(pressed[static_cast<size_t>(input::Command::Help)])
        explicitSpeech=std::wstring(L"Rewind. The double tick warns near the starting last-action mark; the falling cue means it was reached or passed. ")+
            (gamepad?L"D pad up reads the gauge. ":L"Home reads the gauge. ")+L"Rewinding is not stopped automatically.";
    return true;
}
void Pump(bool foreground,bool readableMenu) {
    Feedback output;bool explicitRead=false,enable=false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(!foreground||readableMenu) {
            tracker.Reset();pending={};explicitSpeech.clear();cueUntil=0;
        } else if(faulted) {
            if(!faultReported){output.speech=L"Rewind feedback failed and has been disabled.";faultReported=true;}
        } else if(!Fresh()||!latest.context) {
            tracker.Reset();pending={};explicitSpeech.clear();cueUntil=0;
        } else {
            if(!pendingAt||GetTickCount64()-pendingAt>250)pending={};
            output=std::move(pending);pending={};
            if(!explicitSpeech.empty()){output.speech.swap(explicitSpeech);explicitSpeech.clear();explicitRead=true;}
            if(output.cue!=Cue::None)cueUntil=GetTickCount64()+2000; // maximum supported replacement WAV duration
            enable=tracker.Running()||GetTickCount64()<cueUntil;
        }
    }
    audio::SetRewindEnabled(enable);
    if(output.cue!=Cue::None) {
        const auto cue=output.cue==Cue::Crossed?audio::Cue::RewindCrossed:audio::Cue::RewindApproach;
        if(!audio::Play(cue,0,1))Speak(output.cue==Cue::Crossed?L"Last-action mark reached or passed.":L"Approaching last-action mark.");
    }
    if(!output.speech.empty()) {
        Speak(output.speech,explicitRead);
        prompts::RememberReadout(output.speech);
    }
}
void Shutdown(){audio::SetRewindEnabled(false);std::lock_guard<std::mutex> lock(mutex);tracker.Reset();pending={};explicitSpeech.clear();}
}
