#pragma once
#include "engine/abi.h"
namespace lis::world {
struct Session;
struct Snapshot;
// Internal game-thread scan boundary; a native scan fault latches independently.
bool SafeInteractions(Session& session,Snapshot& snapshot);
bool Binding(Session& session,unsigned slot,engine::Name& out);
}
