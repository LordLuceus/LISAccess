#include "features/navigation/collision.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include <Windows.h>
#include <cmath>
namespace lis::world {
using engine::Read;
bool Finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::fabs(v.x) < 1e7f && std::fabs(v.y) < 1e7f && std::fabs(v.z) < 1e7f; }
float Length(Vec3 v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static bool TraceVerified(const engine::ObjectRegistry& registry,uintptr_t trace) {
    return registry.Registered(trace) && registry.ClassOf(trace) == "Function" && registry.NameOf(trace) == "Trace" &&
        registry.NameOf(Read<uintptr_t>(trace + 0x28)) == "Actor" && Read<uint16_t>(trace + 0xa2) == 100 &&
        Read<uint16_t>(trace + 0xa4) == 96 && (Read<uint32_t>(trace + 0x90) & 0x400) && Read<uint16_t>(trace + 0x94) == 277;
}
// ONLY Actor.Trace(277): stock UE3 indexed-native SDK dispatch adapter.
// This transient write is dispatch metadata, not world/save state. Restore even on SEH.
// No other native index or function flag is ever changed.
static void DispatchTrace(uintptr_t base,uintptr_t trace,uintptr_t pawn, TraceParams* p) {
    volatile uint16_t* index = reinterpret_cast<volatile uint16_t*>(trace + 0x94);
    __try {
        *index = 0;
        // AActor's override (vtable72, RVA2a1b00) checks world readiness and calls
        // UObject::ProcessEvent. Both end in ret 0x0c, including the legacy result slot.
        reinterpret_cast<engine::ProcessEvent>(base + engine::profile::ActorDispatch)(reinterpret_cast<void*>(pawn), reinterpret_cast<void*>(trace), p, nullptr);
    } __finally { *index = 277; }
}
bool Query(const engine::ObjectRegistry& registry,uintptr_t base,uintptr_t trace,uintptr_t pawn, Vec3 start, Vec3 end, Vec3 extent, TraceParams& p) {
    if (!TraceVerified(registry,trace) || !registry.Instance(pawn) ||
        Read<uintptr_t>(Read<uintptr_t>(pawn) + 72 * 4) != base + engine::profile::ActorDispatch ||
        !Finite(start) || !Finite(end) || !Finite(extent)) return false;
    p = {}; p.start = start; p.end = end; p.extent = extent; p.actors = 1; p.flags = 8;
    DispatchTrace(base,trace,pawn, &p);
    if (!p.actor) return true; // meaningful only after this sample's positive floor calibration
    float n = Length(p.normal);
    return registry.Registered(p.actor) && p.actor != pawn &&
        Read<uintptr_t>(p.actor + 0xd8) == Read<uintptr_t>(pawn + 0xd8) &&
        Finite(p.location) && Finite(p.normal) && n >= 0.8f && n <= 1.2f &&
        Length(Sub(p.location, start)) <= Length(Sub(end, start)) + Length(extent) + 4;
}
}
