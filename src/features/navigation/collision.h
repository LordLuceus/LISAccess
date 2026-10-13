#pragma once
#include "features/world/world.h"
#include "engine/object_registry.h"
namespace lis::world {
struct TraceParams {
    Vec3 location, normal, end, start;
    uint32_t actors = 1;
    Vec3 extent;
    unsigned char hitInfo[28]{};
    int flags = 8;
    uintptr_t actor = 0;
};
static_assert(sizeof(TraceParams) == 100);
bool Finite(Vec3 value);
float Length(Vec3 value);
Vec3 Sub(Vec3 a,Vec3 b);
bool Query(const engine::ObjectRegistry& registry,uintptr_t base,uintptr_t trace,uintptr_t pawn,Vec3 start,Vec3 end,Vec3 extent,TraceParams& result);
}
