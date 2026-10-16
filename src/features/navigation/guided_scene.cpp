#include "features/navigation/guided_scene.h"
#include "features/navigation/collision.h"
#include "features/world/session.h"
#include "platform/foreground.h"
#include <Windows.h>

namespace lis::navigation::guided_scene {
namespace {
bool faulted=false;
bool Same(route::Identity a,route::Identity b) {
    return a.controller==b.controller&&a.pawn==b.pawn&&a.world==b.world;
}
bool CaptureImpl(const world::Snapshot& cached,route::Identity expected,Frame& out) {
    auto& s=world::GameSession();
    if(!Thread()||faulted||s.faulted||!s.ready||!s.base||!s.trace||!IsGameForeground()||
       !cached.active||cached.interactionBusy||!expected.controller||!expected.pawn||!expected.world)return false;
    world::Snapshot fresh;uintptr_t pawn=0;
    if(!world::GatePlayer(s,fresh,pawn,false))return false;
    const route::Identity actual{s.liveController,pawn,s.liveWorld};
    if(!Same(actual,expected)||!world::Finite(fresh.player)||fresh.radius!=cached.radius||fresh.halfHeight!=cached.halfHeight)return false;
    out.snapshot=cached;
    out.snapshot.player=fresh.player;out.snapshot.cameraYaw=fresh.cameraYaw;
    // Do not carry a previously focused action through new walking or aiming.
    // The normal scanner will supply fresh focus on its next sample.
    if(world::Length(world::Sub(cached.player,fresh.player))>.1f||cached.cameraYaw!=fresh.cameraYaw) {
        out.snapshot.focusedId.clear();out.snapshot.focusedLabel.clear();out.snapshot.actions={};
    }
    out.identity=actual;out.sampledAt=GetTickCount64();return true;
}
bool SafeCapture(const world::Snapshot& cached,route::Identity expected,Frame& out) {
    __try {return CaptureImpl(cached,expected,out);}
    __except(EXCEPTION_EXECUTE_HANDLER){faulted=true;return false;}
}
route::TraceSample Query(Frame& frame,world::Vec3 from,world::Vec3 to,world::Vec3 extent) {
    auto& s=world::GameSession();const auto now=GetTickCount64();
    if(!Thread()||faulted||s.faulted||!s.ready)return {};
    if(!frame.sampledAt||now<frame.sampledAt||now-frame.sampledAt>100)return {};
    if(++frame.queries>MaxQueriesPerFrame)return {};
    if(!Same(CurrentIdentity(),frame.identity))return {};
    if(!world::Finite(from)||!world::Finite(to)||!world::Finite(extent)||extent.x<0||extent.y<0||extent.z<0)return {};
    world::Vec3 position{};
    if(!engine::Copy(&position,frame.identity.pawn+0x54,sizeof(position))||
       position.x!=frame.snapshot.player.x||position.y!=frame.snapshot.player.y||position.z!=frame.snapshot.player.z)return {};
    world::TraceParams result{};
    const bool valid=world::Query(s.reflection.registry,s.base,s.trace,frame.identity.pawn,from,to,extent,result);
    if(!valid)return {};
    // Geometry's positive floor calibration is mandatory before any no-hit is
    // interpreted as traversable. Trace alone never certifies a route.
    return {true,result.actor!=0,result.location,result.normal};
}
route::TraceSample SafeQuery(Frame& frame,world::Vec3 from,world::Vec3 to,world::Vec3 extent) {
    __try {return Query(frame,from,to,extent);}
    __except(EXCEPTION_EXECUTE_HANDLER){faulted=true;return {};}
}
}
bool Thread() noexcept {
    const auto& s=world::GameSession();return s.thread&&s.thread==GetCurrentThreadId();
}
route::Identity CurrentIdentity() {
    if(!Thread())return {};
    auto& s=world::GameSession();uintptr_t pawn=0;
    if(s.faulted||!s.ready||!s.liveController||!s.liveWorld||
       !s.reflection.Value(s.liveController,"Controller","Pawn","ObjectProperty",pawn))return {};
    return {s.liveController,pawn,s.liveWorld};
}
bool Capture(const world::Snapshot& cached,route::Identity expected,Frame& out) {
    out={};if(!Thread()||faulted)return false;
    try {return SafeCapture(cached,expected,out);}catch(...){faulted=true;return false;}
}
route::TraceSample Trace(Frame& frame,world::Vec3 from,world::Vec3 to,world::Vec3 extent) {
    if(!Thread()||faulted)return {};
    try {return SafeQuery(frame,from,to,extent);}catch(...){faulted=true;return {};}
}
}
