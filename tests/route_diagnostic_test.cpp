#ifdef NDEBUG
#undef NDEBUG
#endif
#include "features/navigation/route_guide.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
using namespace lis::navigation::route;
using lis::world::Vec3;
namespace {
bool Is(const char* a,const char* b) { return std::strcmp(a,b)==0; }
void Cause(const Guide& g,const char* cause) {
    if(!Is(g.Diagnostics().cause,cause))std::cerr<<"Expected "<<cause<<", got "<<g.Diagnostics().cause<<'\n';
    assert(Is(g.Diagnostics().cause,cause));
}
TraceSample Flat(Vec3 a,Vec3 b,Vec3 e) {
    if(!e.x&&b.z<a.z-60)return {true,true,{a.x,a.y,0},{0,0,1}};
    return {true,false};
}
lis::world::Snapshot Sample(float x=300) {
    lis::world::Snapshot s;s.active=true;s.player={0,0,44};s.radius=20;s.halfHeight=40;
    lis::world::Target t;t.stableId="goal";t.position={x,0,0};t.kind=lis::world::Kind::Destination;
    t.approachRadius=72; // Explicit synthetic test parameter, not a game default.
    s.targets.push_back(t);return s;
}
Result Tick(Guide& g,lis::world::Snapshot& s,uint64_t& now,const Trace& trace=Flat) {
    return g.Tick(s,{1,2,3},++now,trace);
}
Result Run(Guide& g,lis::world::Snapshot& s,uint64_t& now,const Trace& trace=Flat) {
    for(int i=0;i<4000;++i) {
        const auto r=Tick(g,s,now,trace);if(r.status!=Status::Planning)return r;
    }
    assert(false);return {};
}
void PlannerDiagnostics() {
    using namespace routing;
    Planner p;Config config;config.pitch=1;config.maxCells=2;
    const Goal goal{{10,0,0},0,0,0};
    auto begin=[&](){return p.Begin({0,0,0},{0,0,2},1,2,goal,config);};
    auto clear=[](const EdgeQuery&){return EdgeResult{EdgeState::Traversable,0};};
    auto run=[&](auto probe) {
        for(int i=0;i<2000&&p.GetStatus()==routing::Status::Planning;++i)p.Step(probe,8,10);
        assert(p.GetStatus()!=routing::Status::Planning);
    };
    assert(Is(p.StopReason(),"none"));
    begin();assert(Is(p.StopReason(),"planning"));
    run([](const EdgeQuery&){return EdgeResult{EdgeState::Blocked,0};});
    assert(Is(p.StopReason(),"frontier_empty")&&p.Stats().blocked==4&&p.Stats().edges==4);
    begin();p.Step([](const EdgeQuery&){return EdgeResult{EdgeState::Unsupported,0};},1,10);
    assert(p.Stats().unsupported==1&&Is(p.StopReason(),"planning"));
    run([](const EdgeQuery&){return EdgeResult{EdgeState::Blocked,0};});
    assert(Is(p.StopReason(),"exhausted_unknown")&&p.Stats().unsupported==1&&p.Stats().blocked==3);
    begin();run(clear);
    assert(Is(p.StopReason(),"exhausted_unknown")&&p.Stats().bounds==20&&p.Stats().nodes==25);
    assert(p.Stats().traversable==24&&p.Stats().unsupported==0);
    begin();run([](const EdgeQuery&){return EdgeResult{EdgeState::Deferred,0};});
    assert(Is(p.StopReason(),"exhausted_unknown")&&p.Stats().deferred==12&&p.Stats().deferredExhausted==4);
    config.maxNodes=1;begin();run(clear);
    assert(Is(p.StopReason(),"node_budget")&&p.Stats().traversable==1&&p.Stats().nodes==1);
    config.maxNodes=2048;config.maxEdges=1;begin();run(clear);
    assert(Is(p.StopReason(),"edge_budget")&&p.Stats().edges==1);
    config.maxEdges=8192;config.maxMilliseconds=std::numeric_limits<double>::min();begin();run(clear);
    assert(Is(p.StopReason(),"cpu_budget")&&p.Stats().milliseconds>=config.maxMilliseconds);
    // A late successful callback still exposes the actual CPU stop, not Found.
    config.maxMilliseconds=1;
    p.Begin({0,0,0},{0,0,2},1,2,{{1,0,0},0,0,0},config);
    p.Step([&](const EdgeQuery& q){std::this_thread::sleep_for(std::chrono::milliseconds(3));return clear(q);},1,10);
    assert(Is(p.StopReason(),"cpu_budget")&&p.Path().empty()&&p.Stats().milliseconds>=1);
    config.maxMilliseconds=1000;p.Begin({0,0,0},{0,0,2},1,2,{{1,0,0},0,0,0},config);run(clear);
    assert(Is(p.StopReason(),"found")&&p.Stats().traversable==1);
    p.Reset();assert(Is(p.StopReason(),"none")&&p.Stats().edges==0&&p.Stats().bounds==0);
    config.maxNodes=0;begin();assert(Is(p.StopReason(),"invalid"));
}
}
int main() {
    PlannerDiagnostics();Guide g;auto s=Sample();uint64_t now=0;
    assert(Is(g.Diagnostics().cause,"none")&&g.Diagnostics().searches==0);
    auto wall=[](Vec3 a,Vec3 b,Vec3 e){return e.x?TraceSample{true,true,a,{-1,0,0}}:Flat(a,b,e);};
    g.Start("goal");assert(Run(g,s,now,wall).status==Status::NoRoute);Cause(g,"frontier_empty");
    auto d=g.Diagnostics();assert(d.searches==1&&d.restarts==0&&d.stats.edges==4&&d.stats.blocked==4);
    assert(Is(d.restartCause,"initial")&&d.ground.body.z==40&&d.ground.headMargin==4);
    assert(d.player.z==44&&d.target.x==300);
    Tick(g,s,now,wall);assert(g.Diagnostics().stats.edges==4);Cause(g,"frontier_empty");
    s.active=false;Tick(g,s,now);Cause(g,"bad_context");assert(g.Diagnostics().stats.blocked==4);
    assert(g.Diagnostics().ground.status==Clearance::Invalid);
    s.active=true;g.Start("goal");assert(g.Diagnostics().searches==0&&g.Diagnostics().stats.edges==0);
    Tick(g,s,now,[](Vec3,Vec3,Vec3){return TraceSample{};});Cause(g,"ground");
    assert(g.Diagnostics().ground.status==Clearance::Unknown);
    g.Start("goal");Tick(g,s,now,[](Vec3 a,Vec3 b,Vec3 e){return e.x?TraceSample{}:Flat(a,b,e);});
    Cause(g,"query_failure");assert(g.Diagnostics().stats.unsupported>0);
    g.Start("goal");Run(g,s,now,[](Vec3 a,Vec3 b,Vec3 e) {
        return e.x?TraceSample{true,true,a,{0,0,1}}:Flat(a,b,e);
    });Cause(g,"exhausted_unknown");assert(g.Diagnostics().stats.unsupported==4);
    s=Sample(30000);g.Start("goal");Run(g,s,now);Cause(g,"node_budget");
    assert(g.Diagnostics().stats.nodes==1024&&g.Diagnostics().stats.traversable==1024);
    s=Sample(300);g.Start("goal");Tick(g,s,now);const auto before=g.Diagnostics().stats.edges;
    now+=15001;Tick(g,s,now);Cause(g,"wall_timeout");
    assert(g.Diagnostics().stats.edges==before&&g.Diagnostics().searchAge>=15001);
    s=Sample(0);g.Start("goal");Tick(g,s,now);Cause(g,"found");
    assert(g.Diagnostics().stats.nodes==1);Run(g,s,now);Cause(g,"found");
    s.player.x=12;s.targets[0].position.x=4;Tick(g,s,now);
    assert(g.Diagnostics().player.x==12&&g.Diagnostics().target.x==4);
    g.Tick(s,{1,2,3},now-1,Flat);Cause(g,"backward_clock");assert(g.Diagnostics().stats.nodes==1);
    s=Sample(300);s.targets[0].position.z=200;g.Start("goal");Tick(g,s,now);Cause(g,"target_height");
    s.targets[0].kind=lis::world::Kind::Exit;g.Start("goal");Tick(g,s,now);Cause(g,"exit_height");
    s=Sample(20);g.Start("goal");Tick(g,s,now);
    Tick(g,s,now,[&](Vec3 a,Vec3 b,Vec3 e) {
        if(!e.x&&a.x!=s.player.x)return TraceSample{true,false};return Flat(a,b,e);
    });Cause(g,"geometry_failure");assert(g.Diagnostics().stats.nodes==1);
    // Each restart condition retains a request-level count despite planner reset.
    s=Sample(300);g.Start("goal");Run(g,s,now);s.player.y=50;Tick(g,s,now);
    assert(Is(g.Diagnostics().restartCause,"off_route")&&g.Diagnostics().searches==2);
    Run(g,s,now);Tick(g,s,now,wall);assert(Is(g.Diagnostics().restartCause,"obstruction"));
    s=Sample(700);g.Start("goal");Tick(g,s,now);s.player.x=200;Tick(g,s,now);
    assert(Is(g.Diagnostics().restartCause,"player_jump"));
    s.targets[0].position.x+=20;Tick(g,s,now);assert(Is(g.Diagnostics().restartCause,"target"));
    s.radius=21;Tick(g,s,now);assert(Is(g.Diagnostics().restartCause,"body"));
    g.Tick(s,{1,2,4},++now,Flat);assert(Is(g.Diagnostics().restartCause,"identity"));
    s=Sample(700);g.Start("goal");Tick(g,s,now);
    for(int i=0;i<12;++i) {s.player.y+=25;if(Tick(g,s,now).status==Status::Limit)break;}
    Cause(g,"restart_limit");d=g.Diagnostics();
    assert(Is(d.restartCause,"planning_movement")&&d.restarts==8&&d.searches==8&&d.stats.nodes>0);
    s=Sample(60);s.radius=10;s.targets[0].kind=lis::world::Kind::Object;g.Start("goal");
    unsigned visionRays=0;
    const auto approach=Run(g,s,now,[&](Vec3 a,Vec3 b,Vec3 e) {
        if(e.x||b.z<a.z-60)return Flat(a,b,e);
        ++visionRays;
        return TraceSample{true,true,b,{-1,0,0}};
    });Cause(g,"found");d=g.Diagnostics();
    assert(approach.status==Status::Approach&&visionRays==0);
    assert(d.searches==1&&d.restarts==0&&Is(d.restartCause,"initial"));
    assert(d.stats.nodes>0);
    g.Reset();d=g.Diagnostics();assert(Is(d.cause,"none")&&Is(d.restartCause,"none"));
    assert(d.searches==0&&d.restarts==0&&d.searchAge==0&&d.stats.nodes==0);
    std::cout<<"PASS: exact planner stops, retained Guide diagnostics, restart causes and request bookkeeping\n";
}
