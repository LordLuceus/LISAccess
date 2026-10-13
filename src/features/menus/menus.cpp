#include "features/menus/menus.h"
#include "features/menus/state.h"
#include "features/menus/diagnostics.h"
#include "features/menus/narration.h"
#include "features/menus/popup.h"
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
bool pendingInterrupt = true;
StartupDelivery startupDelivery;
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
    if(!PopupValuePresent(page))page.clear();
    // Message-only focus objects (notably DNEAutosaveBox) have no selection.
    int selection = -1;
    if (!page.empty()&&!IsStartupSequence(page)) {
        float selectionValue = Number(focus + ".current_selection");
        if (!std::isfinite(selectionValue) || selectionValue < 0 || selectionValue > 100) return;
        selection = static_cast<int>(selectionValue);
    }
    const std::string selected = focus + ".selection_clips." + std::to_string(selection);
    std::vector<std::wstring> parts;
    int modalType = -1;
    if (page == L"CMD_OUTGAMESLOT") {
        for (const char* field : {"title_mc", "episode_mc", "sub_episode_mc", "time_value_mc"})
            parts.push_back(Text(selected + "." + field));
    } else if (!page.empty()&&!IsStartupSequence(page)) parts.push_back(Text(selected));
    else {
        parts = PopupText(PopupRoot(page,focus),
            [](const std::string& path) { return DisplayString(path); },
            [](const std::string& path) { return Number(path); });
        // Preserve legacy message boxes whose TEXT is not a composed label.
        if (parts.empty() && PopupVisible(focus + ".TEXT",
                [](const std::string& path) { return DisplayString(path); }))
            parts.push_back(Text(focus + ".TEXT"));
        if (PopupValuePresent(String(focus + ".message_type"))) {
            float value = Number(focus + ".message_type");
            if (std::isfinite(value) && value >= 0 && value <= 4) modalType = static_cast<int>(value);
        }
    }
    std::wstring text = DescribeMenu(parts, modalType);
    const std::string sequence=IsStartupSequence(page)?std::to_string(Number(focus+".SequenceIndex")):"none";
    std::string identity = Utf8(page) + ":" + sequence + ":" + std::to_string(count) + ":" + std::to_string(selection) + ":" + Utf8(text);
    if (identity != lastIdentity) {
        lastIdentity = identity;
        Log("event=menu_focus page=" + Utf8(page) + " sequence="+sequence+" depth=" + std::to_string(count) + " selection=" + std::to_string(selection) + " text=" + Utf8(text));
        std::lock_guard<std::mutex> lock(stateMutex);
        hasMenu = !text.empty()||(IsStartupSequence(page)&&!lastSpeech.empty());
        if (!text.empty()) {pendingSpeech = text;pendingInterrupt=startupDelivery.Interrupt(page,true);}
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
std::wstring ConsumeSpeech(bool foreground, bool wasForeground, bool& readableMenu,bool& interrupt) {
    std::wstring speech;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        speech.swap(pendingSpeech);
        interrupt=pendingInterrupt;
        if (!speech.empty()) lastSpeech = speech;
        if (foreground && !wasForeground && speech.empty() && hasMenu) {speech = lastSpeech;interrupt=true;}
        if (repeat.exchange(false)) {speech = lastSpeech;interrupt=true;}
        if (help.exchange(false)) {interrupt=true;speech = L"Menu controls. Use the game's arrow keys, Enter to select and Escape to go back, or the controller's native menu buttons. R repeats. F1 reads this help. F3 previews navigation sounds. Scanner controls become available during gameplay.";}
        readableMenu = hasMenu;
    }
    return speech;
}
}
