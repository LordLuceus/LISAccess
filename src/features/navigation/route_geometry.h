// Ported from the shelved autowalk pure geometry/planner regression; no native dependencies.
#pragma once
#include "features/world/world.h"
#include <cmath>

namespace lis::navigation::route {
// Flat-ground planning only. A successful probe is geometry evidence, not native
// action availability, a movement command, or proof of rendered visibility.
enum class Clearance { Clear, Blocked, Unsupported, Unknown, Invalid };
struct TraceSample {
    bool known=false,hit=false;
    world::Vec3 position{},normal{};
};
struct Segment {
    Clearance status=Clearance::Invalid;
    world::Vec3 end{};
    unsigned queries=0;
};
struct Ground {
    Clearance status=Clearance::Invalid;
    world::Vec3 floor{},body{};
    float radius=0,halfHeight=0,headMargin=0;
};
inline bool FinitePoint(world::Vec3 p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
inline bool ValidBody(float radius,float halfHeight) {
    return std::isfinite(radius)&&std::isfinite(halfHeight)&&radius>=1&&radius<=256&&
        halfHeight>=radius&&halfHeight<=512;
}
inline bool ValidHit(const TraceSample& hit) {
    if(!hit.known)return false;
    if(!hit.hit)return true;
    const auto n=hit.normal;
    const float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
    return FinitePoint(hit.position)&&FinitePoint(n)&&length>=.8f&&length<=1.2f;
}
inline bool FlatSupport(const TraceSample& hit) {
    const auto n=hit.normal;
    return hit.hit&&double(n.z)>=.99*std::hypot(std::hypot(double(n.x),double(n.y)),double(n.z));
}
template<class Trace>
Ground CalibrateGround(world::Vec3 player,float radius,float halfHeight,Trace trace) {
    Ground out;
    if(!ValidBody(radius,halfHeight)||!FinitePoint(player))return out;
    auto down=player;down.z-=halfHeight*3;
    if(!FinitePoint(down))return out;
    const auto support=trace(player,down,world::Vec3{});
    if(!ValidHit(support)){out.status=Clearance::Unknown;return out;}
    if(!FlatSupport(support)){out.status=Clearance::Unsupported;return out;}
    const float separation=player.z-support.position.z;
    if(std::fabs(support.position.x-player.x)>1||std::fabs(support.position.y-player.y)>1||
       separation<halfHeight*.75f||separation>halfHeight*1.25f) {
        out.status=Clearance::Unsupported;return out;
    }
    out.status=Clearance::Clear;out.floor=support.position;out.body=out.floor;
    out.body.z+=halfHeight;out.radius=radius;out.halfHeight=halfHeight;
    // Captured pawn origin is above floor+halfHeight. Preserve that extra head
    // clearance when expressing the graph in normalized floor/body coordinates.
    out.headMargin=separation>halfHeight?separation-halfHeight:0;return out;
}
// start.z is a previously calibrated body centre (floor + halfHeight). Segment
// length is bounded to one diameter. Reject slopes/steps instead of guessing that
// the custom character controller can traverse them. No native calls in this file.
template<class Trace>
Segment CheckFlatSegment(world::Vec3 start,world::Vec3 requested,float radius,float halfHeight,Trace trace,float headMargin=0,float horizontalMargin=0) {
    Segment out;
    if(!ValidBody(radius,halfHeight)||!std::isfinite(headMargin)||headMargin<0||headMargin>halfHeight*.25f||
       !std::isfinite(horizontalMargin)||horizontalMargin<0||horizontalMargin>radius||
       !FinitePoint(start)||!FinitePoint(requested)||
       std::fabs(requested.z-start.z)>1||
       std::hypot(requested.x-start.x,requested.y-start.y)>radius*2+.01f)return out;
    const float floor=start.z-halfHeight;
    requested.z=start.z;
    // Full width and full head clearance; lift the bottom one unit off calibrated
    // floor so a flat supporting surface is not mistaken for a forward blocker.
    auto a=start,b=requested;a.z+=.5f+headMargin*.5f;b.z+=.5f+headMargin*.5f;
    auto body=trace(a,b,world::Vec3{radius+horizontalMargin,radius+horizontalMargin,halfHeight-.5f+headMargin*.5f});++out.queries;
    if(!ValidHit(body)){out.status=Clearance::Unknown;return out;}
    if(body.hit){out.status=body.normal.z>=.65f?Clearance::Unsupported:Clearance::Blocked;return out;}
    // Check midpoint as well as endpoint: two supported endpoints alone do not
    // establish support between them. Tiny gaps below sample spacing remain a
    // limitation; guidance rechecks from the player but never controls movement.
    for(float t:{.5f,1.f}) {
        world::Vec3 from{start.x+(requested.x-start.x)*t,start.y+(requested.y-start.y)*t,start.z};
        auto down=from;down.z-=halfHeight*3;
        auto support=trace(from,down,world::Vec3{});++out.queries;
        if(!ValidHit(support)){out.status=Clearance::Unknown;return out;}
        if(support.hit&&(std::fabs(support.position.x-from.x)>1||std::fabs(support.position.y-from.y)>1)) {
            out.status=Clearance::Unknown;return out;
        }
        if(!FlatSupport(support)||std::fabs(support.position.z-floor)>1) {
            out.status=Clearance::Unsupported;return out;
        }
        if(t==1)out.end={requested.x,requested.y,support.position.z+halfHeight};
    }
    out.status=Clearance::Clear;return out;
}
}
