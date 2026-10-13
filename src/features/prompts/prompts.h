#pragma once
#include <cstdint>
namespace lis::prompts {
void Initialize(uintptr_t base); // worker before the game-thread timer starts
void Tick(); // independent guarded game-thread snapshot, including paused tutorials
void RequestRepeat(); // worker, only outside readable menus
void Pump(bool foreground,bool readableMenu); // worker, after menu/scanner speech
}
