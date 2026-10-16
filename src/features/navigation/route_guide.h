#pragma once
#include "features/navigation/route_geometry.h"
#include "features/navigation/route_routing.h"
#include <cstdint>
#include <functional>
#include <string>

namespace lis::navigation::route {
// A slice may retire cheap visited-neighbor work as well as probe edges. Eight
// operations left real searches idle for seconds despite only milliseconds of
// work. Keep the existing time slice and leave headroom under the native cap.
inline constexpr std::size_t PlanningWorkPerTick=64;
inline constexpr double PlanningSliceMilliseconds=1.5;
inline constexpr unsigned MaxPlanningTraceQueries=1+3*PlanningWorkPerTick;
struct Identity { uintptr_t controller=0,pawn=0,world=0; };
enum class Status { Off,Planning,Following,Approach,Arrived,NoRoute,Unsupported,Limit,Unavailable };
struct Result {
    Status status=Status::Off;
    world::Vec3 waypoint{};
    bool hasWaypoint=false;
    bool replanning=false;
};
using Trace=std::function<TraceSample(world::Vec3,world::Vec3,world::Vec3)>;
struct Diagnostic {
    // Static literals; found survives Following/Approach/Arrived. Fail overrides
    // cause but preserves stats before resetting the planner. Stats cover the
    // latest search, while searches/restarts cover the whole request.
    const char* cause="none";
    const char* restartCause="none";
    routing::Statistics stats{};
    unsigned searches=0,restarts=0; // Restarts include denied attempts.
    uint64_t searchAge=0; // Milliseconds since latest Begin; frozen after Fail.
    Ground ground{};
    world::Vec3 player{},target{};
};
class Guide {
public:
    // Single-threaded, engine-free. now is monotonic milliseconds. Start resets
    // terminal state. Trace callbacks must be bounded by the caller; they cannot
    // be preempted. Waypoints use normalized floor+halfHeight body coordinates.
    void Start(const std::string& selected);
    void Reset();
    Result Tick(const world::Snapshot&,Identity,uint64_t now,const Trace&);
    Diagnostic Diagnostics() const { return diagnostic_; }
private:
    Result Fail(Status,const char* cause);
    bool Begin(const world::Target&,const Ground&,uint64_t);
    Result Follow(const world::Snapshot&,const world::Target&,const Ground&,const Trace&);
    std::string selected_;
    Identity identity_{};
    routing::Planner planner_;
    Diagnostic diagnostic_{};
    uint64_t diagnosticStart_=0;
    Ground ground_{};
    world::Vec3 target_{},lastPlayer_{},volumeExtent_{};
    float approachRadius_=0;
    world::Kind kind_=world::Kind::Object;
    Status status_=Status::Off;
    uint64_t lastTime_=0,planningSince_=0;
    unsigned restarts_=0;
    bool sampled_=false,replanning_=false,clocked_=false;
};
}
