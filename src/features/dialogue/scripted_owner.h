#pragma once
#include "features/dialogue/registered_choices.h"
namespace lis::world {struct Session;}
namespace lis::dialogue {
bool ScriptedOwner(world::Session& session,uintptr_t controller,uintptr_t world,RegisteredChoices& out);
bool ScriptedWaiting(world::Session& session,uintptr_t owner);
}
