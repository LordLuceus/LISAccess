// Standalone x86 MSVC test: compile this TU only, /std:c++17 /EHsc /O2 /W4 /WX /I src.
// Owned memory and narrow native stubs; no process, registry or game access.
// Do not also link guided_scene.cpp, player.cpp, collision.cpp or engine sources.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "features/navigation/guided_scene.h"
#include "features/navigation/collision.h"
#include "features/world/session.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>

namespace fixture {
using namespace lis;
world::Session session;
struct Pawn {unsigned char padding[0x54]{};world::Vec3 position;};
Pawn pawn,otherPawn;
struct Controller {uintptr_t pawn=0;} controller;
uint64_t now=1000;
int gates=0,copies=0,offsets=0,queries=0;
bool foreground=true,gateOk=true,copyOk=true,queryOk=true,raiseQuery=false,hit=false;
bool discoveryRequested=false;
world::Snapshot fresh;
world::Vec3 lastFrom,lastTo,lastExtent;
uint64_t Clock(){return now;}
bool Equal(world::Vec3 a,world::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
}
#define GetTickCount64 fixture::Clock
#include "../src/features/navigation/guided_scene.cpp"
#undef GetTickCount64

namespace lis {bool IsGameForeground(){return fixture::foreground;}}
namespace lis::engine {
bool Reflection::Offset(const char* owner,const char* name,const char* type,int size,int& offset)const{
    ++fixture::offsets;offset=0;
    return std::strcmp(owner,"Controller")==0&&std::strcmp(name,"Pawn")==0&&std::strcmp(type,"ObjectProperty")==0&&size==sizeof(uintptr_t);
}
bool Copy(void* destination,uintptr_t source,size_t bytes){
    ++fixture::copies;if(!fixture::copyOk)return false;
    if((source==reinterpret_cast<uintptr_t>(&fixture::controller)&&bytes==sizeof(uintptr_t))||
       (source==reinterpret_cast<uintptr_t>(&fixture::pawn.position)&&bytes==sizeof(world::Vec3))){
        std::memcpy(destination,reinterpret_cast<const void*>(source),bytes);return true;
    }
    return false;
}
}
namespace lis::world {
Session& GameSession(){return fixture::session;}
bool GatePlayer(Session&,Snapshot& snapshot,uintptr_t& pawn,bool refreshDiscovery){
    ++fixture::gates;fixture::discoveryRequested|=refreshDiscovery;
    snapshot=fixture::fresh;pawn=fixture::controller.pawn;return fixture::gateOk;
}
bool Finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 Sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float Length(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
bool Query(const engine::ObjectRegistry&,uintptr_t,uintptr_t,uintptr_t,Vec3 from,Vec3 to,Vec3 extent,TraceParams& result){
    ++fixture::queries;fixture::lastFrom=from;fixture::lastTo=to;fixture::lastExtent=extent;
    if(fixture::raiseQuery)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    result.actor=fixture::hit?123:0;result.location={1,2,3};result.normal={0,0,1};return fixture::queryOk;
}
}
int main(){
    using namespace fixture;
    namespace scene=lis::navigation::guided_scene;
    using lis::navigation::route::Identity;
    int errors=0;
    const auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS: ":"FAIL: ")<<label<<'\n';errors+=!ok;};
    session.thread=GetCurrentThreadId();session.ready=true;session.base=1;session.trace=2;
    controller.pawn=reinterpret_cast<uintptr_t>(&pawn);
    session.liveController=reinterpret_cast<uintptr_t>(&controller);session.liveWorld=3;
    const Identity identity{session.liveController,controller.pawn,session.liveWorld};
    fresh.active=true;fresh.player={10,20,30};fresh.radius=20;fresh.halfHeight=40;fresh.cameraYaw=1;
    pawn.position=fresh.player;
    auto cached=fresh;cached.focusedId="focus";cached.focusedLabel=L"Focus";cached.actions[0].label=L"Use";
    scene::Frame frame;
    check(scene::Capture(cached,identity,frame)&&!discoveryRequested,"capture reuses discovery metadata");
    check(frame.snapshot.focusedId=="focus"&&frame.snapshot.actions[0].label==L"Use","unchanged position and yaw retain scanner focus");
    fresh.player.x+=1;fresh.cameraYaw+=1;
    check(scene::Capture(cached,identity,frame)&&Equal(frame.snapshot.player,fresh.player)&&frame.snapshot.cameraYaw==fresh.cameraYaw&&frame.snapshot.radius==fresh.radius&&frame.snapshot.halfHeight==fresh.halfHeight,"capture provides actual fresh player geometry");
    check(frame.snapshot.focusedId.empty()&&frame.snapshot.focusedLabel.empty()&&frame.snapshot.actions[0].label.empty(),"walking and aiming discard cached focus and actions");
    fresh.player=cached.player;
    check(scene::Capture(cached,identity,frame)&&frame.snapshot.focusedId.empty(),"yaw change alone invalidates focus");
    fresh.cameraYaw=cached.cameraYaw;fresh.player.x+=1;
    check(scene::Capture(cached,identity,frame)&&frame.snapshot.focusedId.empty(),"position change alone invalidates focus");
    fresh=cached;
    const auto rejected=[&](const char* label){frame.snapshot=cached;frame.sampledAt=now;check(!scene::Capture(cached,identity,frame)&&frame.sampledAt==0&&!frame.snapshot.active,label);};
    cached.active=false;rejected("inactive scanner rejected");cached.active=true;
    cached.interactionBusy=true;rejected("interaction scanner rejected");cached.interactionBusy=false;
    foreground=false;rejected("background rejected");foreground=true;
    gateOk=false;rejected("fresh player gate rejects menu or disabled traversal context");gateOk=true;
    session.liveWorld=4;rejected("world replacement rejected");session.liveWorld=3;
    controller.pawn=reinterpret_cast<uintptr_t>(&otherPawn);rejected("pawn replacement rejected");controller.pawn=identity.pawn;
    session.liveController=4;rejected("controller replacement rejected");session.liveController=identity.controller;
    fresh.radius+=1;rejected("radius change rejected");fresh.radius=cached.radius;
    fresh.halfHeight+=1;rejected("height change rejected");fresh.halfHeight=cached.halfHeight;
    fresh.player.x=std::numeric_limits<float>::quiet_NaN();rejected("nonfinite player rejected");fresh.player=cached.player;
    session.ready=false;rejected("unready session rejected");session.ready=true;
    session.faulted=true;rejected("faulted world rejected");session.faulted=false;
    const auto capture=[&]{check(scene::Capture(cached,identity,frame),"fresh frame captured");};
    const world::Vec3 from{10,20,30},to{100,20,30},extent{20,20,40};
    const auto trace=[&]{return scene::Trace(frame,from,to,extent);};
    capture();auto sample=trace();
    check(sample.known&&!sample.hit&&Equal(lastFrom,from)&&Equal(lastTo,to)&&Equal(lastExtent,extent),"trace forwards geometry and represents a known no-hit");
    hit=true;sample=trace();hit=false;
    check(sample.known&&sample.hit&&sample.position.z==3&&sample.normal.z==1,"trace preserves hit position and normal");
    const auto denied=[&](const char* label){const auto n=queries;check(!trace().known&&queries==n,label);};
    capture();frame.queries=255;check(trace().known&&frame.queries==256,"last budgeted query allowed");denied("query budget exhausted");
    capture();now+=100;check(trace().known,"query accepted at freshness boundary");++now;denied("expired frame denied");
    capture();--now;denied("future timestamp denied");++now;
    capture();frame.sampledAt=0;denied("missing timestamp denied");
    capture();pawn.position.x+=1;denied("player movement since capture denied");pawn.position=cached.player;
    capture();controller.pawn=reinterpret_cast<uintptr_t>(&otherPawn);denied("pawn change before query denied");controller.pawn=identity.pawn;
    capture();session.liveWorld=4;denied("world change before query denied");session.liveWorld=3;
    capture();copyOk=false;denied("unreadable player denied");copyOk=true;
    capture();const auto beforeInvalid=queries;
    check(!scene::Trace(frame,from,to,{-1,0,0}).known&&!scene::Trace(frame,{std::numeric_limits<float>::infinity(),0,0},to,extent).known&&queries==beforeInvalid,"invalid trace geometry denied");
    queryOk=false;check(!trace().known,"native query failure is unknown");queryOk=true;check(trace().known,"ordinary query failure does not latch fault");
    const auto beforeNative=copies+offsets+gates+queries;
    bool threadDenied=false;
    std::thread worker([&]{scene::Frame other;threadDenied=!scene::Thread()&&!scene::CurrentIdentity().pawn&&!scene::Capture(cached,identity,other)&&!trace().known;});worker.join();
    check(threadDenied&&copies+offsets+gates+queries==beforeNative,"wrong thread cannot read native identity, capture or query");
    // Fault latch is process-lifetime state: run this case last.
    capture();raiseQuery=true;const auto beforeFault=queries;sample=trace();raiseQuery=false;
    check(!sample.known&&queries==beforeFault+1,"native Query SEH is contained");
    const auto afterFault=copies+offsets+gates+queries;
    check(!trace().known&&!scene::Capture(cached,identity,frame)&&copies+offsets+gates+queries==afterFault,"guidance fault latches and blocks further native work");
    check(!session.faulted&&!session.interactionFaulted&&!session.faceFaulted&&!session.exitFaulted&&session.ready,"guidance fault leaves other world features enabled");
    return errors?1:0;
}
