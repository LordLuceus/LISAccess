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
struct Scene {
    bool wall=false,boxed=false,unknown=false,drop=false,slope=false;
    unsigned calls=0;
    // Closed expanded AABB intersection models a swept body, including corners.
    static bool Box(Vec3 a,Vec3 b,Vec3 e,float x0,float x1,float y0,float y1) {
        float lo=0,hi=1;
        const float av[2]={a.x,a.y},bv[2]={b.x,b.y};
        const float lower[2]={x0-e.x,y0-e.y},upper[2]={x1+e.x,y1+e.y};
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
    TraceSample operator()(Vec3 a,Vec3 b,Vec3 e) {
        ++calls;if(unknown)return {};
        if(e.x) {
            if(boxed||(wall&&(Box(a,b,e,70,90,-100,100)||Box(a,b,e,70,200,80,100))))
                return {true,true,{a.x,a.y,a.z},{-1,0,0}};
            return {true,false};
        }
        if(b.z<a.z-60) {
            if(drop)return {true,false};
            return {true,true,{a.x,a.y,0},slope?Vec3{.6f,0,.8f}:Vec3{0,0,1}};
        }
        return {true,false};
    }
};
lis::world::Snapshot Sample(float x=300,float y=0) {
    lis::world::Snapshot s;s.active=true;s.player={0,0,44};s.radius=20;s.halfHeight=40;
    s.targets.push_back({"goal",L"goal",{x,y,0},lis::world::Kind::Destination});
    s.targets[0].approachRadius=72; // Explicit synthetic test parameter, not a game default.
    return s;
}
Identity id{1,2,3};
Result Run(Guide& g,lis::world::Snapshot& s,Scene& scene,uint64_t& time) {
    Result r;
    for(int i=0;i<350;++i) {
        time+=50;const auto before=scene.calls;
        r=g.Tick(s,id,time,std::ref(scene));
        assert(scene.calls-before<=MaxPlanningTraceQueries); // calibration plus bounded body/floor probes
        if(r.status!=Status::Planning)return r;
    }
    assert(false&&"bounded termination");return r;
}
}
int main() {
    Guide g;Scene scene;auto s=Sample();uint64_t time=0;
    assert(g.Tick(s,id,time,std::ref(scene)).status==Status::Off);
    g.Start("goal");auto r=Run(g,s,scene,time);
    assert(r.status==Status::Following&&r.hasWaypoint&&r.waypoint.x>=80);
    // Captured origin 44 / half-height 40 must normalize to body Z 40.
    assert(r.waypoint.z==40);
    for(int i=0;i<100;++i) {
        time+=10000;r=g.Tick(s,id,time,std::ref(scene));
        assert(r.status==Status::Following&&!r.replanning&&r.hasWaypoint);
    }
    s.player.x=r.waypoint.x+15;time+=50;r=g.Tick(s,id,time,std::ref(scene));
    assert(r.status==Status::Following&&r.waypoint.x>s.player.x); // overshoot/manual input
    s.active=false;time+=50;r=g.Tick(s,id,time,std::ref(scene));
    assert(r.status==Status::Unavailable&&!r.hasWaypoint);
    s.active=true;r=Run(g,s,scene,time);assert(r.hasWaypoint);
    s.targets[0].position.x+=5;time+=50;r=g.Tick(s,id,time,std::ref(scene));
    assert(r.status==Status::Following&&!r.replanning); // scanner noise
    s.targets[0].position={-200,0,0};time+=50;r=g.Tick(s,id,time,std::ref(scene));
    assert(r.status==Status::Planning&&!r.hasWaypoint&&r.replanning);
    r=Run(g,s,scene,time);assert(r.hasWaypoint&&r.waypoint.x<s.player.x);
    time+=50;r=g.Tick(s,{1,2,4},time,std::ref(scene));assert(!r.hasWaypoint);
    time+=50;r=g.Tick(s,{0,2,4},time,std::ref(scene));assert(r.status==Status::Unavailable&&!r.hasWaypoint);
    r=Run(g,s,scene,time);assert(r.hasWaypoint);
    r=g.Tick(s,id,time-1,std::ref(scene));assert(r.status==Status::Unavailable&&!r.hasWaypoint);
    r=g.Tick(s,id,time-2,std::ref(scene));assert(r.status==Status::Unavailable&&!r.hasWaypoint);
    s.targets.clear();time+=50;assert(g.Tick(s,id,time,std::ref(scene)).status==Status::Unavailable);

    s=Sample();g.Start("goal");scene.unknown=true;
    assert(Run(g,s,scene,time).status==Status::Unavailable);
    scene.unknown=false;scene.drop=true;g.Start("goal");
    assert(Run(g,s,scene,time).status==Status::Unsupported);
    scene.drop=false;scene.slope=true;g.Start("goal");
    assert(Run(g,s,scene,time).status==Status::Unsupported);
    scene.slope=false;scene.boxed=true;g.Start("goal");
    assert(Run(g,s,scene,time).status==Status::NoRoute);
    scene.boxed=false;s.targets[0].position.z=150;g.Start("goal");
    assert(Run(g,s,scene,time).status==Status::Unsupported);
    s=Sample(30000);g.Start("goal");assert(Run(g,s,scene,time).status==Status::Limit);

    // Newly obstructed evidence never publishes the previous waypoint.
    s=Sample();g.Start("goal");r=Run(g,s,scene,time);assert(r.hasWaypoint);
    scene.boxed=true;time+=50;r=g.Tick(s,id,time,std::ref(scene));
    assert(r.status==Status::Planning&&!r.hasWaypoint&&r.replanning);
    assert(Run(g,s,scene,time).status==Status::NoRoute);scene.boxed=false;
    g.Start("goal");r=Run(g,s,scene,time);assert(r.hasWaypoint);
    auto unreadableBody=[&](Vec3 a,Vec3 b,Vec3 e) {
        return e.x?TraceSample{}:scene(a,b,e);
    };
    time+=50;r=g.Tick(s,id,time,unreadableBody);
    assert(r.status==Status::Unavailable&&!r.hasWaypoint);
    g.Start("goal");time+=50;r=g.Tick(s,id,time,unreadableBody);
    assert(r.status==Status::Unavailable&&!r.hasWaypoint);
    auto missingEdgeFloor=[&](Vec3 a,Vec3 b,Vec3 e) {
        if(!e.x&&a.x!=s.player.x)return TraceSample{true,false};
        return scene(a,b,e);
    };
    g.Start("goal");
    for(int i=0;i<350;++i) {
        time+=50;r=g.Tick(s,id,time,missingEdgeFloor);
        if(r.status!=Status::Planning)break;
        assert(!r.hasWaypoint);
    }
    assert(r.status==Status::Limit&&!r.hasWaypoint);
    // Continuous displacement during planning ends at a bounded restart limit.
    s=Sample(700);g.Start("goal");
    for(int i=0;i<12;++i) {
        s.player.y+=25;time+=50;r=g.Tick(s,id,time,std::ref(scene));
        if(r.status==Status::Limit)break;
        assert(r.status==Status::Planning&&!r.hasWaypoint);
    }
    assert(r.status==Status::Limit);

    s=Sample(200,0);scene.wall=true;g.Start("goal");r=Run(g,s,scene,time);
    assert(r.status==Status::Following&&r.hasWaypoint);
    for(int i=0;i<100&&r.status!=Status::Arrived;++i) {
        if(r.hasWaypoint) {
            Vec3 body=s.player;body.z=40;
            assert(!Scene::Box(body,r.waypoint,{20,20,40},70,90,-100,100));
            assert(!Scene::Box(body,r.waypoint,{20,20,40},70,200,80,100));
            s.player=r.waypoint;s.player.z=44;
        }
        r=Run(g,s,scene,time);
    }
    assert(r.status==Status::Arrived&&!r.hasWaypoint);
    scene.wall=false;s=Sample(200);s.targets[0].kind=lis::world::Kind::Object;
    g.Start("goal");r=Run(g,s,scene,time);
    for(int i=0;i<20&&r.hasWaypoint;++i) {
        s.player=r.waypoint;s.player.z=44;r=Run(g,s,scene,time);
    }
    assert(r.status==Status::Approach&&!r.hasWaypoint);
    s.focusedId="goal";r=Run(g,s,scene,time);assert(r.status==Status::Approach);
    s.actions[0].label=L"Look";r=Run(g,s,scene,time);assert(r.status==Status::Arrived);
    s.focusedId="other";r=Run(g,s,scene,time);assert(r.status==Status::Approach);
    // Movement while planning invalidates the old start; teleport cannot advance it.
    s=Sample(700);g.Start("goal");time+=50;g.Tick(s,id,time,std::ref(scene));
    s.player={400,0,44};time+=50;r=g.Tick(s,id,time,std::ref(scene));
    assert(r.status==Status::Planning&&!r.hasWaypoint);
    r=Run(g,s,scene,time);assert(r.hasWaypoint&&r.waypoint.x>400);
    s.player={-800,0,44};time+=50;r=g.Tick(s,id,time,std::ref(scene));assert(!r.hasWaypoint);
    s.player.x=std::numeric_limits<float>::quiet_NaN();
    time+=50;assert(g.Tick(s,id,time,std::ref(scene)).status==Status::Unavailable);
    std::cout<<"PASS: guided route lifecycle, L wall, manual progress, budgets and arrival\n";
}
