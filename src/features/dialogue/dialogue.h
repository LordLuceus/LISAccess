#pragma once
#include "input/commands.h"
#include <optional>
namespace lis::dialogue {
void Tick(); // game-thread capture and explicit queued requests
bool HasContext(); // fresh worker-side dialogue context for controller help
bool HandleInput(const input::Presses& pressed,bool foreground,bool readableMenu,std::optional<bool> gamepad=std::nullopt);
void Pump(bool foreground,bool readableMenu);
}
