#include "output/speech.h"
#include <prism.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

struct PrismContext {};
struct PrismBackend {size_t index;};
namespace {
struct Reader {
    PrismBackendId id;const char* name;int priority;bool available,braille;
    PrismError speakResult=PRISM_OK,brailleResult=PRISM_OK;
    bool fault=false;int speaks=0,brailles=0,stops=0,inits=0;
    std::string text,brailleText;bool interrupt=false;
};
// Deliberately unsorted, with an unknown provider to prove registry-based routing.
std::array<Reader,4> readers{{
    {PRISM_BACKEND_SAPI,"SAPI",97,true,false},
    {123,"Other reader",104,false,true},
    {PRISM_BACKEND_JAWS,"JAWS",100,true,true},
    {PRISM_BACKEND_NVDA,"NVDA",103,true,true}
}};
auto& sapi=readers[0];auto& other=readers[1];auto& jaws=readers[2];auto& nvda=readers[3];
int allocated=0,freed=0,contexts=0;
bool initFails=false;
std::vector<std::string> logs;
void Check(bool value,const char* description) {
    if(!value){std::cerr<<"FAIL: "<<description<<'\n';std::exit(1);}
    std::cout<<"PASS: "<<description<<'\n';
}
}
namespace lis {void Log(const std::string& text){logs.push_back(text);}}
extern "C" {
PrismConfig PRISM_CALL prism_config_init(){return {PRISM_CONFIG_VERSION};}
PRISM_MALLOC PrismContext* PRISM_CALL prism_init(PrismConfig*){if(initFails)return nullptr;++contexts;return new PrismContext;}
void PRISM_CALL prism_shutdown(PrismContext* p){--contexts;delete p;}
size_t PRISM_CALL prism_registry_count(PrismContext*){return readers.size();}
PrismBackendId PRISM_CALL prism_registry_id_at(PrismContext*,size_t i){return readers[i].id;}
int PRISM_CALL prism_registry_priority(PrismContext*,PrismBackendId id){for(const auto& r:readers)if(r.id==id)return r.priority;return 0;}
PrismBackend* PRISM_CALL prism_registry_acquire(PrismContext*,PrismBackendId id){
    for(size_t i=0;i<readers.size();++i)if(readers[i].id==id){++allocated;return new PrismBackend{i};}return nullptr;
}
void PRISM_CALL prism_backend_free(PrismBackend* p){++freed;delete p;}
const char* PRISM_CALL prism_backend_name(PrismBackend* p){return readers[p->index].name;}
uint64_t PRISM_CALL prism_backend_get_features(PrismBackend* p){
    const auto& r=readers[p->index];
    return PRISM_BACKEND_SUPPORTS_SPEAK|PRISM_BACKEND_SUPPORTS_STOP|
        (r.available?PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME:0)|(r.braille?PRISM_BACKEND_SUPPORTS_BRAILLE:0);
}
PrismError PRISM_CALL prism_backend_initialize(PrismBackend* p){++readers[p->index].inits;return PRISM_OK;}
PrismError PRISM_CALL prism_backend_speak(PrismBackend* p,const char* text,bool interrupt){
    auto& r=readers[p->index];++r.speaks;r.text=text;r.interrupt=interrupt;
    if(r.fault)RaiseException(RPC_S_SERVER_UNAVAILABLE,0,0,nullptr);
    return r.speakResult;
}
PrismError PRISM_CALL prism_backend_braille(PrismBackend* p,const char* text){auto& r=readers[p->index];++r.brailles;r.brailleText=text;return r.brailleResult;}
PrismError PRISM_CALL prism_backend_stop(PrismBackend* p){++readers[p->index].stops;return PRISM_OK;}
}
int main() {
    lis::Speak(L"Before initialization");Check(allocated==0,"inactive output is ignored");
    lis::InitializeSpeech(nullptr);lis::InitializeSpeech(nullptr);
    Check(std::string(lis::SpeechBackendName())=="NVDA"&&nvda.inits==1&&contexts==1,"prefer active NVDA and initialize once");
    lis::Speak(L"Photo \u2014 caf\u00e9",true);
    Check(nvda.text==u8"Photo \u2014 caf\u00e9"&&nvda.brailleText==nvda.text&&nvda.interrupt,"same UTF-8 text reaches speech and braille");
    lis::Speak(L"Queued",false);Check(!nvda.interrupt&&nvda.inits==1,"noninterrupting output preserves queue flag and backend lifetime");
    const auto obsolete=lis::SpeechSerial();const int stops=nvda.stops;
    lis::Speak(L"Newer menu readout");lis::StopSpeechIfCurrent(obsolete);
    Check(nvda.stops==stops,"obsolete dialogue cannot cancel newer menu speech");
    const auto current=lis::SpeechSerial();lis::StopSpeechIfCurrent(current);lis::StopSpeechIfCurrent(current);
    Check(nvda.stops==stops+1,"current dialogue cancellation stops once and invalidates its token");
    nvda.brailleResult=PRISM_ERROR_INTERNAL;
    const int spoken=nvda.speaks;
    lis::Speak(L"Braille failure");
    Check(nvda.speaks==spoken+1&&sapi.speaks==0&&jaws.speaks==0,"braille failure never replays successful speech");
    const auto logged=logs.size();lis::Speak(L"Same braille failure");
    Check(logs.size()==logged,"repeated braille failure is not logged per announcement");
    nvda.brailleResult=PRISM_OK;nvda.speakResult=PRISM_ERROR_BACKEND_NOT_AVAILABLE;
    lis::Speak(L"Reader vanished");Check(jaws.speaks==1&&jaws.brailles==1,"speech failure falls back to another available reader");
    nvda.speakResult=PRISM_OK;lis::Speak(L"Reader returned");
    Check(std::string(lis::SpeechBackendName())=="NVDA"&&jaws.stops==1,"recovered preferred reader stops old speech on interrupting switch");
    nvda.available=jaws.available=false;lis::Speak(L"System fallback");
    Check(sapi.speaks==1&&sapi.brailles==0&&std::string(lis::SpeechBackendName())=="SAPI","speech-only SAPI fallback needs no braille support");
    nvda.available=true;lis::Speak(L"NVDA restarted");
    Check(std::string(lis::SpeechBackendName())=="NVDA"&&sapi.stops==1,"reader restart leaves fallback and cancels its speech");
    const int count=nvda.speaks;
    std::thread wrong([]{lis::Speak(L"Wrong thread");lis::ShutdownSpeech();});wrong.join();lis::Speak(L"");
    Check(nvda.speaks==count&&contexts==1,"wrong-thread calls and empty text are ignored");
    nvda.fault=true;lis::Speak(L"Native RPC failure");
    Check(sapi.speaks==2,"native reader exception cannot escape and falls back");
    nvda.fault=false;other.available=true;lis::Speak(L"Another Prism reader");
    Check(other.speaks==1&&other.brailles==1,"other runtime-supported Prism providers are not hardcoded out");
    lis::ShutdownSpeech();lis::ShutdownSpeech();
    Check(contexts==0&&allocated==freed,"every acquired backend is freed before context shutdown");
    initFails=true;lis::InitializeSpeech(nullptr);lis::Speak(L"Failed initialization");lis::ShutdownSpeech();
    Check(contexts==0&&allocated==freed,"failed context initialization remains safe");
}
