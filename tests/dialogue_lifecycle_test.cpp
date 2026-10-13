#include "features/dialogue/dialogue.h"
#include "features/dialogue/native.h"
#include "output/speech.h"
#include <Windows.h>
#include <iostream>
#include <vector>

namespace {
lis::dialogue::NativeFrame source;
bool sampleOk=true;
int deliveries=0,attempts=0,stops=0;
uint64_t serial=0;
std::vector<std::wstring> spoken;
lis::input::Presses Press(lis::input::Command c) {
    lis::input::Presses p{};p[static_cast<size_t>(c)]=true;return p;
}
void Choices() {
    source={};source.blocked=false;source.view.context=source.view.visible=true;
    source.view.identity="E1_2A_ArtClass_CHJefferson_Phase01|page=0|sequence=1";
    source.view.choices[1]={L"$GT_HUD_Dial_E1_2A_ArtClass_CHJefferson_TalkJefferson_003",L"You're asking me?",true};
    source.view.choices[2]={L"$GT_HUD_Dial_E1_2A_ArtClass_CHJefferson_TalkJefferson_004",L"I did know...",true};
}
void Pump(){lis::dialogue::Pump(true,false);}
}
namespace lis {
void Log(const std::string&) {}
void Speak(const std::wstring& text,bool) {spoken.push_back(text);++serial;}
uint64_t SpeechSerial(){return serial;}
void StopSpeechIfCurrent(uint64_t expected){if(expected&&expected==serial){++stops;++serial;}}
}
namespace lis::dialogue {
bool SampleNative(NativeFrame& frame){frame=source;return sampleOk;}
std::wstring ActivateNative(const Snapshot& expected,unsigned slot) {
    ++attempts;
    if(!CanActivate(expected,source.view,static_cast<int>(slot),true,source.blocked))return L"Choices changed.";
    ++deliveries;source.view.visible=false;return {};
}
}
int main() {
    using namespace lis::dialogue;using lis::input::Command;
    int errors=0;auto check=[&](bool ok,const char* what){std::cout<<(ok?"PASS: ":"FAIL: ")<<what<<'\n';errors+=!ok;};
    Choices();Tick();Pump();const auto first=spoken.size();Pump();
    check(first==1&&spoken.size()==first&&deliveries==0,"opening announces once and never selects automatically");
    HandleInput(Press(Command::RepeatPrompt),true,false);Pump();
    check(spoken.size()==first+1&&deliveries==0,"repeat reads choices without activation");
    source.view.gamepad=true;Tick();Pump();const auto switched=spoken.size();Pump();
    check(spoken.back()==L"X: You're asking me?\nB: I did know..."&&spoken.size()==switched,"native controller-mode change updates speech once, without idle repetition");
    HandleInput(Press(Command::RepeatPrompt),true,false,false);Pump();
    check(spoken.back()==L"2: You're asking me?\n3: I did know...","keyboard repeat immediately overrides the older controller snapshot");
    HandleInput(Press(Command::Help),true,false,true);Pump();
    check(spoken.back().find(L"face button")!=std::wstring::npos&&spoken.back().find(L"Control Home")==std::wstring::npos,"controller help speaks only controller commands");
    HandleInput(Press(Command::Help),true,false,false);Pump();
    check(spoken.back().find(L"Control Home")!=std::wstring::npos&&spoken.back().find(L"face button")==std::wstring::npos&&deliveries==0,"keyboard help speaks only keyboard commands and never selects");
    source.view.gamepad=false;Tick();
    HandleInput(Press(Command::Action1),true,false);Tick();Pump();
    check(deliveries==0&&spoken.back()==L"That dialogue choice is not available.","unoffered key never reaches native input");
    HandleInput(Press(Command::Action2),true,false);Pump();source.view.gamepad=true;Tick();Pump();
    check(deliveries==1&&attempts==1,"explicit offered key sends exactly one native request");
    HandleInput(Press(Command::RepeatPrompt),true,false);Pump();const auto unavailableStops=stops;
    check(spoken.back()==L"No dialogue choices are available right now.","repeat never reads the previous closed question");
    Pump();check(stops==unavailableStops,"unavailable feedback is not immediately cancelled as obsolete choice speech");
    Choices();Tick();const auto beforeReopen=spoken.size();Pump();
    check(spoken.size()==beforeReopen+1,"reopening the same question announces again");
    HandleInput(Press(Command::Action2),true,false);source.view.identity+="|new-node";Tick();Pump();
    check(deliveries==1&&attempts==2&&spoken.back()==L"Choices changed.","game-thread revalidation rejects a queued stale question");
    Tick();HandleInput(Press(Command::Action3),true,false);HandleInput({},false,false);lis::dialogue::Pump(false,false);Tick();
    check(deliveries==1&&attempts==2,"losing foreground cancels pending selection");
    check(!HandleInput(Press(Command::Action3),true,true),"readable menu owns input instead of dialogue");
    lis::dialogue::Pump(true,true);
    Tick();HandleInput(Press(Command::Action3),true,false);Sleep(500);Tick();Pump();
    check(deliveries==1&&attempts==2&&spoken.back().find(L"expired")!=std::wstring::npos,"stalled requests expire without reaching the native handler");
    Choices();Tick();Pump();const int beforeStop=stops;source.view.visible=false;Tick();Pump();
    check(stops==beforeStop+1,"closing choices cancels their still-current readout");
    Choices();Tick();Pump();lis::Speak(L"New menu message");const int beforeMenu=stops;source.view.visible=false;Tick();Pump();
    check(stops==beforeMenu,"closing choices does not cancel a newer announcement");
    Choices();Tick();Pump();sampleOk=false;Tick();Pump();const auto failedCount=spoken.size();const int failedStops=stops;Pump();Pump();
    check(spoken.size()==failedCount&&spoken.back().find(L"disabled")!=std::wstring::npos&&stops==failedStops,"fault reports once without reviving partial choices or cancelling its own warning");
    return errors?1:0;
}
