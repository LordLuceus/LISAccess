#ifdef NDEBUG
#undef NDEBUG
#endif
#include "features/navigation/route_guide.h"
#include <cassert>
#include <iostream>
using namespace lis::navigation::route;
using lis::world::Vec3;
namespace {
lis::world::Snapshot Sample(float x=60,float y=0) {
    lis::world::Snapshot s;s.active=true;s.player={0,0,44};s.radius=10;s.halfHeight=40;
    lis::world::Target t;t.stableId="goal";t.position={x,y,44};
    t.approachRadius=72; // Explicit synthetic test parameter, not a game default.
    s.targets.push_back(t);return s;
}
TraceSample Flat(Vec3 a,Vec3 b,Vec3 e) {
    if(!e.x&&b.z<a.z-60)return {true,true,{a.x,a.y,0},{0,0,1}};
    return {true,false};
}
bool Box(Vec3 a,Vec3 b,Vec3 e) {
    float lo=0,hi=1;
    const float av[2]={a.x,a.y},bv[2]={b.x,b.y};
    const float lower[2]={30-e.x,-25-e.y},upper[2]={40+e.x,25+e.y};
    for(int k=0;k<2;++k) {
        const float d=bv[k]-av[k];
        if(std::fabs(d)<.001f){if(av[k]<lower[k]||av[k]>upper[k])return false;}
        else {
            float t0=(lower[k]-av[k])/d,t1=(upper[k]-av[k])/d;
            if(t0>t1)std::swap(t0,t1);
            lo=std::max(lo,t0);hi=std::min(hi,t1);if(lo>hi)return false;
        }
    }
    return true;
}
Result Run(Guide& guide,lis::world::Snapshot& s,uint64_t& now,const Trace& trace) {
    for(unsigned i=0;i<350;++i) {
        now+=50;unsigned calls=0;
        const auto result=guide.Tick(s,{1,2,3},now,[&](Vec3 a,Vec3 b,Vec3 e){++calls;return trace(a,b,e);});
        assert(calls<=MaxPlanningTraceQueries);
        if(result.status!=Status::Planning)return result;
        assert(!result.hasWaypoint);
    }
    assert(false&&"finite planning window");return {};
}
}
int main() {
    Guide guide;uint64_t now=0;auto s=Sample(120);
    s.targets[0].kind=lis::world::Kind::Destination;
    s.targets[0].position.z=0;
    unsigned unsupported=0,drops=0,slopes=0;
    auto detour=[&](Vec3 a,Vec3 b,Vec3 e) {
        // The first +X branch crosses a drop/slope, but +Y then +X is flat.
        if(!e.x&&b.z<a.z-60&&a.x>0&&a.x<40&&std::fabs(a.y)<5) {
            ++unsupported;
            if(a.x<15){++drops;return TraceSample{true,false};}
            ++slopes;
            return TraceSample{true,true,{a.x,a.y,0},{.6f,0,.8f}};
        }
        return Flat(a,b,e);
    };
    guide.Start("goal");auto r=Run(guide,s,now,detour);
    assert(drops>0&&slopes>0&&r.status==Status::Following&&r.hasWaypoint);
    // Unsupported far diagonal shortcuts must fall back to the supported corner.
    const auto before=unsupported;
    now+=50;r=guide.Tick(s,{1,2,3},now,detour);
    assert(r.hasWaypoint&&r.status==Status::Following&&unsupported>before);
    assert(r.waypoint.y>0); // no shortcut straight across the missing floor

    // A skipped unsupported edge remains unknown when the other branches are
    // blocked; exhausting the frontier on a later tick cannot become NoRoute.
    guide.Start("goal");
    r=Run(guide,s,now,[](Vec3 a,Vec3 b,Vec3 e) {
        if(e.x&&(b.x<=0||b.y!=0))return TraceSample{true,true,a,{-1,0,0}};
        if(!e.x&&a.x>0)return TraceSample{true,false};
        return Flat(a,b,e);
    });
    assert(r.status==Status::Limit&&!r.hasWaypoint);

    // A wall between a safe approach position and the target is not a body
    // obstacle at that position. Neither blocked nor unreadable hypothetical
    // visual rays may be queried; native focus/actions alone determine arrival.
    for(bool unreadableVisual:{false,true}) {
        s=Sample();unsigned visionRays=0,bodyQueries=0,floorQueries=0;
        auto wall=[&](Vec3 a,Vec3 b,Vec3 e) {
            if(e.x) {
                ++bodyQueries;
                if(Box(a,b,e))return TraceSample{true,true,a,{-1,0,0}};
                return TraceSample{true,false};
            }
            if(b.z<a.z-60){++floorQueries;return Flat(a,b,e);}
            ++visionRays;
            return unreadableVisual?TraceSample{}:TraceSample{true,true,b,{-1,0,0}};
        };
        guide.Start("goal");r=Run(guide,s,now,wall);
        assert(r.status==Status::Approach&&!r.hasWaypoint);
        assert(visionRays==0&&bodyQueries>0&&floorQueries>0);
        assert(guide.Diagnostics().searches==1&&guide.Diagnostics().restarts==0);
        s.actions[0].label=L"Look";r=Run(guide,s,now,wall);
        assert(r.status==Status::Approach); // action belonging to another focus
        s.focusedId="goal";s.actions[0].label.clear();r=Run(guide,s,now,wall);
        assert(r.status==Status::Approach); // focus without action
        s.actions[0].label=L"Look";r=Run(guide,s,now,wall);
        assert(r.status==Status::Arrived&&!r.hasWaypoint);
        s.focusedId="other";r=Run(guide,s,now,wall);assert(r.status==Status::Approach);

        // Outside the zone, the same wall must still force verified body detours.
        s=Sample(120);guide.Start("goal");r=Run(guide,s,now,wall);
        assert(r.status==Status::Following&&r.hasWaypoint);
        for(int i=0;i<40&&r.hasWaypoint;++i) {
            assert(!Box(s.player,r.waypoint,{s.radius,s.radius,s.halfHeight}));
            s.player=r.waypoint;s.player.z=44;r=Run(guide,s,now,wall);
        }
        assert(r.status==Status::Approach&&!r.hasWaypoint&&visionRays==0);
    }
    // Body collisions remain blocking, whether planning or checking the final
    // approach. There is no selected-object exception to physical clearance.
    auto bodyWall=[](Vec3 a,Vec3 b,Vec3 e) {
        return e.x?TraceSample{true,true,a,{-1,0,0}}:Flat(a,b,e);
    };
    s=Sample(200);guide.Start("goal");r=Run(guide,s,now,bodyWall);
    assert(r.status==Status::NoRoute&&!r.hasWaypoint);
    s=Sample();guide.Start("goal");r=Run(guide,s,now,bodyWall);
    assert(r.status==Status::Limit&&!r.hasWaypoint); // bounded obstruction replans
    // Unknown body evidence still fails closed, even at an in-zone start.
    guide.Start("goal");r=Run(guide,s,now,[](Vec3 a,Vec3 b,Vec3 e) {
        return e.x?TraceSample{}:Flat(a,b,e);
    });
    assert(r.status==Status::Unavailable&&!r.hasWaypoint);
    // Calibration succeeds, but the final zero-length swept-body check must
    // still reject a subsequently missing or unreadable support sample.
    for(bool unknownFloor:{false,true}) {
        bool swept=false;guide.Start("goal");
        r=Run(guide,s,now,[&](Vec3 a,Vec3 b,Vec3 e) {
            if(e.x){swept=true;return TraceSample{true,false};}
            if(swept)return unknownFloor?TraceSample{}:TraceSample{true,false};
            return Flat(a,b,e);
        });
        assert(r.status==(unknownFloor?Status::Unavailable:Status::Unsupported)&&!r.hasWaypoint);
    }
    std::cout<<"PASS: approach-only native focus, zero vision rays, body detours and failed clearance\n";
}
