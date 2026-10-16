// Ported from the shelved autowalk pure geometry/planner regression; no native dependencies.
#include "features/navigation/route_routing.h"
#include <cstdlib>
#include <iostream>
using namespace lis::navigation::route;
void check(bool yes,const char* what){if(!yes){std::cerr<<what<<'\n';std::exit(1);}}
int main() {
    unsigned calls=0;
    auto physics=[&](lis::world::Vec3 from,lis::world::Vec3 to,lis::world::Vec3 extent) {
        ++calls;
        if(extent.x) {
            check(from.z==45.75f&&to.z==45.75f&&extent.z==44.75f,"normalized floor graph preserves actual pawn head clearance");
            if(from.y==0&&to.y==0&&((from.x<21&&to.x>21)||(from.x>21&&to.x<21)))
                return TraceSample{true,true,{21,0,44},{-1,0,0}};
            return TraceSample{true,false};
        }
        return TraceSample{true,true,{from.x,from.y,0},{0,0,1}};
    };
    routing::Planner p;routing::Config config;config.maxCells=4;config.flatTolerance=1;
    const auto ground=CalibrateGround(lis::world::Vec3{0,0,46.5f},21,44,physics);
    check(ground.status==Clearance::Clear&&ground.body.z==44&&ground.headMargin==2.5f,"positive floor calibration retains actor/body offset");
    check(BeginOnGround(p,ground,{{84,0,0},0,1,1},config)==routing::Status::Planning,"route requires work");
    for(unsigned i=0;i<100&&p.GetStatus()==routing::Status::Planning;++i)
        p.Step([&](const routing::EdgeQuery& q){return ProbeFloorEdge(q,ground,physics);},8,10);
    check(p.GetStatus()==routing::Status::Found,"find alternate route around blocked edge");
    check(p.Path().size()==5,"detour uses verified orthogonal edges");
    check(calls>0,"collision callback actually used");
    for(const auto& point:p.Path())check(point.z==0,"body height never leaks into floor path");
    auto unknown=[](lis::world::Vec3,lis::world::Vec3,lis::world::Vec3){return TraceSample{};};
    const auto failedGround=CalibrateGround(lis::world::Vec3{0,0,46.5f},21,44,unknown);
    check(BeginOnGround(p,failedGround,{{84,0,0},0,1,1})==routing::Status::Invalid&&p.Path().empty(),"failed calibration cannot retain an old path");
    check(ProbeFloorEdge(routing::EdgeQuery{{0,0,0},{42,0,0},21,44},ground,unknown).state==routing::EdgeState::Unsupported,"unknown not blocked");
    check(ProbeFloorEdge(routing::EdgeQuery{{1e30,0,0},{1e30,42,0},21,44},ground,physics).state==routing::EdgeState::Unsupported,"unrepresentable native spacing rejected");
    std::cout<<"PASS: planner/clearance adapter (mock physics only)\n";
}
