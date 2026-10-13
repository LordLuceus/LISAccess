#pragma once
#include <Windows.h>
namespace lis::navigation {
void Initialize(HMODULE module); // worker, before timer starts; build must be verified
void Tick(); // game/window thread; independent sampler fault boundary
void Pump(bool foreground, bool readableMenu); // worker; poll free keys, play cues
void Shutdown(); // worker, after timer is removed
}
