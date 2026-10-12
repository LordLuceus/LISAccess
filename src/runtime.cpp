#include "platform.h"
#include "narration.h"
#include "engine_abi.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <vector>

namespace lis {
namespace {
// Only the SHA-256-verified Steam build 2304417. These are RVAs, not loaded VAs.
constexpr uintptr_t NamesRva = 0x11e43b4, ObjectsRva = 0x11d0598;
constexpr uintptr_t ProcessEventRva = 0x7b4b0, FreeRva = 0x259e0;
uintptr_t base = 0, menu = 0, stringFn = 0, numberFn = 0, invokeFn = 0;
HWND window = nullptr;
WNDPROC previousProc = nullptr;
UINT tickMessage = 0;
UINT_PTR timer = 0;
std::atomic<bool> failed{false}, repeat{false}, help{false};
std::mutex stateMutex;
std::wstring pendingSpeech, lastSpeech;
std::string lastIdentity, pendingRequest, completedResponse;
bool hasMenu = false;
unsigned ticks = 0;
std::wstring localDirectory;

bool Copy(void* target, uintptr_t source, size_t bytes) {
    if (source < 0x10000 || bytes > 1024 * 1024) return false;
    __try { std::memcpy(target, reinterpret_cast<void*>(source), bytes); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> T Read(uintptr_t address) { T value{}; Copy(&value, address, sizeof(value)); return value; }
struct Array { uintptr_t data; int count; int capacity; };
struct FString { wchar_t* data; int count; int capacity; };
static_assert(sizeof(FString) == 12);

std::string Utf8(const std::wstring& value) {
    if (value.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), n, nullptr, nullptr);
    return result;
}
std::wstring Wide(const std::string& value) {
    if (value.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), n);
    return result;
}
bool VerifyBuild() {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, ARRAYSIZE(path))) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<unsigned char, 32> digest{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0;
    if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0;
    std::array<unsigned char, 65536> buffer{}; DWORD count = 0;
    while (ok) {
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        ok = BCryptHashData(hash, buffer.data(), count, 0) == 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    const unsigned char expected[32] = {0x09,0x73,0xa7,0xe1,0x93,0x72,0x80,0x88,0xed,0x43,0x8c,0x02,0xdd,0x1f,0x99,0xb4,0x90,0xeb,0x35,0x35,0x5a,0xd8,0xbe,0x0f,0x22,0x40,0xa3,0xd2,0x69,0x0f,0xa2,0x33};
    return ok && std::memcmp(digest.data(), expected, 32) == 0;
}
std::string Name(int index) {
    Array a = Read<Array>(base + NamesRva);
    if (index < 0 || index >= a.count || a.count > 1000000) return {};
    uintptr_t entry = Read<uintptr_t>(a.data + index * 4);
    char text[192]{};
    if (!Copy(text, entry + 8, sizeof(text)-1)) return {};
    return text;
}
std::string ObjectName(uintptr_t object) { return Name(Read<int>(object + 0x2c)); }
std::string ClassName(uintptr_t object) { return ObjectName(Read<uintptr_t>(object + 0x34)); }
bool Registered(uintptr_t object) {
    Array a = Read<Array>(base + ObjectsRva);
    int index = Read<int>(object + 0x20);
    return object && index >= 0 && index < a.count && a.count < 1000000 && Read<uintptr_t>(a.data + index * 4) == object;
}
bool Function(uintptr_t fn, const char* name, int size) {
    return Registered(fn) && ClassName(fn) == "Function" && ObjectName(fn) == name &&
        ObjectName(Read<uintptr_t>(fn + 0x28)) == "GFxMoviePlayer" &&
        Read<unsigned short>(fn + 0xa2) == size && (Read<unsigned>(fn + 0x90) & 0x400);
}
void Discover() {
    Array a = Read<Array>(base + ObjectsRva);
    if (!a.data || a.count < 100 || a.count > 1000000 || a.capacity < a.count || Name(0) != "None" || Name(2) != "IntProperty") return;
    std::vector<uintptr_t> objects(a.count);
    if (!Copy(objects.data(), a.data, objects.size() * 4)) return;
    for (uintptr_t obj : objects) {
        if (!obj) continue;
        const std::string cls = ClassName(obj);
        if (cls == "Function" && ObjectName(Read<uintptr_t>(obj + 0x28)) == "GFxMoviePlayer") {
            std::string name = ObjectName(obj);
            if (name == "GetVariableString" && Function(obj, name.c_str(), 24)) stringFn = obj;
            if (name == "GetVariableNumber" && Function(obj, name.c_str(), 16)) numberFn = obj;
            if (name == "Invoke" && Function(obj, name.c_str(), 48)) invokeFn = obj;
        }
        if (cls == "OutGameSensenMenu" && ObjectName(obj).find("Default__") != 0 &&
            Read<uintptr_t>(Read<uintptr_t>(obj) + 72 * 4) == base + ProcessEventRva) menu = obj;
    }
}
// Both UObject and AActor dispatchers end in ret 0x0c: THREE stack arguments.
// The legacy result pointer is unused on this path but must still be supplied.
void Event(uintptr_t fn, void* args) {
    reinterpret_cast<engine::ProcessEvent>(base + ProcessEventRva)(reinterpret_cast<void*>(menu), reinterpret_cast<void*>(fn), args, nullptr);
}
std::wstring TakeString(FString& value) {
    std::wstring text;
    if (value.data && value.count > 0 && value.count <= 8192 && value.capacity >= value.count) {
        text.resize(value.count - 1);
        if (!Copy(text.data(), reinterpret_cast<uintptr_t>(value.data), text.size()*2)) text.clear();
    }
    // This is the game's allocator wrapper, verified from FString destruction.
    if (value.data) reinterpret_cast<void(__cdecl*)(void*)>(base + FreeRva)(value.data);
    value = {};
    return text;
}
FString Borrow(std::wstring& s) { return {s.data(), static_cast<int>(s.size()+1), static_cast<int>(s.size()+1)}; }
std::wstring String(const std::string& path) {
    std::wstring input = Wide(path);
    struct Params { FString path; FString result; } p{Borrow(input), {}};
    Event(stringFn, &p);
    return TakeString(p.result);
}
float Number(const std::string& path) {
    std::wstring input = Wide(path);
    struct Params { FString path; float result; } p{Borrow(input), 0};
    Event(numberFn, &p); return p.result;
}
std::wstring Text(const std::string& objectPath) {
    std::wstring method = Wide(objectPath + ".GetText");
    struct Value { unsigned type; unsigned boolean; float number; FString text; };
    struct Params { FString method; Array args; Value result; } p{Borrow(method), {}, {}};
    static_assert(sizeof(Params) == 48);
    Event(invokeFn, &p);
    return TakeString(p.result.text);
}
bool ValidPath(const std::string& path) {
    return path.size() < 256 && path.rfind("_root.", 0) == 0 &&
        path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.$") == std::string::npos;
}
void InspectRequest() {
    std::string command;
    { std::lock_guard<std::mutex> lock(stateMutex); command.swap(pendingRequest); }
    if (command.empty()) return;
    std::istringstream in(command); std::string kind, path; in >> kind >> path;
    std::string result = "error invalid read-only request";
    if (ValidPath(path)) {
        if (kind == "string") result = "ok " + Utf8(String(path));
        else if (kind == "number") result = "ok " + std::to_string(Number(path));
        else if (kind == "text") result = "ok " + Utf8(Text(path));
    }
    { std::lock_guard<std::mutex> lock(stateMutex); completedResponse = result; }
}
void Snapshot() {
    ++ticks;
    if (ticks == 1) Log("event=first_game_thread_tick");
    if ((!menu || !stringFn || !numberFn || !invokeFn || !Registered(menu)) && ticks % 5 == 1) Discover();
    if (ticks == 1) Log("event=discovery_state menu=" + std::to_string(menu != 0) +
        " string=" + std::to_string(stringFn != 0) + " number=" + std::to_string(numberFn != 0) +
        " invoke=" + std::to_string(invokeFn != 0) + " opening=" + std::to_string(Read<unsigned char>(menu + 0x1f4)));
    if (!menu || !stringFn || !numberFn || !invokeFn || !Registered(menu) || Read<unsigned char>(menu + 0x1f4) != 2) {
        std::lock_guard<std::mutex> lock(stateMutex); hasMenu = false; lastIdentity.clear(); return;
    }
    InspectRequest();
    float countValue = Number("_root.focus_arrays.length");
    if (!std::isfinite(countValue) || countValue < 1 || countValue > 16) {
        std::lock_guard<std::mutex> lock(stateMutex); hasMenu = false; lastIdentity.clear(); return;
    }
    int count = static_cast<int>(countValue);
    const std::string focus = "_root.focus_arrays." + std::to_string(count-1);
    std::wstring page = String(focus + ".PageID");
    float selectionValue = Number(focus + ".current_selection");
    if (!std::isfinite(selectionValue) || selectionValue < 0 || selectionValue > 100) return;
    int selection = static_cast<int>(selectionValue);
    const std::string selected = focus + ".selection_clips." + std::to_string(selection);
    std::vector<std::wstring> parts;
    int modalType = -1;
    if (page == L"CMD_OUTGAMESLOT") {
        for (const char* field : {"title_mc", "episode_mc", "sub_episode_mc", "time_value_mc"})
            parts.push_back(Text(selected + "." + field));
    } else if (!page.empty()) parts.push_back(Text(selected));
    else {
        parts.push_back(Text(focus + ".TEXT"));
        float value = Number(focus + ".message_type");
        if (std::isfinite(value) && value >= 0 && value <= 4) modalType = static_cast<int>(value);
    }
    std::wstring text = DescribeMenu(parts, modalType);
    std::string identity = Utf8(page) + ":" + std::to_string(count) + ":" + std::to_string(selection) + ":" + Utf8(text);
    if (identity != lastIdentity) {
        lastIdentity = identity;
        Log("event=menu_focus page=" + Utf8(page) + " depth=" + std::to_string(count) + " selection=" + std::to_string(selection) + " text=" + Utf8(text));
        std::lock_guard<std::mutex> lock(stateMutex);
        hasMenu = !text.empty();
        if (hasMenu) pendingSpeech = text;
    }
}
bool SafeSnapshot() {
    __try { Snapshot(); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD) {
    if (!failed && !SafeSnapshot()) { failed = true; Log("event=bridge_fault status=disabled"); }
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == tickMessage) {
        if (!failed && !SafeSnapshot()) { failed = true; Log("event=bridge_fault status=disabled"); }
        return 0;
    }
    if (message == WM_KEYDOWN && !(lp & (1LL<<30))) {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (hasMenu && wp == 'R') { repeat = true; return 0; }
        if (hasMenu && wp == VK_F1) { help = true; return 0; }
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
void Files() {
    const auto requestPath = localDirectory + L"\\request.txt";
    std::ifstream input(requestPath, std::ios::binary);
    if (input) {
        char bytes[513]{}; input.read(bytes, 512); const auto n = input.gcount(); input.close();
        DeleteFileW(requestPath.c_str());
        std::lock_guard<std::mutex> lock(stateMutex); pendingRequest.assign(bytes, static_cast<size_t>(n));
    }
    std::string response;
    { std::lock_guard<std::mutex> lock(stateMutex); response.swap(completedResponse); }
    if (!response.empty()) {
        const auto temp = localDirectory + L"\\response.tmp";
        std::ofstream output(temp, std::ios::binary); output << response; output.close();
        MoveFileExW(temp.c_str(), (localDirectory + L"\\response.txt").c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}
}

void RunRuntime(HMODULE module) {
    InitializePlatform(module);
    if (!VerifyBuild()) { Log("event=unsupported_build"); Speak(L"Life Is Strange accessibility: unsupported game build. Menu inspection disabled."); return; }
    base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    PWSTR directory = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &directory))) {
        localDirectory = std::wstring(directory) + L"\\LISAccess"; CoTaskMemFree(directory);
    }
    Log("event=prototype_start pid=" + std::to_string(GetCurrentProcessId()) + " build=2304417 compiled=\"" __DATE__ " " __TIME__ "\"");
    Speak(L"Life Is Strange accessibility prototype loaded.");
    tickMessage = RegisterWindowMessageW(L"LISAccess.ReadOnlyMenuProbe.v1");
    for (int n=0; n<300 && !window; ++n) { EnumWindows(FindWindow, 0); if (!window) Sleep(100); }
    if (!window) { Log("event=no_game_window"); Speak(L"Could not attach menu inspection."); return; }
    // The engine replaces its bootstrap WndProc after window creation. Wait for
    // the real menu objects, and use a window timer for game-thread reads so a
    // later procedure replacement cannot silently stop state observation.
    for (int n=0; n<120 && (!menu || !stringFn || !numberFn || !invokeFn); ++n) {
        Discover(); if (!menu || !stringFn || !numberFn || !invokeFn) Sleep(500);
    }
    SetLastError(0);
    previousProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc)));
    if (!previousProc) { Log("event=subclass_failed"); Speak(L"Could not attach menu inspection."); return; }
    timer = SetTimer(window, 0x4c4953, 200, TimerProc);
    if (!timer) { Log("event=timer_failed"); Speak(L"Could not start menu inspection."); return; }
    Log("event=bridge_ready window_thread=" + std::to_string(GetWindowThreadProcessId(window, nullptr)));
    bool faultSpoken = false;
    bool wasForeground = false;
    while (IsWindow(window)) {
        Files();
        if (failed && !faultSpoken) { Speak(L"Menu inspection failed and has been disabled. See the prototype log."); faultSpoken = true; }
        std::wstring speech;
        const bool foreground = IsGameForeground();
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            speech.swap(pendingSpeech);
            if (!speech.empty()) lastSpeech = speech;
            if (foreground && !wasForeground && speech.empty() && hasMenu) speech = lastSpeech;
            if (repeat.exchange(false)) speech = lastSpeech;
            if (help.exchange(false)) speech = L"Menu prototype. Use the game's arrow keys, Enter to select and Escape to go back. R repeats. F1 reads this help. Opening navigation is not implemented.";
        }
        if (!speech.empty() && foreground) Speak(speech);
        wasForeground = foreground;
        Sleep(200);
    }
    KillTimer(window, timer);
    ShutdownPlatform();
}
}
