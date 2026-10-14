#include "features/world/session.h"
#include "features/exits/exits.h"
#include "platform/log.h"
#include "platform/foreground.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include "features/navigation/collision.h"
#include <algorithm>
#include <cmath>
#include <cctype>
namespace lis::world {
using engine::Copy;
using engine::Read;
using engine::Array;
bool EqualPath(const std::string& a, const std::string& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](unsigned char x,unsigned char y){return std::tolower(x)==std::tolower(y);});
}
void Add(std::vector<uintptr_t>& v, uintptr_t p) {
    if (v.size() < 256 && std::find(v.begin(), v.end(), p) == v.end()) v.push_back(p);
}
bool Discover(Session& session) {
    Array a{};
    if (!session.reflection.registry.Registry(a) || session.reflection.registry.Name(0) != "None" || session.reflection.registry.Name(2) != "IntProperty") return false;
    if(session.cursor==0)session.pendingExitAnnouncements.clear();
    // Hard per-tick discovery bound. Initial sampling waits for a complete registry pass.
    int end = (std::min)(a.count, session.cursor + 8192);
    for (; session.cursor < end; ++session.cursor) {
        uintptr_t p = Read<uintptr_t>(a.data + session.cursor * 4);
        if (!session.reflection.registry.Registered(p)) continue;
        std::string cls = session.reflection.registry.ClassOf(p), name = session.reflection.registry.NameOf(p);
        session.reflection.Observe(p,cls,name);
        if (cls == "Function" && name == "Trace" && session.reflection.registry.NameOf(Read<uintptr_t>(p + 0x28)) == "Actor") session.trace = p;
        if (cls == "Function" && name == "Localize" && session.reflection.registry.NameOf(Read<uintptr_t>(p + 0x28)) == "Object") session.localize = p;
        if (cls == "Function" && name == "SetNextCameraRotationForced" && session.reflection.registry.NameOf(Read<uintptr_t>(p + 0x28)) == "CBA_3RD_Basic") session.faceFunction = p;
        if (!session.reflection.registry.Instance(p)) continue;
        if(cls=="IA_GFxUIControlAnnouncement"&&!session.exitFaulted&&exits::IsSource(session,p)) {
            if(session.pendingExitAnnouncements.size()<4096)session.pendingExitAnnouncements.push_back(p);
            else {session.exitFaulted=true;Log("event=exit_scan_fault reason=discovery_bound interactions=preserved");}
        }
        if (cls == "WhatIfController") Add(session.controllers, p);
        if (cls == "PauseMenu" || cls == "OutGameSensenMenu" || cls=="SensenMenu") Add(session.menus, p);
        std::string shortName=name;
        const int number=Read<int>(p+0x30);
        if(number>0) shortName+="_"+std::to_string(number-1);
        for(const auto& landmark:session.landmarks) {
            const auto last=landmark.actorPath.find_last_of('.');
            const auto targetName=last==std::string::npos ? landmark.actorPath : landmark.actorPath.substr(last+1);
            if(EqualPath(shortName,targetName) && session.reflection.registry.IsA(p,"Actor") && EqualPath(session.reflection.registry.Path(p),landmark.actorPath)) Add(session.destinations,p);
        }
    }
    if (session.cursor == a.count) {
        session.exitAnnouncements.swap(session.pendingExitAnnouncements);
        session.cursor = 0; session.ready = true;
    }
    return session.ready;
}
}
