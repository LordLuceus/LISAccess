#include "bootstrap/runtime.h"
#include "bootstrap/platform_lifecycle.h"
#include "engine/build_verification.h"
#include "engine/gfx.h"
#include "features/menus/menus.h"
#include "features/menus/diagnostics.h"
#include "features/navigation/navigation.h"
#include "features/prompts/prompts.h"
#include "features/dialogue/dialogue.h"
#include "platform/foreground.h"
#include "platform/log.h"
#include "output/speech.h"
#include <shlobj.h>
#include <atomic>

namespace lis {
namespace {
HWND window = nullptr;
WNDPROC previousProc = nullptr;
UINT tickMessage = 0;
UINT_PTR timer = 0;
std::atomic<bool> failed{false};
std::wstring localDirectory;

void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD) {
    prompts::Tick();
    navigation::Tick();
    dialogue::Tick();
    if (!failed && !menus::SafeSnapshot()) { failed = true; Log("event=bridge_fault status=disabled"); }
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == tickMessage) {
        if (!failed && !menus::SafeSnapshot()) { failed = true; Log("event=bridge_fault status=disabled"); }
        return 0;
    }
    if (message == WM_KEYDOWN && !(lp & (1LL<<30))) {
        if (menus::HandleKey(wp)) return 0;
    }
    return CallWindowProcW(previousProc, hwnd, message, wp, lp);
}
BOOL CALLBACK FindWindow(HWND hwnd, LPARAM) {
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
        wchar_t title[128]{}; GetWindowTextW(hwnd, title, ARRAYSIZE(title));
        if (std::wstring(title).find(L"Life Is Strange") != std::wstring::npos) { window = hwnd; return FALSE; }
    }
    return TRUE;
}
}

void RunRuntime(HMODULE module) {
    InitializePlatform(module);
    if (!VerifyBuild()) { Log("event=unsupported_build"); Speak(L"Life Is Strange accessibility: unsupported game build. Menu inspection disabled."); return; }
    engine::gfx::Initialize(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
    PWSTR directory = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &directory))) {
        localDirectory = std::wstring(directory) + L"\\LISAccess"; CoTaskMemFree(directory);
    }
    Log("event=prototype_start pid=" + std::to_string(GetCurrentProcessId()) + " build=2304417 compiled=\"" __DATE__ " " __TIME__ "\"");
    Speak(L"Life Is Strange accessibility mod loaded.");
    tickMessage = RegisterWindowMessageW(L"LISAccess.ReadOnlyMenuProbe.v1");
    for (int n=0; n<300 && !window; ++n) { EnumWindows(FindWindow, 0); if (!window) Sleep(100); }
    if (!window) { Log("event=no_game_window"); Speak(L"Could not attach menu inspection."); return; }
    // The engine replaces its bootstrap WndProc after window creation. Wait for
    // the real menu objects, and use a window timer for game-thread reads so a
    // later procedure replacement cannot silently stop state observation.
    for (int n=0; n<120 && !engine::gfx::Bound(); ++n) {
        engine::gfx::Discover(); if (!engine::gfx::Bound()) Sleep(500);
    }
    SetLastError(0);
    previousProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc)));
    if (!previousProc) { Log("event=subclass_failed"); Speak(L"Could not attach menu inspection."); return; }
    navigation::Initialize(module);
    prompts::Initialize(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
    timer = SetTimer(window, 0x4c4953, 200, TimerProc);
    if (!timer) { Log("event=timer_failed"); Speak(L"Could not start menu inspection."); return; }
    Log("event=bridge_ready window_thread=" + std::to_string(GetWindowThreadProcessId(window, nullptr)));
    bool faultSpoken = false;
    bool wasForeground = false;
    while (IsWindow(window)) {
        menus::Files(localDirectory);
        if (failed && !faultSpoken) { Speak(L"Menu inspection failed and has been disabled. See the mod log."); faultSpoken = true; }
        bool readableMenu = false;
        const bool foreground = IsGameForeground();
        bool interrupt=true;
        std::wstring speech = menus::ConsumeSpeech(foreground, wasForeground, readableMenu,interrupt);
        if (!speech.empty() && foreground) Speak(speech,interrupt);
        navigation::Pump(foreground, readableMenu);
        dialogue::Pump(foreground, readableMenu);
        prompts::Pump(foreground,readableMenu);
        wasForeground = foreground;
        Sleep(25);
    }
    KillTimer(window, timer);
    navigation::Shutdown();
    ShutdownPlatform();
}
}
