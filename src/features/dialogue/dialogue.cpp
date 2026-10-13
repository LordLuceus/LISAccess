#include "features/dialogue/dialogue.h"
#include "features/dialogue/native.h"
#include "platform/log.h"
#include "output/speech.h"
#include <Windows.h>
#include <mutex>

namespace lis::dialogue {
namespace {
std::mutex mutex;
NativeFrame latest;
Snapshot requested;
ULONGLONG stamp=0,requestedAt=0;
int pending=-1;
uint64_t ownedSpeech=0;
bool faulted=false,faultReported=false;
std::wstring feedback,explicitSpeech,lastDescription,lastProblem;
std::string lastIdentity,lastState;
constexpr bool TraceDialogue=false; // optional state diagnostics; initial dialogue playtest accepted
bool Fresh(ULONGLONG now) {return stamp&&now-stamp<=450;}
}
void Tick() {
    int action=-1;Snapshot expected;ULONGLONG at=0;
    {std::lock_guard<std::mutex> lock(mutex);if(faulted)return;action=pending;pending=-1;expected=std::move(requested);at=requestedAt;}
    NativeFrame frame;std::wstring reply;bool ok=false;
    try {
        if(action>=0)reply=GetTickCount64()-at>450 ? L"The choice request expired. Read the choices and press again." : ActivateNative(expected,static_cast<unsigned>(action));
        ok=SampleNative(frame);
    } catch(...) {ok=false;}
    if constexpr(TraceDialogue) {
        const std::string state="context="+std::to_string(frame.view.context)+" visible="+std::to_string(frame.view.visible)+
            " blocked="+std::to_string(frame.blocked)+" timed="+std::to_string(frame.view.timed)+
            " identity="+frame.view.identity+" read_error="+std::to_string(!frame.problem.empty())+" reason="+frame.reason;
        if(state!=lastState){lastState=state;Log("event=dialogue_state "+state);}
        if(action>=0)Log("event=dialogue_request slot="+std::to_string(action)+" submitted="+std::to_string(reply.empty()));
    }
    std::lock_guard<std::mutex> lock(mutex);
    if(!ok){faulted=true;Log("event=dialogue_fault status=disabled");}
    if(!reply.empty())feedback=std::move(reply);
    latest=std::move(frame);stamp=GetTickCount64();
}
bool HasContext() {
    std::lock_guard<std::mutex> lock(mutex);
    return !faulted&&Fresh(GetTickCount64())&&latest.view.context&&!latest.blocked;
}
bool HandleInput(const input::Presses& pressed,bool foreground,bool readableMenu,std::optional<bool> gamepad) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!foreground||readableMenu){pending=-1;explicitSpeech.clear();return false;}
    if(faulted||!Fresh(GetTickCount64())||!latest.view.context)return false;
    // The current command wins over the up-to-200ms-old native sample, just as
    // object actions do. The next game-thread snapshot resumes native detection.
    if(gamepad.has_value())latest.view.gamepad=*gamepad;
    const auto description=Describe(latest.view);
    const auto repeat=pressed[static_cast<size_t>(input::Command::RepeatPrompt)];
    if(pressed[static_cast<size_t>(input::Command::Help)]) {
        explicitSpeech=latest.view.gamepad
            ? L"Dialogue. Press the announced face button to choose. Hold left stick click and press D-pad right to repeat the choices."
            : L"Dialogue. Press the announced number to choose. Control Home repeats the choices. R keeps the game's rewind control.";
        if(!description.empty())explicitSpeech+=L"\n"+description;
    } else if(repeat)explicitSpeech=description.empty()?L"No dialogue choices are available right now.":description;
    int slot=-1,count=0;
    for(int i=0;i<4;++i)if(pressed[static_cast<size_t>(input::Command::Action1)+i]){slot=i;++count;}
    if(count) {
        if(count!=1)explicitSpeech=L"Press only one dialogue choice key at a time.";
        else if(pending>=0)explicitSpeech=L"A dialogue choice is already pending.";
        else if(!CanActivate(latest.view,latest.view,slot,true,latest.blocked))explicitSpeech=L"That dialogue choice is not available.";
        else {pending=slot;requested=latest.view;requestedAt=GetTickCount64();}
    }
    return true; // never run scanner/facing commands against a dialogue actor
}
void Pump(bool foreground,bool readableMenu) {
    std::wstring speech;
    uint64_t stop=0;
    bool ownsChoices=false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(!foreground||readableMenu) {
            stop=ownedSpeech;ownedSpeech=0;
            lastIdentity.clear();lastDescription.clear();lastProblem.clear();explicitSpeech.clear();feedback.clear();pending=-1;
        }
        else if(faulted) {
            stop=ownedSpeech;ownedSpeech=0;
            if(!faultReported){faultReported=true;speech=L"Dialogue accessibility failed and has been disabled. See the mod log.";}
        }
        else if(!feedback.empty())speech.swap(feedback);
        else if(!explicitSpeech.empty()) {
            speech.swap(explicitSpeech);lastIdentity=latest.view.identity;lastDescription=Describe(latest.view);
            ownsChoices=latest.view.visible;
        } else if(!Fresh(GetTickCount64())||!latest.view.visible) {
            stop=ownedSpeech;ownedSpeech=0;
            lastIdentity.clear();lastDescription.clear();
            if(Fresh(GetTickCount64())&&!latest.problem.empty()&&latest.problem!=lastProblem)speech=latest.problem;
            lastProblem=latest.problem;
        } else if(pending<0) {
            const auto description=Describe(latest.view);
            if(latest.view.identity!=lastIdentity||description!=lastDescription) {
                speech=description;lastIdentity=latest.view.identity;lastDescription=description;lastProblem.clear();
                ownsChoices=true;
            }
        }
    }
    StopSpeechIfCurrent(stop);
    if(!speech.empty()){Speak(speech);ownedSpeech=ownsChoices?SpeechSerial():0;} // choices/repeat take priority over older prompt speech
}
}
