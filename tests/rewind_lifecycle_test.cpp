#include "features/rewind/rewind.h"
#include "features/rewind/native.h"
#include "output/audio_cues.h"
#include "output/speech.h"
#include <Windows.h>
#include <iostream>
#include <vector>
namespace {
lis::rewind::Frame source;
bool sampleOk=true,audioOk=true,enabled=false;
std::vector<std::wstring> speech;
std::vector<lis::audio::Cue> cues;
std::wstring remembered;
void State(unsigned state,float time,float mark=60) {
    source={};source.context=source.valid=source.visible=source.markerKnown=source.markerPresent=true;
    source.manager=1;source.world=2;source.controller=3;source.state=state;source.reference=time;source.end=100;source.marker=mark;
}
void Sample(){lis::rewind::Tick();lis::rewind::Pump(true,false);}
}
namespace lis {void Log(const std::string&){} void Speak(const std::wstring& text,bool){speech.push_back(text);}}
namespace lis::rewind {bool Sample(Frame& f){f=source;return sampleOk;}}
namespace lis::prompts {void RememberReadout(const std::wstring& text){remembered=text;}}
namespace lis::audio {
void SetRewindEnabled(bool value){enabled=value;}
bool Play(Cue c,float,float,float){if(!audioOk||!enabled)return false;cues.push_back(c);return true;}
}
int main() {
    using namespace lis::rewind;int errors=0;
    auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS: ":"FAIL: ")<<label<<'\n';errors+=!ok;};
    State(2,100);Sample();check(speech.empty()&&cues.empty()&&!HasContext(),"ordinary paused choice remains silent");
    State(1,64);Sample();check(cues.size()==1&&cues.back()==lis::audio::Cue::RewindApproach&&speech.empty(),"approach cue is dispatched without chatter");
    lis::input::Presses pressed{};pressed[static_cast<size_t>(lis::input::Command::Describe)]=true;
    check(HandleInput(pressed,true,false),"rewind owns existing status command");Pump(true,false);
    check(speech.back().find(L"64 percent")!=std::wstring::npos,"explicit status reads current gauge");
    lis::input::Presses help{};help[static_cast<size_t>(lis::input::Command::Help)]=true;
    HandleInput(help,true,false,true);Pump(true,false);
    check(speech.back().find(L"D pad up")!=std::wstring::npos&&speech.back().find(L"Home")==std::wstring::npos,"help names only the active device's status control");
    State(1,59,30);Sample();check(cues.back()==lis::audio::Cue::RewindCrossed,"crossing reaches independent rewind audio group");
    State(0,59,30);Sample();const auto n=speech.size();Pump(true,false);
    check(speech.size()==n&&remembered.find(L"59 percent")!=std::wstring::npos&&!HasContext(),"one completion readout is retained for repeat");
    State(1,64);Tick();Pump(false,false);Pump(true,false);
    check(!enabled&&speech.size()==n,"focus loss drops pending approach and readout");
    State(1,64);Tick();Pump(true,true);check(!enabled&&!HandleInput(pressed,true,true),"readable menus keep input and silence rewind audio");
    State(0,100);Sample();State(1,64);Tick();State(1,59,30);Tick();const auto before=cues.size();Pump(true,false);
    check(cues.size()==before+1&&cues.back()==lis::audio::Cue::RewindCrossed,"pending crossing supersedes an unplayed approach");
    Pump(false,false);audioOk=false;State(0,100);Sample();State(1,64);Sample();
    check(speech.back()==L"Approaching last-action mark.","failed cue playback falls back to speech");
    State(1,59,30);Tick();Sleep(275);const auto stale=speech.size();Pump(true,false);
    check(speech.size()==stale&&!enabled,"stale snapshots cannot play delayed warnings");
    audioOk=true;State(0,100);Sample();State(1,64);Tick();const auto oldCues=cues.size();Sleep(275);State(1,63);Tick();Pump(true,false);
    check(cues.size()==oldCues,"fresh snapshots cannot make an old queued warning fresh again");
    sampleOk=false;Tick();Pump(true,false);const auto failed=speech.size();Pump(true,false);
    check(speech.back().find(L"failed")!=std::wstring::npos&&speech.size()==failed,"reader failure is explicit and one-shot");
    Shutdown();return errors?1:0;
}
