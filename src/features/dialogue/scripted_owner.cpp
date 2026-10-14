#include "features/dialogue/scripted_owner.h"
#include "features/world/session.h"
#include "features/interactions/scanner.h"
#include "engine/build_profile.h"

namespace lis::dialogue {
bool ScriptedWaiting(world::Session& s,uintptr_t owner) {
    int wait=-1,chosen=0;
    return s.reflection.Value(owner,"IA_Dialogue","WaitClosingState","IntProperty",wait)&&
        s.reflection.Value(owner,"IA_Dialogue","ChoiceIndex","IntProperty",chosen)&&
        s.reflection.ClearBool(owner,"IA_SeqLatent","Finished")&&
        ScriptedChoiceWaiting(wait,chosen,false);
}
bool ScriptedOwner(world::Session& s,uintptr_t controller,uintptr_t world,RegisteredChoices& out) {
    auto& f=s.reflection;auto& r=f.registry;out={};
    // Never inspect partially applied listener registration during input dispatch.
    NativeChoiceMap map{};int interfaceOffset=0;
    if(!f.ClearBool(controller,"WhatIfController","bShouldDeferRegistration")||
       !f.Value(controller,"WhatIfController","IAInputTrigWatcherMap","StructProperty",map)||
       !f.Offset("IA_InputBox","VfTable_IInputListenerInterface","StructProperty",4,interfaceOffset)||
       interfaceOffset!=0x80)return false;
    std::array<engine::Name,4> bindings{};
    for(unsigned i=0;i<4;++i)if(!world::Binding(s,i,bindings[i]))return false;
    auto mask=[&](engine::Name key)->unsigned {
        if(key.index==0&&key.number==0)return 15; // Native wildcard registration.
        for(unsigned i=0;i<4;++i)if(key.index==bindings[i].index&&key.number==bindings[i].number)return 1u<<i;
        return 0;
    };
    auto candidate=[&](ChoiceListener listener) {
        const uintptr_t object=listener.object;
        if(!r.Instance(object)||!r.IsA(object,"IA_Dialogue")||listener.interface!=object+interfaceOffset||
           engine::Read<uintptr_t>(listener.interface)!=s.base+engine::profile::ScriptedDialogInterface||
           engine::Read<uintptr_t>(engine::Read<uintptr_t>(listener.interface)+8)!=s.base+engine::profile::ScriptedDialogPress||
           !ScriptedWaiting(s,object))return false;
        // Script actions are UObjects, not actors. Follow their real outer chain
        // to the owning actor; never read an Actor world pointer from the action.
        for(uintptr_t p=engine::Read<uintptr_t>(object+0x28),n=0;p&&n<16;++n,p=engine::Read<uintptr_t>(p+0x28)) {
            if(!r.Instance(p))return false;
            if(r.IsA(p,"Actor"))return engine::Read<uintptr_t>(p+0xd8)==world&&f.ClearBool(p,"Actor","bDeleteMe");
        }
        return false;
    };
    return ReadRegisteredChoices(map,engine::Copy,mask,candidate,out);
}
}
