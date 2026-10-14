#include "features/exits/exits.h"
#include "features/exits/policy.h"
#include "features/world/session.h"
#include "engine/memory.h"
#include <functional>

namespace lis::exits {
namespace {
struct Owners {uintptr_t behavior=0,component=0,volume=0;};
bool Resolve(world::Session& s,uintptr_t announcement,Owners& out) {
    auto& f=s.reflection;auto& r=f.registry;
    if(!r.Instance(announcement)||!r.IsA(announcement,"IA_GFxUIControlAnnouncement"))return false;
    out.behavior=engine::Read<uintptr_t>(announcement+0x28);
    if(!r.Instance(out.behavior)||!r.IsA(out.behavior,"IA_Behavior"))return false;
    engine::Name name{};
    if(!f.Value(out.behavior,"IA_Behavior","BehaviorName","NameProperty",name)||name.number||
       !world::EqualPath(r.Name(name.index),"BH_LoadingZone_TriggerZone"))return false;
    out.component=engine::Read<uintptr_t>(out.behavior+0x28);
    if(!r.Instance(out.component)||!r.IsA(out.component,"DNEBehaviorComponent"))return false;
    uintptr_t activeBehavior=0;
    if(!f.Value(out.component,"DNEBehaviorComponent","Behavior","ObjectProperty",activeBehavior)||activeBehavior!=out.behavior)return false;
    out.volume=engine::Read<uintptr_t>(out.component+0x28);
    if(!r.Instance(out.volume)||!r.IsA(out.volume,"DNEVolume"))return false;
    uint8_t control=255,type=255;
    // Verified eGFxControlAnnouncement_Open=0, Invoke=6; Tooltips=1.
    return f.Value(announcement,"IA_GFxUIControlAnnouncement","ControlType","ByteProperty",control)&&
        (control==0||control==6)&&
        f.Value(announcement,"IA_GFxUIControlAnnouncement","AnnouncementType","ByteProperty",type)&&type==1;
}
struct Value {uint8_t type,padding[3];uint32_t boolean;float number;engine::Array text;};
static_assert(sizeof(Value)==24);
std::wstring Label(world::Session& s,uintptr_t announcement) {
    auto& f=s.reflection;auto& r=f.registry;
    engine::Array args{};Value first{};
    const uintptr_t property=f.Field("IA_GFxUIControlAnnouncement","InitArguments","ArrayProperty",12);
    if(!property)return {};
    const uintptr_t inner=engine::Read<uintptr_t>(property+0x80);
    if(!r.Registered(inner)||r.ClassOf(inner)!="StructProperty"||engine::Read<int>(inner+0x44)!=sizeof(Value)||
       !f.Value(announcement,"IA_GFxUIControlAnnouncement","InitArguments","ArrayProperty",args)||
       !world::ValidArray(args,64)||!args.count||!engine::Copy(&first,args.data,sizeof(first))||
       first.type!=3||!world::ValidArray(first.text,8192)||first.text.count<2)return {};
    std::wstring key(static_cast<size_t>(first.text.count),L'\0');
    if(!engine::Copy(key.data(),first.text.data,key.size()*sizeof(wchar_t))||key.back()!=L'\0')return {};
    key.pop_back();
    return DestinationLabel(s.localization.Translate(r,s.base,s.localize,s.liveController,key,L""));
}
void ReadAreas(world::Session& s,world::Snapshot& snapshot) {
    auto& f=s.reflection;auto& r=f.registry;
    for(uintptr_t announcement:s.exitAnnouncements) {
        Owners owner;
        if(!Resolve(s,announcement,owner)||engine::Read<uintptr_t>(owner.volume+0xd8)!=s.liveWorld||
           !f.ClearBool(owner.volume,"Actor","bDeleteMe")||!f.BoolIsSet(owner.component,"DNEBehaviorComponent","bActive"))continue;
        const std::string id=r.Path(owner.volume);
        if(id.empty()||std::any_of(snapshot.targets.begin(),snapshot.targets.end(),[&](const auto& t){return t.stableId==id;}))continue;
        uintptr_t collision=0;Bounds bounds{};
        if(!ReadCollision(std::cref(f),owner.volume,collision)||!r.Instance(collision)||!r.IsA(collision,"BrushComponent")||
           !f.Value(collision,"PrimitiveComponent","Bounds","StructProperty",bounds)||!ExitBoundsValid(bounds))continue;
        AddDestination(snapshot,id,Label(s,announcement),bounds);
    }
}
}
bool IsSource(world::Session& session,uintptr_t announcement) {Owners owners;return Resolve(session,announcement,owners);}
bool Sample(world::Session& session,world::Snapshot& snapshot) {
    __try {ReadAreas(session,snapshot);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){session.exitFaulted=true;return false;}
}
}
