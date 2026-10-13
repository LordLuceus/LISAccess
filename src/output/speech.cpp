#include "output/speech.h"
#include "platform/log.h"
#include <sapi.h>

namespace lis {
namespace {
HMODULE nvda = nullptr;
using NvdaTest = int(WINAPI*)();
using NvdaSpeak = int(WINAPI*)(const wchar_t*);
NvdaTest testNvda = nullptr;
NvdaTest cancelNvda = nullptr;
NvdaSpeak speakNvda = nullptr;
ISpVoice* voice = nullptr;
bool ownsCom = false;
bool initialized = false;
DWORD ownerThread = 0;

void EnsureSapi() {
    if (voice) return;
    if (!ownsCom) {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        // S_OK and S_FALSE both acquire an apartment reference. A mismatched
        // pre-existing apartment belongs to somebody else; do not uninitialize it.
        if (SUCCEEDED(hr)) ownsCom = true;
        else if (hr != RPC_E_CHANGED_MODE) return;
    }
    const HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_ISpVoice, reinterpret_cast<void**>(&voice));
    Log(SUCCEEDED(hr) ? "event=speech_backend backend=sapi status=ready"
                      : "event=speech_backend backend=sapi status=unavailable");
}
}
void InitializeSpeech(HMODULE module) {
    if (initialized) return;
    initialized = true;
    ownerThread = GetCurrentThreadId();
    wchar_t path[32768]{};
    const DWORD count = GetModuleFileNameW(module, path, ARRAYSIZE(path));
    if (count && count < ARRAYSIZE(path)) {
        const std::wstring full(path, count);
        const auto separator = full.find_last_of(L"\\/");
        if (separator != std::wstring::npos) {
            const auto client = full.substr(0, separator + 1) + L"nvdaControllerClient32.dll";
            nvda = LoadLibraryExW(client.c_str(), nullptr,
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        }
    }
    if (nvda) {
        testNvda = reinterpret_cast<NvdaTest>(GetProcAddress(nvda, "nvdaController_testIfRunning"));
        speakNvda = reinterpret_cast<NvdaSpeak>(GetProcAddress(nvda, "nvdaController_speakText"));
        cancelNvda = reinterpret_cast<NvdaTest>(GetProcAddress(nvda, "nvdaController_cancelSpeech"));
        if (!testNvda || !speakNvda || !cancelNvda) {
            FreeLibrary(nvda);
            nvda = nullptr;
            testNvda = nullptr;
            speakNvda = nullptr;
            cancelNvda = nullptr;
        }
    }
    if (testNvda && testNvda() == 0) Log("event=speech_backend backend=nvda status=ready");
    else EnsureSapi();
}
void Speak(const std::wstring& text, bool interrupt) {
    if (!initialized || ownerThread != GetCurrentThreadId() || text.empty()) return;
    if (testNvda && testNvda() == 0) {
        if (interrupt) {
            if (voice) voice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
            cancelNvda();
        }
        if (speakNvda(text.c_str()) == 0) return;
    }
    EnsureSapi();
    if (voice) {
        const DWORD flags = SPF_ASYNC | SPF_IS_NOT_XML | (interrupt ? SPF_PURGEBEFORESPEAK : 0);
        if (FAILED(voice->Speak(text.c_str(), flags, nullptr))) Log("event=speech_failed backend=sapi");
    }
}
void ShutdownSpeech() {
    if (!initialized || ownerThread != GetCurrentThreadId()) return;
    if (voice) {
        voice->Speak(nullptr, SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
        voice->Release();
        voice = nullptr;
    }
    if (nvda) FreeLibrary(nvda);
    nvda = nullptr;
    testNvda = nullptr;
    cancelNvda = nullptr;
    speakNvda = nullptr;
    if (ownsCom) CoUninitialize();
    ownsCom = false;
    initialized = false;
    ownerThread = 0;
}
}
