#ifdef NDEBUG
#undef NDEBUG
#endif
#include "features/navigation/route_guide.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace lis::navigation::route;
using lis::world::Vec3;
namespace {
// Provenance: captured campus exit bounds, DNEVolume_5. Player/floor poses
// below are controlled test inputs, not a recorded campus playthrough. Actual
// BrushBounds (AddDestination uses origin, not floor). Coordinates retained
// verbatim; floor approximately 0, pawn origin 44, half-height 40.
constexpr Vec3 campusOrigin{-590.4821167f,-3197.0419922f,99.0826111f};
constexpr Vec3 campusExtent{81.2416458f,198.7959290f,99.0849915f};
TraceSample Flat(Vec3 a,Vec3 b,Vec3 e) {
    if(!e.x&&b.z<a.z-60)return {true,true,{a.x,a.y,0},{0,0,1}};
    return {true,false};
}
lis::world::Snapshot Sample(Vec3 origin,Vec3 extent,Vec3 player) {
    lis::world::Snapshot s;s.active=true;s.player=player;s.radius=20;s.halfHeight=40;
    lis::world::Target t;t.stableId="exit";t.kind=lis::world::Kind::Exit;
    t.position=origin;t.volumeExtent=extent;s.targets.push_back(t);return s;
}
Result Run(Guide& guide,lis::world::Snapshot& s,uint64_t& now,const Trace& trace=Flat) {
    for(unsigned i=0;i<350;++i) {
        now+=50;
        const auto result=guide.Tick(s,{1,2,3},now,trace);
        if(result.status!=Status::Planning)return result;
        assert(!result.hasWaypoint);
    }
    assert(false&&"bounded route search");return {};
}
}
int main() {
    Guide guide;uint64_t now=0;
    auto s=Sample(campusOrigin,campusExtent,{campusOrigin.x-200,campusOrigin.y,44});
    guide.Start("exit");auto r=Run(guide,s,now);
    assert(r.status==Status::Following&&r.hasWaypoint&&r.waypoint.z==40);
    for(int i=0;i<20&&r.hasWaypoint;++i) {
        s.player=r.waypoint;s.player.z=44;r=Run(guide,s,now);
    }
    assert(r.status==Status::Arrived&&!r.hasWaypoint);
    assert(std::fabs(s.player.x-campusOrigin.x)<=campusExtent.x);
    assert(std::fabs(s.player.y-campusOrigin.y)<=campusExtent.y);

    // Same XY but a genuine different-floor volume must not become an XY goal.
    s=Sample({0,0,300},{80,80,50},{0,0,44});guide.Start("exit");
    r=Run(guide,s,now);assert(r.status==Status::Unsupported&&!r.hasWaypoint);
    // Membership uses actual pawn Z 44, not normalized body centre Z 40.
    s=Sample({0,0,44},{2,2,1},{-20,0,44});guide.Start("exit");
    r=Run(guide,s,now);assert(r.status==Status::Following&&r.hasWaypoint);
    assert(r.waypoint.x==0&&r.waypoint.y==0&&r.waypoint.z==40);
    s.player={0,0,44};r=Run(guide,s,now);assert(r.status==Status::Arrived);
    s.targets[0].position.z=40;now+=50;r=guide.Tick(s,{1,2,3},now,Flat);
    assert(r.status==Status::Unsupported&&!r.hasWaypoint);

    // Narrow volumes cannot use the generic 24-unit point arrival tolerance,
    // including Y-only outside positions. The tail remains collision verified.
    for(const auto player:{Vec3{-20,0,44},Vec3{0,20,44}}) {
        s=Sample({0,0,99},campusExtent,player);s.targets[0].volumeExtent.x=2;
        s.targets[0].volumeExtent.y=2;guide.Start("exit");
        r=Run(guide,s,now);assert(r.status==Status::Following&&r.hasWaypoint);
        assert(r.waypoint.x==0&&r.waypoint.y==0);
        s.player={0,0,44};r=Run(guide,s,now);assert(r.status==Status::Arrived);
    }
    // Reaching the tail centre beyond the graph endpoint must not guide back
    // toward the old endpoint (30 units away here).
    s=Sample({70,0,44},{1,1,2},{0,0,44});guide.Start("exit");r=Run(guide,s,now);
    for(int i=0;i<10&&r.hasWaypoint;++i) {
        s.player=r.waypoint;s.player.z=44;r=Run(guide,s,now);
    }
    assert(r.status==Status::Arrived&&s.player.x==70);

    // Bounds changes invalidate a previous Arrived result before reuse.
    s=Sample({0,0,44},{2,2,1},{1.5f,0,44});guide.Start("exit");
    r=Run(guide,s,now);assert(r.status==Status::Arrived);
    s.targets[0].volumeExtent.x=1;now+=50;r=guide.Tick(s,{1,2,3},now,Flat);
    assert(r.status==Status::Planning&&!r.hasWaypoint&&r.replanning);
    r=Run(guide,s,now);assert(r.status==Status::Following&&r.hasWaypoint&&r.waypoint.x==0);
    // Even a sub-noise-threshold centre shift changes the volume membership.
    s.targets[0].position.x=4;now+=50;r=guide.Tick(s,{1,2,3},now,Flat);
    assert(r.status==Status::Planning&&!r.hasWaypoint);

    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    for(const auto extent:{Vec3{},Vec3{0,2,2},Vec3{2,-1,2},Vec3{2,2,0},Vec3{nan,2,2},Vec3{2,inf,2},Vec3{2,2,nan}}) {
        s=Sample({0,0,44},extent,{0,0,44});guide.Start("exit");r=Run(guide,s,now);
        assert(r.status==Status::Unsupported&&!r.hasWaypoint);
    }
    s=Sample({0,0,44},{2,2,1},{-20,0,44});guide.Start("exit");r=Run(guide,s,now);
    now+=50;r=guide.Tick(s,{1,2,3},now,[](Vec3 a,Vec3 b,Vec3 e) {
        return e.x?TraceSample{true,true,a,{-1,0,0}}:Flat(a,b,e);
    });
    assert(r.status==Status::Planning&&!r.hasWaypoint);
    guide.Start("exit");r=Run(guide,s,now);
    now+=50;r=guide.Tick(s,{1,2,3},now,[&](Vec3 a,Vec3 b,Vec3 e) {
        if(!e.x&&a.x!=s.player.x)return TraceSample{true,false};
        return Flat(a,b,e);
    });
    assert(r.status==Status::Unsupported&&!r.hasWaypoint);
    // Point Destination rules remain independent of missing volume bounds.
    s=Sample({0,0,0},{},{-20,0,44});s.targets[0].kind=lis::world::Kind::Destination;
    guide.Start("exit");r=Run(guide,s,now);assert(r.status==Status::Arrived);
    s.targets[0].position.z=99;guide.Start("exit");r=Run(guide,s,now);
    assert(r.status==Status::Unsupported&&!r.hasWaypoint);
    std::cout<<"PASS: captured campus exit, actual-origin volume arrival, invalid bounds and verified tails\n";
}
