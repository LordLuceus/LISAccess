// Ported from the shelved autowalk pure geometry/planner regression; no native dependencies.
#pragma once
#include "features/navigation/route_planner.h"
#include "features/navigation/route_geometry.h"
#include <limits>

namespace lis::navigation::route {
inline routing::Status BeginOnGround(routing::Planner& planner,const Ground& ground,
                                    routing::Goal goal,routing::Config config={}) {
    if(ground.status!=Clearance::Clear){planner.Reset();return planner.GetStatus();}
    config.pitch=2*ground.radius;config.flatTolerance=1;
    const auto f=ground.floor,b=ground.body;
    return planner.Begin({f.x,f.y,f.z},{b.x,b.y,b.z},ground.radius,ground.halfHeight,goal,config);
}
// Adapter between floor-space planning and body-centre collision queries. No
// UObject access here: the caller must supply a validated trace
// callback and positively calibrate the initial floor before Planner::Begin.
inline bool NativeCoordinate(double x) {
    return std::isfinite(x)&&std::abs(x)<std::numeric_limits<float>::max()/4&&
        std::abs(static_cast<double>(static_cast<float>(x))-x)<=.01&&
        static_cast<float>(x+1)!=static_cast<float>(x);
}
template<class Trace>
routing::EdgeResult ProbeFloorEdge(const routing::EdgeQuery& q,const Ground& ground,Trace trace) {
    if(ground.status!=Clearance::Clear||q.radius!=ground.radius||q.halfHeight!=ground.halfHeight)
        return {routing::EdgeState::Unsupported,0};
    for(double x:{q.from.x,q.from.y,q.from.z,q.to.x,q.to.y,q.to.z,q.radius,q.halfHeight})
        if(!NativeCoordinate(x))return {routing::EdgeState::Unsupported,0};
    const float radius=static_cast<float>(q.radius),height=static_cast<float>(q.halfHeight);
    const world::Vec3 start{static_cast<float>(q.from.x),static_cast<float>(q.from.y),static_cast<float>(q.from.z)+height};
    const world::Vec3 goal{static_cast<float>(q.to.x),static_cast<float>(q.to.y),static_cast<float>(q.to.z)+height};
    const auto result=CheckFlatSegment(start,goal,radius,height,trace,ground.headMargin);
    if(result.status==Clearance::Clear)return {routing::EdgeState::Traversable,result.end.z-height};
    if(result.status==Clearance::Blocked)return {routing::EdgeState::Blocked,q.from.z};
    // Invalid, unsupported slope/step/drop and unreadable evidence cannot prove
    // the destination is unreachable. Planner distinguishes these from NoRoute.
    return {routing::EdgeState::Unsupported,q.from.z};
}
}
