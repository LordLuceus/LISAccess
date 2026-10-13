#pragma once
#include <Windows.h>
#include <string>

namespace lis::menus {
// Game-thread inspection retains its structured-exception boundary.
bool SafeSnapshot();
bool HandleKey(WPARAM key);
// Worker-thread handoff; consumes pending speech/repeat/help under one lock.
std::wstring ConsumeSpeech(bool foreground, bool wasForeground, bool& readableMenu,bool& interrupt);
}
