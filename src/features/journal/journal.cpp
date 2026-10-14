#include "features/journal/journal.h"
#include "features/journal/native.h"
#include "features/journal/policy.h"
#include "input/devices.h"
#include "output/speech.h"
#include "platform/foreground.h"
#include "platform/log.h"
#include <mutex>

namespace lis::journal {
namespace {
std::mutex mutex;
Frame latest,tabExpected;
ULONGLONG stamp=0,tabAt=0,commandAt=0,keyRepeatAt=0;
int pendingTab=0,command=0;
std::string commandContext,lastState;
bool commandPad=false,faulted=false,faultReported=false;
std::wstring feedback;
input::Devices devices;
Cursor cursor;
PadReader pad;
uint64_t ownedSpeech=0;
std::string waitingContext;
ULONGLONG waitingSince=0;
bool wasForeground=false,waitingSpoken=false;
bool Fresh(ULONGLONG now){return stamp&&now-stamp<=450;}
std::wstring Help(bool gamepad,const Page& p) {
    if(p.inspection)return gamepad?L"Object inspection. A shows the game's text version when available. B returns to the image or closes inspection. D-pad left and right change image pages. Left stick click repeats. Right stick click reads help.":L"Object inspection. Enter shows the game's text version when available. Escape returns to the image or closes inspection. Left and right change image pages. R repeats. F1 reads help.";
    if(p.document)return gamepad?L"Files. A shows the game's text version when available. B returns to the image or goes back. Left stick click repeats. Right stick click reads help.":L"Files. Enter shows the game's text version when available. Escape returns to the image or goes back. Control left and right switch tabs when available. R repeats. F1 reads help.";
    if(gamepad) {
        auto text=L"Diary. Left and right bumpers switch tabs. Left stick click repeats. Right stick click reads help. B goes back.";
        return Join(text,ItemReading(p.kind)?L"D-pad up and down read items. Left and right turn journal pages.":L"Use the native directional controls to select, and A to open.");
    }
    return L"Diary. Control left and right switch tabs. Left and right turn journal pages. Up and down read paragraphs or messages, or select a contact. Enter opens a selection. Escape goes back. R repeats. F1 reads this help.";
}
}
void Initialize(){devices.Initialize();}
void Shutdown(){devices.Shutdown();}
void Tick() {
    Frame expected;int direction=0;ULONGLONG at=0;
    {std::lock_guard<std::mutex> lock(mutex);if(faulted)return;direction=pendingTab;pendingTab=0;expected=std::move(tabExpected);at=tabAt;}
    Frame frame;std::wstring reply;bool ok=false;
    const auto begin=GetTickCount64();
    try {
        if(direction)reply=begin-at>450?L"Journal request expired. Try again.":ChangeTab(expected,direction);
        ok=SampleNative(frame);
    } catch(...){ok=false;}
    const auto state=frame.reason+" page="+frame.page.id+" items="+std::to_string(frame.page.items.size())+" ready="+std::to_string(frame.page.ready);
    if(state!=lastState){lastState=state;Log("event=journal_state "+state+" sample_ms="+std::to_string(GetTickCount64()-begin));}
    std::lock_guard<std::mutex> lock(mutex);
    if(!ok){faulted=true;Log("event=journal_fault status=disabled");}
    latest=std::move(frame);stamp=GetTickCount64();
    if(!reply.empty())feedback=std::move(reply);
}
bool HandleKey(WPARAM key,bool repeated) {
    if(!IsGameForeground()||(GetKeyState(VK_MENU)&0x8000))return false;
    std::lock_guard<std::mutex> lock(mutex);
    const auto now=GetTickCount64();
    if(faulted||!Fresh(now)||!latest.open||latest.blocked||!StillOpen(latest.movie))return false;
    const bool control=(GetKeyState(VK_CONTROL)&0x8000)!=0,shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;
    if(control&&!shift&&(key==VK_LEFT||key==VK_RIGHT)) {
        if(latest.page.inspection)return false;
        if(!repeated&&!pendingTab){pendingTab=key==VK_LEFT?-1:1;tabExpected=latest;tabAt=now;}
        return true;
    }
    if(control||shift)return false;
    int next=key=='R'?3:key==VK_F1?4:0;
    if(latest.page.ready&&ItemReading(latest.page.kind)) {
        if(key==VK_UP)next=1;
        if(key==VK_DOWN)next=2;
    }
    if(!next)return false;
    if(repeated&&(next>=3||now<keyRepeatAt))return true;
    keyRepeatAt=now+180;command=next;commandPad=false;commandAt=now;commandContext=latest.identity;
    return true;
}
bool Pump(bool foreground) {
    Frame frame;std::wstring speech;int action=0;bool byPad=false,failed=false;ULONGLONG at=0,requestedAt=0;std::string expected;
    {std::lock_guard<std::mutex> lock(mutex);frame=latest;at=stamp;failed=faulted;action=command;command=0;
        byPad=commandPad;requestedAt=commandAt;expected=commandContext;speech.swap(feedback);
        if(!foreground)pendingTab=0;}
    const auto now=GetTickCount64();
    const bool fresh=at&&now-at<=450;
    const bool open=fresh&&frame.open;
    const bool enabled=foreground&&open&&!frame.blocked&&!failed;
    uint16_t buttons=0;const bool connected=devices.ReadButtons(frame.controllerIndex,buttons);
    const int padAction=pad.Update(buttons,enabled,connected,now);
    if(padAction&&(padAction>=3||(frame.page.ready&&ItemReading(frame.page.kind)))) {
        action=padAction;byPad=true;requestedAt=now;expected=frame.identity;
    }
    if(!enabled) {
        if(ownedSpeech){StopSpeechIfCurrent(ownedSpeech);ownedSpeech=0;}
        if(!open){cursor.Reset();waitingContext.clear();waitingSpoken=false;}
        if(foreground&&failed&&!faultReported){Speak(L"Diary accessibility failed and has been disabled. See the mod log.");faultReported=true;}
        wasForeground=foreground;return open;
    }
    if(waitingContext!=frame.identity){waitingContext=frame.identity;waitingSince=now;waitingSpoken=false;}
    if(frame.page.ready) {
        auto automatic=cursor.Update(frame.page,frame.identity);
        if(speech.empty())speech=std::move(automatic);
        if(!wasForeground&&speech.empty())speech=cursor.Describe(frame.page);
    } else if(now-waitingSince>=1500&&!waitingSpoken) {
        waitingSpoken=true;speech=Join(frame.page.title,L"This page is not readable yet. You can go back.");
    }
    if(action&&now-requestedAt<=450&&expected==frame.identity) {
        if(action==4)speech=Help(byPad,frame.page);
        else if(!frame.page.ready)speech=L"This page is not ready to read.";
        else if(action==3)speech=cursor.Describe(frame.page);
        else speech=cursor.Move(frame.page,action==1?-1:1);
    }
    if(!speech.empty()){Speak(speech,true);ownedSpeech=SpeechSerial();}
    wasForeground=foreground;return true;
}
}
