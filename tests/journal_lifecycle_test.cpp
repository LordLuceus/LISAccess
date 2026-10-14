#include "features/journal/journal.h"
#include "features/journal/native.h"
#include "input/devices.h"
#include "output/speech.h"
#include <iostream>
#include <vector>
namespace {
lis::journal::Frame frame;
bool foreground=true,sampleOk=true,connected=true;
uint16_t buttons=0;
uint64_t serial=0;
int stops=0;
std::vector<std::wstring> spoken;
}
namespace lis {
bool IsGameForeground(){return foreground;}
void Log(const std::string&){}
void Speak(const std::wstring& text,bool){spoken.push_back(text);++serial;}
uint64_t SpeechSerial(){return serial;}
void StopSpeechIfCurrent(uint64_t token){if(token&&token==serial)++stops;}
}
namespace lis::journal {
bool SampleNative(Frame& out){out=frame;return sampleOk;}
bool StillOpen(uintptr_t){return frame.open;}
std::wstring ChangeTab(const Frame&,int){return {};}
}
namespace lis::input {
void Devices::Initialize(){}
void Devices::Shutdown(){}
bool Devices::ReadButtons(int,uint16_t& out){out=buttons;return connected;}
}
int main() {
    int errors=0;auto check=[&](bool ok,const char* what){std::cout<<(ok?"PASS: ":"FAIL: ")<<what<<'\n';errors+=!ok;};
    using namespace lis::journal;
    Initialize();
    frame.open=true;frame.movie=1;frame.identity="sms";
    frame.page.ready=true;frame.page.kind=View::Messages;frame.page.title=L"Messages";frame.page.selected=1;
    frame.page.items={{"a",L"Old message."},{"b",L"Latest message."}};
    Tick();check(Pump(true)&&spoken.size()==1&&spoken.back().find(L"Latest")!=std::wstring::npos,"open diary blocks world and announces latest message");
    Pump(true);check(spoken.size()==1,"worker polling is silent for unchanged snapshot");
    buttons=1;Pump(true);check(spoken.back().find(L"Old message")!=std::wstring::npos,"controller up reads older message");
    auto count=spoken.size();foreground=false;buttons=0x40;
    Pump(false);check(spoken.size()==count&&stops==1,"focus loss cancels own speech and drops commands");
    foreground=true;Pump(true);check(spoken.size()==count+1&&spoken.back().find(L"Old message")!=std::wstring::npos,"focus return preserves reading cursor");
    buttons=0;Pump(true);buttons=0x80;Pump(true);
    check(spoken.back().find(L"bumpers")!=std::wstring::npos&&spoken.back().find(L"Control left")==std::wstring::npos,"controller help only describes controller bindings");
    frame.blocked=true;Tick();count=spoken.size();buttons=0;Pump(true);buttons=2;Pump(true);
    check(spoken.size()==count,"modal blocks reading commands");
    frame.blocked=false;Tick();buttons=0;Pump(true);
    // A newer output owns the channel when the diary closes.
    lis::Speak(L"Newer menu readout.",true);const int before=stops;
    frame.open=false;Tick();check(!Pump(true)&&stops==before,"closing does not stop a newer unrelated announcement");
    frame.open=true;frame.page.ready=false;frame.identity="unknown";Tick();
    check(Pump(true),"unreadable diary page still blocks scanner commands");
    frame.page={};frame.page.ready=true;frame.page.document=true;frame.page.inspection=true;
    frame.page.title=L"Object inspection";frame.page.selected=0;frame.page.items={{"poster",L"Poster transcription."}};
    frame.identity="inspection";Tick();buttons=0;
    check(Pump(true)&&spoken.back().find(L"Object inspection")!=std::wstring::npos,"world inspection owns input and announces its own context");
    buttons=0x80;Pump(true);
    check(spoken.back().find(L"Object inspection")!=std::wstring::npos&&spoken.back().find(L"diary")==std::wstring::npos&&
        spoken.back().find(L"Files")==std::wstring::npos&&spoken.back().find(L"A shows")!=std::wstring::npos,"inspection help uses native text-toggle controls, not diary tabs");
    frame.page.ready=true;Tick();Pump(true);
    Sleep(475);count=spoken.size();buttons=0x40;
    check(!Pump(true)&&spoken.size()==count,"stale snapshot cannot read or activate");
    Tick();buttons=0;Pump(true);sampleOk=false;Tick();Pump(true);count=spoken.size();Tick();Pump(true);
    check(count==spoken.size()&&spoken.back().find(L"failed")!=std::wstring::npos,"native fault is latched and reported once");
    Shutdown();return errors?1:0;
}
