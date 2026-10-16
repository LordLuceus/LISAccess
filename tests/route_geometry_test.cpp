// Pure geometry assertions ported from autowalk_safety_test.cpp; ownership tests excluded.
#include "features/navigation/route_geometry.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace lis::navigation::route;
void check(bool yes,const char* message){if(!yes){std::cerr<<message<<'\n';std::exit(1);}}
int main() {
    using lis::world::Vec3;
    unsigned calls=0;
    auto clear=[&](Vec3 a,Vec3 b,Vec3 extent){++calls;
        if(extent.x>0){check(extent.x==21&&extent.z==43.5f&&a.z==44.5f,"full-body footprint/head clearance");return TraceSample{true,false};}
        return TraceSample{true,true,{b.x,b.y,0},{0,0,1}};
    };
    auto result=CheckFlatSegment(Vec3{0,0,44},Vec3{42,0,44},21,44,clear);
    check(result.status==Clearance::Clear&&calls==3&&result.end.z==44,"body and both support probes required");
    calls=0;result=CheckFlatSegment(Vec3{0,0,44},Vec3{43,0,44},21,44,clear);
    check(result.status==Clearance::Invalid&&calls==0,"overlong unchecked segment refused");
    auto blocked=[](Vec3,Vec3,Vec3){return TraceSample{true,true,{1,0,44},{-1,0,0}};};
    check(CheckFlatSegment(Vec3{0,0,44},Vec3{42,0,44},21,44,blocked).status==Clearance::Blocked,"wall blocks route");
    auto hole=[](Vec3,Vec3,Vec3){return TraceSample{true,false};};
    check(CheckFlatSegment(Vec3{0,0,44},Vec3{42,0,44},21,44,hole).status==Clearance::Unsupported,"no floor is not clear");
    auto unknown=[](Vec3,Vec3,Vec3){return TraceSample{};};
    check(CheckFlatSegment(Vec3{0,0,44},Vec3{42,0,44},21,44,unknown).status==Clearance::Unknown,"failed query distinct from obstacle");
    auto gap=[&](Vec3 a,Vec3 b,Vec3 e){if(!e.x&&b.x==21)return TraceSample{true,false};return clear(a,b,e);};
    check(CheckFlatSegment(Vec3{0,0,44},Vec3{42,0,44},21,44,gap).status==Clearance::Unsupported,"midpoint gap rejected");
    auto step=[&](Vec3 a,Vec3 b,Vec3 e){auto h=clear(a,b,e);if(!e.x)h.position.z=2;return h;};
    check(CheckFlatSegment(Vec3{0,0,44},Vec3{42,0,44},21,44,step).status==Clearance::Unsupported,"steps not silently treated as flat");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    check(CheckFlatSegment(Vec3{nan,0,44},Vec3{42,0,44},21,44,clear).status==Clearance::Invalid,"NaN rejected");
    std::cout<<"PASS: pure swept body and floor geometry\n";
}
