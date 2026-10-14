#pragma once
#include <Windows.h>
namespace lis::journal {
void Initialize();
void Shutdown();
void Tick();
bool HandleKey(WPARAM key,bool repeated);
// Worker returns whether the diary owns input, even on an unreadable page.
bool Pump(bool foreground);
}
