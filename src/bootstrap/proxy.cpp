#include "bootstrap/runtime.h"
#include "bootstrap/platform_lifecycle.h"
#include "platform/log.h"
#include <objbase.h>

namespace {
INIT_ONCE realOnce = INIT_ONCE_STATIC_INIT;
HMODULE realModule = nullptr;
using CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using GetClassFn = HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*);
using SimpleFn = HRESULT(WINAPI*)();

BOOL CALLBACK LoadReal(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t directory[MAX_PATH + 1]{};
    const UINT count = GetSystemDirectoryW(directory, MAX_PATH);
    if (!count || count >= MAX_PATH) return TRUE;
    const std::wstring path = std::wstring(directory, count) + L"\\dinput8.dll";
    realModule = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return TRUE; // A missing system library is a permanent failure, never search elsewhere.
}
FARPROC Resolve(const char* name) {
    InitOnceExecuteOnce(&realOnce, LoadReal, nullptr, nullptr);
    return realModule ? GetProcAddress(realModule, name) : nullptr;
}
DWORD WINAPI Worker(void* context) {
    // Thread entry is serialized behind DLL_PROCESS_ATTACH by the OS. Pin here,
    // outside the loader lock: the prototype is process-lifetime, not hot-unloadable.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&Worker), &pinned)) return 0;
    try {
        lis::RunRuntime(static_cast<HMODULE>(context));
    } catch (...) {
        lis::Log("event=runtime_exception");
        lis::ShutdownPlatform();
    }
    return 0;
}
}
extern "C" HRESULT WINAPI ProxyDirectInput8Create(HINSTANCE instance, DWORD version,
                                                  REFIID iid, LPVOID* output, LPUNKNOWN outer) {
    const auto fn = reinterpret_cast<CreateFn>(Resolve("DirectInput8Create"));
    if (!fn) { if (output) *output = nullptr; return E_FAIL; }
    return fn(instance, version, iid, output, outer);
}
extern "C" HRESULT WINAPI ProxyDllCanUnloadNow() {
    const auto fn = reinterpret_cast<SimpleFn>(Resolve("DllCanUnloadNow"));
    return fn ? fn() : S_FALSE;
}
extern "C" HRESULT WINAPI ProxyDllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID* output) {
    const auto fn = reinterpret_cast<GetClassFn>(Resolve("DllGetClassObject"));
    if (!fn) { if (output) *output = nullptr; return CLASS_E_CLASSNOTAVAILABLE; }
    return fn(clsid, iid, output);
}
extern "C" HRESULT WINAPI ProxyDllRegisterServer() {
    const auto fn = reinterpret_cast<SimpleFn>(Resolve("DllRegisterServer"));
    return fn ? fn() : E_FAIL;
}
extern "C" HRESULT WINAPI ProxyDllUnregisterServer() {
    const auto fn = reinterpret_cast<SimpleFn>(Resolve("DllUnregisterServer"));
    return fn ? fn() : E_FAIL;
}
extern "C" const void* WINAPI ProxyGetdfDIJoystick() {
    const auto fn = reinterpret_cast<const void*(WINAPI*)()>(Resolve("GetdfDIJoystick"));
    return fn ? fn() : nullptr;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Do not disable thread callbacks with the static CRT. No COM, speech,
        // logging, library loads or waits occur here. Windows defers Worker entry.
        const HANDLE thread = CreateThread(nullptr, 0, Worker, module, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    // Detach deliberately performs no cleanup; the runtime owns its resources.
    return TRUE;
}
