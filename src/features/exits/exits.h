#pragma once
#include <cstdint>
namespace lis::world {struct Session;struct Snapshot;}
namespace lis::exits {
// Discovery only caches sources; current activation/ownership is checked again
// each sample. No input, touch events, transitions or story facts are synthesized.
bool IsSource(world::Session& session,uintptr_t announcement);
bool Sample(world::Session& session,world::Snapshot& snapshot);
}
