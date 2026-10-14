#pragma once
#include "input/commands.h"
namespace lis::rewind {
void Tick();
void Pump(bool foreground,bool readableMenu);
bool HasContext();
bool HandleInput(const input::Presses& pressed,bool foreground,bool readableMenu,bool gamepad=false);
void Shutdown();
}
