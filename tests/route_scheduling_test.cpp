#ifdef NDEBUG
#undef NDEBUG
#endif
#include "features/navigation/route_guide.h"
#include "fixtures/route_capture.h"
#include <cassert>
#include <iostream>
#include <set>
#include <thread>
using namespace lis::navigation::route;
namespace {
using Cell=std::pair<int,int>;
using Edge=std::pair<Cell,Cell>;
Cell Grid(routing::Vec3 p){return {int(std::lround(p.x/42)),int(std::lround(p.y/42))};}
struct RunResult {unsigned ticks=0;std::vector<Edge> probes,path;};
RunResult Replay(const route_fixture::Capture& capture,std::size_t work) {
    std::set<Edge> positive;
    for(std::size_t i=1;i<capture.witness.size();++i)positive.insert({capture.witness[i-1],capture.witness[i]});
    routing::Planner planner;routing::Config config;config.maxMilliseconds=10000;
    planner.Begin({0,0,0},{0,0,44},21,44,{{capture.targetX,capture.targetY,0},21,84,.01},config);
    RunResult out;
    while(planner.GetStatus()==routing::Status::Planning&&out.ticks<1000) {
        ++out.ticks;
        planner.Step([&](const routing::EdgeQuery& q) {
            const Edge edge{Grid(q.from),Grid(q.to)};out.probes.push_back(edge);
            return routing::EdgeResult{positive.count(edge)?routing::EdgeState::Traversable:routing::EdgeState::Unsupported,0};
        },work,10000); // Isolate operation scheduling from machine/clock speed.
    }
    assert(planner.GetStatus()==routing::Status::Found);
    const auto& path=planner.Path();
    for(std::size_t i=1;i<path.size();++i)out.path.push_back({Grid(path[i-1]),Grid(path[i])});
    return out;
}
lis::world::Snapshot Sample() {
    lis::world::Snapshot s;s.active=true;s.player={0,0,44};s.radius=21;s.halfHeight=44;
    lis::world::Target t;t.stableId="goal";t.position={800,0,44};t.approachRadius=100;
    s.targets.push_back(t);return s;
}
TraceSample Flat(lis::world::Vec3 a,lis::world::Vec3 b,lis::world::Vec3 extent) {
    if(!extent.x&&b.z<a.z-60)return {true,true,{a.x,a.y,0},{0,0,1}};
    return {true,false};
}
}
int main() {
    // Captured classroom witness paths, not a reconstruction of the garden.
    for(const auto* capture:{&route_fixture::original,&route_fixture::expanded}) {
        const auto old=Replay(*capture,8),now=Replay(*capture,PlanningWorkPerTick);
        assert(old.path==now.path&&old.probes==now.probes);
        assert(old.ticks>=4*now.ticks);
        std::cout<<"Captured witness: "<<old.ticks<<" -> "<<now.ticks<<" slices; identical probes/path\n";
    }
    const auto s=Sample();bool advanced=false;unsigned peak=0;
    // Repeated independent attempts tolerate an incidental OS preemption; an
    // eight-operation production slice can never satisfy this progress check.
    for(int attempt=0;attempt<8;++attempt) {
        Guide guide;guide.Start("goal");unsigned queries=0;
        guide.Tick(s,{1,2,3},1000,[&](auto a,auto b,auto e){++queries;return Flat(a,b,e);});
        assert(queries<=MaxPlanningTraceQueries);
        peak=std::max(peak,queries);advanced|=guide.Diagnostics().stats.edges>8;
    }
    assert(advanced);
    Guide slow;slow.Start("goal");unsigned bodies=0;
    const auto result=slow.Tick(s,{1,2,3},1000,[&](auto a,auto b,auto e) {
        if(e.x){++bodies;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        return Flat(a,b,e);
    });
    // The time slice still yields after one slow, non-preemptible edge callback.
    assert(result.status==Status::Planning&&bodies==1&&slow.Diagnostics().stats.edges==1);
    std::cout<<"PASS: production progress, query ceiling (peak "<<peak<<"), and unchanged time-slice yield\n";
}
