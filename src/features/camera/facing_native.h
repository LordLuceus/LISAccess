#pragma once
#include "features/camera/facing_policy.h"
#include "features/camera/facing_read.h"
#include "features/world/session.h"
#include "features/world/menu_state.h"
namespace lis::world::facing_native {
using facing_policy::Rotation;
using facing_policy::Identity;
struct NativeFrame : facing_policy::Context {
    facing_policy::Frame frame;
    const wchar_t* error=L"Camera mode unavailable.";
};
struct ReadFault {};
// Failed copies must not silently become zero/false native state.
template<class T> T NativeRead(uintptr_t address) {
    T v{};if(!facing_read::Span(address,0,sizeof(v))||!engine::Copy(&v,address,sizeof(v)))throw ReadFault{};return v;
}
template<class T> bool Value(const engine::Reflection& f,uintptr_t object,const char* owner,const char* name,const char* type,T& out) {
    return facing_read::DescriptorNativeRead(f,[](void* destination,uintptr_t source,size_t bytes) {
        if(!engine::Copy(destination,source,bytes))throw ReadFault{};
        return true;
    },object,owner,name,type,out);
}
inline bool Bool(const engine::Reflection& f,uintptr_t object,const char* owner,const char* name,bool& out) {
    auto field=f.Field(owner,name,"BoolProperty",4);if(!field)return false;
    auto mask=NativeRead<uint32_t>(field+0x80);if(!mask)return false;
    out=(NativeRead<uint32_t>(object+NativeRead<int>(field+0x60))&mask)!=0;return true;
}
inline bool Identify(const engine::Reflection& f,uintptr_t p,const char* type,Identity& out) {
    if(!f.registry.Instance(p)||!f.registry.IsA(p,type))return false;
    out={p,NativeRead<uintptr_t>(p+0x34),NativeRead<int>(p+0x20),NativeRead<int>(p+0x2c),NativeRead<int>(p+0x30)};return true;
}
inline bool Actor(Session& s,uintptr_t p,const std::string& id,Identity& out) {
    bool deleted=true;
    return Identify(s.reflection,p,"Actor",out)&&NativeRead<uintptr_t>(p+0xd8)==s.liveWorld&&
        Bool(s.reflection,p,"Actor","bDeleteMe",deleted)&&!deleted&&s.reflection.registry.Path(p)==id;
}
inline bool Resolve(Session& s,uintptr_t component,const std::string& id,Identity& out) {
    engine::Array objects{};
    if(!Value(s.reflection,component,"DNEWhatIfInteractionComponent","RegisteredActors","ArrayProperty",objects)||!ValidArray(objects,2048))return false;
    for(int i=0;i<objects.count;++i) {
        Identity candidate;
        if(!Actor(s,NativeRead<uintptr_t>(objects.data+i*4),id,candidate))continue;
        if(out.address)return false;
        out=candidate;
    }
    return out.address!=0;
}
inline bool Probe(Session& s,const std::string& id,const Identity& target,NativeFrame& out,bool needsActor) {
    auto& f=s.reflection;auto& v=out.frame;
    uintptr_t pawn=0,back=0,exPawn=0,camera=0,player=0,settings=0,pauser=0,component=0,current=0;
    unsigned char gameplay=0;
    if(!Identify(f,s.liveController,"WhatIfController",out.controller)||!Identify(f,s.liveWorld,"WorldInfo",out.world)||
        !Value(f,s.liveController,"Controller","Pawn","ObjectProperty",pawn)||!Identify(f,pawn,"Pawn",out.pawn)||
        !Value(f,pawn,"Pawn","Controller","ObjectProperty",back)||back!=s.liveController||
        !Value(f,s.liveController,"WhatIfController","ExPawn","ObjectProperty",exPawn)||exPawn!=pawn||
        NativeRead<uintptr_t>(pawn+0xd8)!=s.liveWorld||NativeRead<uintptr_t>(s.liveController+0xd8)!=s.liveWorld)return false;
    for(auto menu:s.menus)if(BlockingMenu(f,menu))return false;
    if(!Value(f,s.liveController,"WhatIfController","CurrentGameplayType","ByteProperty",gameplay)||gameplay!=1||
        !Value(f,s.liveWorld,"WorldInfo","Pauser","ObjectProperty",pauser)||pauser)return false;
    for(const char* flag:{"bGameplayPause","PlayerControlDisabled","bMenuPause","bFocusLostPause"}) {
        bool set=true;if(!Bool(f,s.liveController,"WhatIfController",flag,set)||set)return false;
    }
    if(needsActor) {
        if(!Value(f,s.liveController,"WhatIfController","DNEWhatIfInteractionComp","ComponentProperty",component)||
            !f.registry.Instance(component)||!f.registry.IsA(component,"DNEWhatIfInteractionComponent")||
            NativeRead<uintptr_t>(component+0x28)!=s.liveController||
            !Value(f,component,"DNEWhatIfInteractionComponent","CurrentInteraction","ObjectProperty",current)||current)return false;
        if(target.address) {if(!Actor(s,target.address,id,out.target)||!(out.target==target))return false;}
        else if(!Resolve(s,component,id,out.target))return false;
        if(!Value(f,out.target.address,"Actor","Location","StructProperty",v.target)||!facing_policy::Finite(v.target))return false;
    }
    if(!Value(f,pawn,"Actor","Location","StructProperty",v.player)||!facing_policy::Finite(v.player))return false;
    if(!Value(f,s.liveController,"WhatIfController","ExCamera","ObjectProperty",camera)||!Identify(f,camera,"WhatIfCamera",out.camera)||
        !Value(f,s.liveController,"PlayerController","PlayerCamera","ObjectProperty",player)||!Identify(f,player,"Camera",out.playerCamera))return false;
    if(camera!=player){out.error=L"Facing is unavailable in this camera mode.";return false;}
    engine::Array behaviours{};
    if(!Value(f,camera,"WhatIfCamera","GameplayCameraBehaviours","ArrayProperty",behaviours)||!ValidArray(behaviours,32))return false;
    out.error=L"Facing is unavailable in this camera mode.";
    for(int i=0;i<behaviours.count;++i) {
        auto p=NativeRead<uintptr_t>(behaviours.data+i*4);Identity identity;bool active=false;
        if(!Identify(f,p,"CameraBehaviour",identity)||!Bool(f,p,"CameraBehaviour","bActive",active))return false;
        if(!active)continue;
        if(out.behaviour.address||f.registry.ClassOf(p)!="CBA_3RD_Basic")return false;
        out.behaviour=identity;
    }
    out.error=L"Camera limits could not be read.";
    auto b=out.behaviour.address;
    if(!b||!Bool(f,b,"CBA_3RD_Basic","bWantNextCameraRotationForced",v.forced)||
        !Value(f,b,"CBA_3RD_Basic","WantNextCameraRotationRemainingTick","IntProperty",v.remaining)||v.remaining<0||v.remaining>3||
        !Value(f,b,"CBA_3RD_Basic","CurPOVSettings","ObjectProperty",settings)||!Identify(f,settings,"CBPOVSettings",out.settings))return false;
    int down=0,up=0,boundary=0,outer=0,inner=0;Rotation low{},high{};
    if(!f.StructOffset("CBPOVSettings","SettingsLookDown",down)||!f.StructOffset("CBPOVSettings","SettingsLookUp",up)||
        !f.Offset("SCameraPOVSettings","PitchBoundary","StructProperty",12,boundary))return false;
    low=NativeRead<Rotation>(settings+down+boundary);high=NativeRead<Rotation>(settings+up+boundary);
    v.low=facing_policy::Signed(low.pitch);v.high=facing_policy::Signed(high.pitch);
    bool overridePitch=false;
    if(!Bool(f,b,"CBA_3RD_Basic","bOverrideNextPitch",overridePitch))return false;
    if(overridePitch) {
        if(!Value(f,b,"CBA_3RD_Basic","NextPitchMin","IntProperty",v.low)||!Value(f,b,"CBA_3RD_Basic","NextPitchMax","IntProperty",v.high))return false;
        v.low=facing_policy::Signed(v.low);v.high=facing_policy::Signed(v.high);
    }
    if(v.low>v.high||v.low< -16384||v.high>16384){out.error=L"Unsupported camera limits.";return false;}
    if(!f.StructOffset("Camera","ViewTarget",outer)||!f.StructOffset("TViewTarget","POV",inner)||
        !Value(f,player+outer+inner,"TPOV","Location","StructProperty",v.origin)||!facing_policy::Finite(v.origin)||
        !Value(f,player+outer+inner,"TPOV","Rotation","StructProperty",v.actual)||
        !Value(f,camera,"WhatIfCamera","GPCamRotation","StructProperty",v.gp))return false;
    return true;
}
inline bool SehRead(Session& s,const std::string& id,const Identity& target,NativeFrame& out,bool needsActor) {
    __try {return Probe(s,id,target,out,needsActor);}
    __except(EXCEPTION_EXECUTE_HANDLER){s.faceFaulted=true;return false;}
}
inline bool Read(Session& s,const std::string& id,const Identity& target,NativeFrame& out,bool needsActor) {
    try {return SehRead(s,id,target,out,needsActor);}catch(...){s.faceFaulted=true;return false;}
}
}
