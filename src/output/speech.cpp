#include "output/speech.h"
#include "output/prism_bridge.h"
#include "platform/log.h"
#include "platform/text.h"
#include <algorithm>
#include <vector>

namespace lis {
namespace {
namespace bridge=output::bridge;
struct Backend {
    PrismBackendId id=PRISM_BACKEND_INVALID;
    int priority=0;
    PrismBackend* handle=nullptr;
    uint64_t features=0;
    bool ready=false;
    PrismError speechError=PRISM_OK,brailleError=PRISM_OK;
};
PrismContext* context=nullptr;
std::vector<Backend> backends;
PrismBackendId active=PRISM_BACKEND_INVALID;
std::string activeName;
bool initialized = false;
DWORD ownerThread = 0;
uint64_t serial=0;

void Release(Backend& backend) {
    if(backend.handle)bridge::Free(backend.handle);
    backend.handle=nullptr;backend.ready=false;backend.features=0;
}
bool Ready(Backend& backend) {
    if(!backend.handle)backend.handle=bridge::Acquire(context,backend.id);
    if(!backend.handle)return false;
    backend.features=bridge::Features(backend.handle);
    constexpr uint64_t required=PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME|PRISM_BACKEND_SUPPORTS_SPEAK;
    if((backend.features&required)!=required) {
        // Discard old RPC bindings on reader exit, so a restart gets a fresh one.
        Release(backend);return false;
    }
    if(!backend.ready) {
        const auto result=bridge::Initialize(backend.handle);
        if(result!=PRISM_OK && result!=PRISM_ERROR_ALREADY_INITIALIZED){Release(backend);return false;}
        backend.ready=true;
    }
    return true;
}
void Selected(Backend& backend) {
    if(active==backend.id)return;
    active=backend.id;
    const char* name=bridge::Name(backend.handle);
    activeName=name?name:"unknown";
    Log("event=speech_backend provider=prism backend="+activeName+" status=ready braille="+
        std::to_string((backend.features&PRISM_BACKEND_SUPPORTS_BRAILLE)!=0));
}
void Report(Backend& backend,const char* channel,PrismError result,PrismError& last) {
    if(result!=PRISM_OK && result!=last) {
        const char* name=bridge::Name(backend.handle);
        Log(std::string("event=")+channel+"_failed provider=prism backend="+(name?name:"unknown")+
            " code="+std::to_string(static_cast<int>(result)));
    }
    last=result;
}
}
void InitializeSpeech(HMODULE) {
    if (initialized) return;
    initialized = true;
    ownerThread = GetCurrentThreadId();
    context=bridge::Init();
    if(!context){Log("event=speech_failed provider=prism reason=initialization");return;}
    const size_t count=bridge::Count(context);
    for(size_t i=0;i<count && i<64;++i) {
        const auto id=bridge::Id(context,i);
        if(id!=PRISM_BACKEND_INVALID)backends.push_back({id,bridge::Priority(context,id)});
    }
    std::stable_sort(backends.begin(),backends.end(),[](const auto& a,const auto& b) {
        if(a.priority!=b.priority)return a.priority>b.priority;
        // Preserve SAPI as the preferred system-TTS fallback in a priority tie.
        return a.id==PRISM_BACKEND_SAPI && b.id!=PRISM_BACKEND_SAPI;
    });
    for(auto& backend:backends)if(Ready(backend)){Selected(backend);return;}
    Log("event=speech_failed provider=prism reason=no_available_backend");
}
void Speak(const std::wstring& text, bool interrupt) {
    if (!initialized || ownerThread != GetCurrentThreadId() || text.empty()) return;
    ++serial;
    if(!context)return;
    const std::string utf8=Utf8(text);
    if(utf8.empty())return;
    for(auto& backend:backends) {
        if(!Ready(backend))continue;
        if(interrupt && active!=backend.id) {
            for(auto& previous:backends)
                if(previous.id==active && previous.ready && previous.handle)bridge::Stop(previous.handle);
        }
        // Separate results are essential: output() can report a braille failure
        // AFTER successfully speaking. Retrying that result would speak twice.
        const auto speech=bridge::Speak(backend.handle,utf8.c_str(),interrupt);
        Report(backend,"speech",speech,backend.speechError);
        if(backend.features&PRISM_BACKEND_SUPPORTS_BRAILLE) {
            const auto braille=bridge::Braille(backend.handle,utf8.c_str());
            Report(backend,"braille",braille,backend.brailleError);
        }
        if(speech==PRISM_OK){Selected(backend);return;}
        Release(backend);
    }
    active=PRISM_BACKEND_INVALID;activeName.clear();
    Log("event=speech_failed provider=prism reason=all_backends_failed");
}
const char* SpeechBackendName() {return ownerThread==GetCurrentThreadId()?activeName.c_str():"";}
uint64_t SpeechSerial() {return ownerThread==GetCurrentThreadId()?serial:0;}
void StopSpeechIfCurrent(uint64_t expected) {
    if(!initialized||ownerThread!=GetCurrentThreadId()||!expected||expected!=serial)return;
    for(auto& backend:backends)if(backend.id==active&&backend.ready&&backend.handle)bridge::Stop(backend.handle);
    ++serial; // an old token cannot cancel a later output lifetime
}
void ShutdownSpeech() {
    if (!initialized || ownerThread != GetCurrentThreadId()) return;
    for(auto& backend:backends){if(backend.ready && backend.handle)bridge::Stop(backend.handle);Release(backend);}
    backends.clear();active=PRISM_BACKEND_INVALID;activeName.clear();
    if(context)bridge::Shutdown(context);
    context=nullptr;
    initialized = false;
    ownerThread = 0;
}
}
