// Standalone x86 MSVC test: compile this TU only, /std:c++17 /EHsc /O2 /W4 /WX /I src.
// Including the implementation gives its clock a deterministic test-only seam.
// Do not also link guided_beacon.cpp, guided_scene.cpp, or route_guide.cpp.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "features/navigation/guided_beacon.h"
#include "features/navigation/guided_scene.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace lis {void Log(const std::string&) {}}

namespace fixture {
using namespace lis;
uint64_t now=1000;
DWORD owner=GetCurrentThreadId();
int captures=0,identities=0,queries=0,steps=0;
bool captureOk=true,throwStep=false;
world::Snapshot source,received;
navigation::route::Identity identity{11,22,33},receivedIdentity;
navigation::route::Status result=navigation::route::Status::Following;
std::function<void()> onCapture,onQuery;
std::string started;
uint64_t Clock(){return now;}
}
#define GetTickCount64 fixture::Clock
#include "../src/features/navigation/guided_beacon.cpp"
#undef GetTickCount64

namespace lis::navigation::guided_scene {
bool Thread() noexcept {return GetCurrentThreadId()==fixture::owner;}
route::Identity CurrentIdentity(){++fixture::identities;return fixture::identity;}
bool Capture(const world::Snapshot&,route::Identity expected,Frame& frame){
    ++fixture::captures;
    if(fixture::onCapture)fixture::onCapture();
    frame.snapshot=fixture::source;frame.identity=expected;frame.sampledAt=fixture::now;
    return fixture::captureOk;
}
route::TraceSample Trace(Frame&,world::Vec3,world::Vec3,world::Vec3){
    ++fixture::queries;if(fixture::onQuery)fixture::onQuery();return {true,false,{},{}};
}
}
namespace lis::navigation::route {
void Guide::Reset() {fixture::started.clear();}
void Guide::Start(const std::string& selected){fixture::started=selected;}
Result Guide::Tick(const world::Snapshot& snapshot,Identity identity,uint64_t now,const Trace& trace){
    ++fixture::steps;fixture::received=snapshot;fixture::receivedIdentity=identity;
    if(fixture::throwStep)throw std::runtime_error("fixture");
    if(now!=fixture::now)throw std::runtime_error("clock mismatch");
    trace(snapshot.player,{100,0,0},{snapshot.radius,snapshot.radius,snapshot.halfHeight});
    return {fixture::result,{100,0,0},true,false};
}
}
int main(){
    using namespace fixture;
    namespace g=lis::navigation::guidance;
    using Status=lis::navigation::route::Status;
    int errors=0;
    const auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS: ":"FAIL: ")<<label<<'\n';errors+=!ok;};
    source.active=true;source.player={12,34,56};source.cameraYaw=1;source.radius=20;source.halfHeight=40;
    for(int i=0;i<5;++i)g::Tick();
    check(captures==0&&identities==0&&queries==0,"idle ticks perform no native work");
    std::thread worker([&]{g::Request("worker");g::Read();g::Observe(source);g::Tick();g::Cancel();});worker.join();
    check(captures==0&&identities==0&&queries==0&&g::Read().route.status==Status::Off,"worker requests, reads, cancellation and wrong-thread ticks perform no native work");
    const auto begin=[&]{g::Observe(source);g::Request("first");};
    begin();g::Tick();
    auto view=g::Read();
    check(view.route.hasWaypoint&&view.selected=="first"&&started=="first","request starts and publishes guidance");
    check(received.player.x==12&&received.cameraYaw==1&&received.radius==20&&received.halfHeight==40&&receivedIdentity.pawn==22&&view.player.z==56&&view.radius==20,"core and publication receive fresh geometry and identity");
    const auto beforeIdle=captures;g::Tick();
    source.player.x+=10;g::Tick();
    check(captures==beforeIdle+2&&received.player.x==22,"idle and manual movement keep active guidance sampling without a movement lease");
    for(bool duringQuery:{false,true})for(int action=0;action<3;++action){
        begin();const auto old=g::Read().generation;
        auto change=[&,action]{if(action==0)g::Cancel();else g::Request(action==1?"second":"first");};
        if(duringQuery)onQuery=change;else onCapture=change;
        g::Tick();onQuery={};onCapture={};view=g::Read();
        check(view.generation>old&&!view.route.hasWaypoint&&view.sampledAt==0&&view.route.status==(action==0?Status::Off:Status::Planning),"capture/query cancellation, selection and same-selection generation suppress old publication");
        if(action!=0){g::Tick();check(g::Read().route.hasWaypoint,"replacement request runs on next tick");}
    }
    begin();onCapture=[]{g::Tick();};const auto recursion=captures;g::Tick();onCapture={};
    check(captures==recursion+1,"capture reentrancy cannot enter another native tick");
    begin();now+=450;const auto edge=captures;g::Tick();
    check(captures==edge+1,"scanner sample accepted at freshness boundary");
    begin();now+=451;const auto stale=captures;g::Tick();
    check(captures==stale&&g::Read().route.status==Status::Unavailable&&!g::Read().route.hasWaypoint,"stale scanner suppresses capture and waypoint");
    begin();--now;g::Tick();check(captures==stale,"clock reversal suppresses capture");++now;
    captureOk=false;begin();g::Tick();captureOk=true;
    check(g::Read().route.status==Status::Unavailable&&!g::Read().route.hasWaypoint,"capture rejection removes previous waypoint");
    throwStep=true;begin();g::Tick();throwStep=false;
    check(g::Read().route.status==Status::Unavailable,"core exception becomes unavailable");
    for(Status terminal:{Status::Approach,Status::Arrived,Status::NoRoute,Status::Unsupported,Status::Limit,Status::Unavailable}){
        result=terminal;begin();g::Tick();const auto count=captures;const auto stamp=g::Read().sampledAt;
        now+=1000;g::Observe(source);g::Tick();g::Tick();
        check(captures==count&&g::Read().sampledAt==stamp&&g::Read().route.status==terminal,"terminal result is stable without repeated capture");
        g::Request("first");g::Tick();check(captures==count+1,"new generation restarts terminal guidance");
    }
    g::Shutdown();const auto stopped=captures;g::Tick();g::Cancel();
    check(captures==stopped&&g::Read().route.status==Status::Off,"shutdown and repeated cancel stay idle");
    return errors?1:0;
}
