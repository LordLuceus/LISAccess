#pragma once
#include "features/navigation/collision.h"
namespace lis::world {
// Calibrate support, then sample bounded body sweeps and the forward drop ray.
bool SampleGeometry(const engine::ObjectRegistry& registry,uintptr_t base,uintptr_t trace,
                    uintptr_t pawn,Snapshot& snapshot,unsigned& queries);
}
