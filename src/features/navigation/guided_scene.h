#pragma once
#include "features/navigation/route_guide.h"
#include <cstdint>
namespace lis::navigation::guided_scene {
inline constexpr unsigned MaxQueriesPerFrame=256;
static_assert(route::MaxPlanningTraceQueries<=MaxQueriesPerFrame,"Planning slice exceeds native query safety cap");
struct Frame {
    world::Snapshot snapshot;
    route::Identity identity;
    uint64_t sampledAt=0;
    unsigned queries=0;
};
bool Thread() noexcept;
route::Identity CurrentIdentity();
bool Capture(const world::Snapshot&,route::Identity expected,Frame&);
route::TraceSample Trace(Frame&,world::Vec3 from,world::Vec3 to,world::Vec3 extent);
}
