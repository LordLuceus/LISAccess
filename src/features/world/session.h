#pragma once
// Private game-thread lifetime state. Public feature APIs do not expose this session.
#include "features/world/world.h"
#include "engine/reflection.h"
#include "engine/localization.h"
#include "engine/abi.h"
#include <Windows.h>
namespace lis::world {
inline constexpr float Pi=3.14159265358979323846f;
struct Session {
    uintptr_t base=0,trace=0,localize=0,liveController=0,liveWorld=0,liveFocus=0,faceFunction=0;
    DWORD thread=0;
    bool faulted=false,ready=false,interactionFaulted=false,faceFaulted=false,exitFaulted=false;
    int cursor=0;
    engine::Reflection reflection;
    engine::Localization localization;
    std::vector<uintptr_t> controllers,menus,destinations;
    std::vector<uintptr_t> exitAnnouncements,pendingExitAnnouncements;
    std::vector<Landmark> landmarks;
    std::string lastState;
};
Session& GameSession();
bool EqualPath(const std::string& a,const std::string& b);
bool Discover(Session& session);
// Normal sampling owns incremental discovery. Fast guidance refreshes may reuse
// completed metadata without running another registry scan.
bool GatePlayer(Session& session,Snapshot& snapshot,uintptr_t& pawn,bool refreshDiscovery=true);
bool SafeSample(Session& session,Snapshot& snapshot);
inline bool ValidArray(const engine::Array& a,int maximum) {return a.count>=0&&a.count<=maximum&&a.capacity>=a.count&&(!a.count||a.data>=0x10000);}
}
