#include "features/menus/menus.h"
#include "features/menus/state.h"
#include "features/menus/diagnostics.h"
#include "features/menus/narration.h"
#include "engine/gfx.h"
#include "platform/log.h"
#include "platform/text.h"
#include <atomic>
#include <cmath>
#include <vector>

namespace lis::menus {
std::mutex stateMutex;
namespace {
using namespace engine::gfx;
std::atomic<bool> repeat{false}, help{false};
std::wstring pendingSpeech, lastSpeech;
std::string lastIdentity;
bool hasMenu = false;
unsigned ticks = 0;

void Snapshot() {
    ++ticks;
    if (ticks == 1) Log("event=first_game_thread_tick");
    if ((!Bound() || !RegisteredMenu()) && ticks % 5 == 1) Discover();
    if (ticks == 1) Log(DiscoveryState());
    if (!Bound() || !RegisteredMenu() || Opening() != 2) {
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
}
bool SafeSnapshot() {
    __try { Snapshot(); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool HandleKey(WPARAM key) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (hasMenu && key == 'R') { repeat = true; return true; }
    if (hasMenu && key == VK_F1) { help = true; return true; }
    return false;
}
std::wstring ConsumeSpeech(bool foreground, bool wasForeground, bool& readableMenu) {
    std::wstring speech;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        speech.swap(pendingSpeech);
        if (!speech.empty()) lastSpeech = speech;
        if (foreground && !wasForeground && speech.empty() && hasMenu) speech = lastSpeech;
        if (repeat.exchange(false)) speech = lastSpeech;
        if (help.exchange(false)) speech = L"Menu controls. Use the game's arrow keys, Enter to select and Escape to go back, or the controller's native menu buttons. R repeats. F1 reads this help. F3 previews navigation sounds. Scanner controls become available during gameplay.";
        readableMenu = hasMenu;
    }
    return speech;
}
}
