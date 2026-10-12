#include "platform.h"
#include <sapi.h>
#include <shlobj.h>

namespace lis {
namespace {
SRWLOCK logLock = SRWLOCK_INIT;
HANDLE logFile = INVALID_HANDLE_VALUE;
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

void OpenLog() {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) return;
    const std::wstring directory = std::wstring(local) + L"\\LISAccess";
    CoTaskMemFree(local);
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return;
    AcquireSRWLockExclusive(&logLock);
    logFile = CreateFileW((directory + L"\\prototype.log").c_str(), FILE_APPEND_DATA,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    ReleaseSRWLockExclusive(&logLock);
}

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

void Log(const std::string& fields) {
    // One stable line per event; speech content is never automatically logged.
    std::string line = "[a11y] " + fields;
    for (char& c : line) if (c == '\r' || c == '\n' || c == '\0') c = ' ';
    line += "\r\n";
    AcquireSRWLockExclusive(&logLock);
    if (logFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(logFile, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    }
    ReleaseSRWLockExclusive(&logLock);
}

void InitializePlatform(HMODULE module) {
    if (initialized) return;
    initialized = true;
    ownerThread = GetCurrentThreadId();
    OpenLog();
    Log("event=platform_initialize arch=x86");
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

void ShutdownPlatform() {
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
    Log("event=platform_shutdown");
    AcquireSRWLockExclusive(&logLock);
    if (logFile != INVALID_HANDLE_VALUE) CloseHandle(logFile);
    logFile = INVALID_HANDLE_VALUE;
    ReleaseSRWLockExclusive(&logLock);
    initialized = false;
    ownerThread = 0;
}

bool IsGameForeground() {
    const HWND foreground = GetForegroundWindow();
    DWORD process = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &process);
    return process != 0 && process == GetCurrentProcessId();
}
}
