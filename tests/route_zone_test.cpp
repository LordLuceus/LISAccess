#ifdef NDEBUG
#undef NDEBUG
#endif
#include "features/navigation/route_guide.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
using namespace lis::navigation::route;
using lis::world::Vec3;
namespace {
// Provenance: current living-room native log request 3 supplied by the parent.
// This ONE positively supported, body-clear floor node is not a room graph or
// a claim about native visibility. Both objects serialize inner/outer=100,
// far=250. Their class CDO defaults FSM to -1; live FSM was not captured.
// Only floor/body clearance and authored zone membership are asserted here.
// No simulated visibility evidence is required or emitted by the core.
constexpr Vec3 capturedNode{697.368f,-228.982f,1.0f};
constexpr Vec3 answeringPosition{668.8221435546875f,-320.7958068847656f,94.34617614746094f};
constexpr Vec3 mailPosition{-32.41411590576172f,-17.8775634765625f,89.25010681152344f};
lis::world::Snapshot CapturedCandidate() {
    lis::world::Snapshot s;s.active=true;s.radius=21;s.halfHeight=44;
    // Normalize body centre from the captured floor; actual pawn origin was not
    // supplied with this isolated candidate, so do not invent a captured offset.
    s.player={capturedNode.x,capturedNode.y,capturedNode.z+s.halfHeight};
    lis::world::Target t;
    t.stableId="e1_5b_livingroom_ld.TheWorld.PersistentLevel.WhatIfInteractionActor_AnsweringMachine_79";
    t.position=answeringPosition;t.approachRadius=100;
    s.targets.push_back(t);return s;
}
TraceSample Flat(Vec3 a,Vec3 b,Vec3 e) {
    if(!e.x&&b.z<a.z-60)return {true,true,{a.x,a.y,1},{0,0,1}};
    return e.x?TraceSample{true,false}:TraceSample{};
}
TraceSample Isolated(Vec3 a,Vec3 b,Vec3 e) {
    // Other graph edges deliberately blocked: the capture only establishes this
    // candidate. This is NOT evidence that the surrounding real room is blocked.
    if(e.x&&(a.x!=b.x||a.y!=b.y))return {true,true,a,{-1,0,0}};
    return Flat(a,b,e);
}
Result Run(Guide& guide,lis::world::Snapshot& s,uint64_t& now,const Trace& trace=Isolated) {
    for(int i=0;i<400;++i) {
        now+=50;const auto r=guide.Tick(s,{1,2,3},now,trace);
        if(r.status!=Status::Planning)return r;
    }
    assert(false);return {};
}
void ApproachWithoutVision(lis::world::Snapshot s,uint64_t& now) {
    Guide guide;
    for(bool unreadable:{false,true}) {
        unsigned visionRays=0;guide.Start(s.targets[0].stableId);
        const auto r=Run(guide,s,now,[&](Vec3 a,Vec3 b,Vec3 e) {
            if(!e.x&&b.z>=a.z-60) {
                ++visionRays;
                return unreadable?TraceSample{}:TraceSample{true,true,b,{-1,0,0}};
            }
            return Isolated(a,b,e);
        });
        assert(r.status==Status::Approach&&!r.hasWaypoint&&visionRays==0);
    }
    // In-zone is insufficient when the actual body clearance fails.
    guide.Start(s.targets[0].stableId);
    const auto blocked=Run(guide,s,now,[](Vec3 a,Vec3 b,Vec3 e) {
        return e.x?TraceSample{true,true,a,{-1,0,0}}:Flat(a,b,e);
    });
    assert(blocked.status==Status::Limit&&!blocked.hasWaypoint);
}
}
int main() {
    Guide guide;uint64_t now=0;auto s=CapturedCandidate();
    const double distance=std::hypot(double(s.player.x)-s.targets[0].position.x,
                                    double(s.player.y)-s.targets[0].position.y);
    assert(distance>72&&distance<100);
    guide.Start(s.targets[0].stableId);auto r=Run(guide,s,now);
    assert(r.status==Status::Approach&&!r.hasWaypoint);
    assert(guide.Diagnostics().stats.nodes==1&&guide.Diagnostics().stats.edges==0);
    ApproachWithoutVision(s,now);
    // Same candidate and geometry, different explicit native-zone parameter.
    s.targets[0].approachRadius=72;now+=50;r=guide.Tick(s,{1,2,3},now,Isolated);
    assert(!r.hasWaypoint&&guide.Diagnostics().searches==2);
    assert(std::strcmp(guide.Diagnostics().restartCause,"target")==0);
    if(r.status==Status::Planning)r=Run(guide,s,now);
    assert(r.status==Status::NoRoute);
    s.targets[0].approachRadius=100;r=Run(guide,s,now);
    assert(r.status==Status::Approach&&guide.Diagnostics().searches==3);

    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(float radius:{0.f,-1.f,nan,inf,-inf}) {
        s.targets[0].approachRadius=radius;r=Run(guide,s,now);
        assert(r.status==Status::Unsupported&&!r.hasWaypoint);
        assert(std::strcmp(guide.Diagnostics().cause,"approach_zone_unavailable")==0);
        assert(s.targets.size()==1); // Core never filters scanner targets.
        s.targets[0].approachRadius=100;r=Run(guide,s,now);assert(r.status==Status::Approach);
    }
    auto missing=CapturedCandidate();missing.targets[0].approachRadius=lis::world::Target{}.approachRadius;
    guide.Start(missing.targets[0].stableId);r=Run(guide,missing,now);
    assert(r.status==Status::Unsupported&&guide.Diagnostics().searches==0);

    // Strict <100, not <=100; this is synthetic geometry with the same body.
    s=CapturedCandidate();s.targets[0].position={0,0,45};s.player={100,0,45};
    guide.Start(s.targets[0].stableId);r=Run(guide,s,now);assert(r.status==Status::NoRoute);
    s.player.x=std::nextafter(100.f,0.f);guide.Start(s.targets[0].stableId);r=Run(guide,s,now);
    assert(r.status==Status::Approach);
    s.player.x=std::nextafter(100.f,inf);guide.Start(s.targets[0].stableId);r=Run(guide,s,now);
    assert(r.status==Status::NoRoute);
    // A near-endpoint tolerance must not label an actual boundary position as
    // Approach. Walk the already verified tail into the strict inner zone.
    s.player={100,0,45};s.radius=10;guide.Start(s.targets[0].stableId);
    r=Run(guide,s,now,Flat);assert(r.status==Status::Following&&r.hasWaypoint);
    assert(std::hypot(r.waypoint.x,r.waypoint.y)<100);
    s.player=r.waypoint;r=Run(guide,s,now,Flat);assert(r.status==Status::Approach);

    // Pin Mail_63's supplied position/property too, using an explicitly
    // SYNTHETIC candidate 96 units away, not an unprovided capture node.
    s=CapturedCandidate();s.targets[0].stableId="Mail_63";
    s.targets[0].position=mailPosition;s.player={mailPosition.x+96,mailPosition.y,45};
    guide.Start("Mail_63");r=Run(guide,s,now);assert(r.status==Status::Approach);
    ApproachWithoutVision(s,now);
    s.targets[0].approachRadius=72;guide.Start("Mail_63");r=Run(guide,s,now);assert(r.status==Status::NoRoute);
    // Point locations still do not require an interaction-zone property.
    s.targets[0].kind=lis::world::Kind::Destination;s.targets[0].approachRadius=0;
    s.targets[0].position=s.player;guide.Start("Mail_63");r=Run(guide,s,now);
    assert(r.status==Status::Arrived);
    std::cout<<"PASS: supplied strict inner zones, captured answering-machine candidate, invalid and dynamic radii\n";
}
