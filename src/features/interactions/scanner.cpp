#include "features/interactions/fields.h"
#include "features/interactions/scanner.h"
#include "features/world/session.h"
#include "platform/log.h"
#include "platform/foreground.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include "features/navigation/collision.h"
#include <algorithm>
#include <cmath>
#include <functional>
namespace lis::world {
using engine::Copy;
using engine::Read;
using engine::Array;
uintptr_t InteractionWidget(Session& session,uintptr_t actor) {
    Array components{};
    if(!session.reflection.Value(actor,"Actor","AllComponents","ArrayProperty",components)||!ValidArray(components,128))return 0;
    uintptr_t result=0;
    for(int i=0;i<components.count;++i) {
        uintptr_t component=Read<uintptr_t>(components.data+i*4);Array spawned{};
        if(!session.reflection.registry.Instance(component)||!session.reflection.registry.IsA(component,"SensenARComponent")||Read<uintptr_t>(component+0x28)!=actor ||
            !session.reflection.Value(component,"SensenARComponent","SpawnedActors","ArrayProperty",spawned)||!ValidArray(spawned,32))continue;
        for(int j=0;j<spawned.count;++j) {
            uintptr_t ar=Read<uintptr_t>(spawned.data+j*4),widget=0;
            if(!session.reflection.registry.Instance(ar)||!session.reflection.registry.IsA(ar,"SensenARActor")||Read<uintptr_t>(ar+0xd8)!=session.liveWorld ||
                !session.reflection.Value(ar,"SensenARActor","ARObject","ObjectProperty",widget)||!session.reflection.registry.Instance(widget)||!session.reflection.registry.IsA(widget,"WhatIfInteractionWidget")||Read<uintptr_t>(widget+0x28)!=ar)continue;
            if(result && result!=widget)return 0;
            result=widget;
        }
    }
    return result;
}
bool Binding(Session& session,unsigned slot,engine::Name& out) {
    static const char* names[]={"Choice_Y","Choice_X","Choice_B","Choice_A"};
    // Four entries only: the adjacent fifth FName is unrelated (DeleteSaveGame).
    return slot<4&&Copy(&out,session.base+engine::profile::ChoiceBindings+slot*sizeof(out),sizeof(out))&&out.number==0&&session.reflection.registry.Name(out.index)==names[slot];
}
int EligibleState(Session& session,uintptr_t actor) {
    Array actions{};int stride=0;
    if(!session.reflection.Value(actor,"WhatIfInteractionActor","Actions","ArrayProperty",actions)||!ValidArray(actions,64))return false;
    uintptr_t property=session.reflection.Field("WhatIfInteractionActor","Actions","ArrayProperty",12),inner=Read<uintptr_t>(property+0x80);
    if(!session.reflection.registry.Registered(inner)||session.reflection.registry.ClassOf(inner)!="StructProperty"||!Copy(&stride,inner+0x44,4)||stride!=0x138)return false;
    int best=0;
    for(int i=0;i<actions.count;++i) {
        const int state=reinterpret_cast<engine::ActionEligibility>(session.base+engine::profile::ActionEligibility)(reinterpret_cast<void*>(actor),reinterpret_cast<void*>(actions.data+i*stride));
        if(state==2)return 2;
        if(state==1)best=1;
    }
    return best;
}
void Interactions(Session& session,Snapshot& s) {
    uintptr_t input=0,component=0,current=0;Array actors{};
    if(session.reflection.Value(session.liveController,"WhatIfController","ExInput","ObjectProperty",input)&&session.reflection.registry.Instance(input)) {
        s.gamepad=session.reflection.BoolIsSet(input,"WhatIfInput","bPlayingWithGamepad");
        session.reflection.Value(input,"WhatIfInput","ActiveControllerId","IntProperty",s.controllerIndex);
    }
    s.interactionStatus="component_unavailable";
    if(!interaction::ReadComponent(std::cref(session.reflection),session.liveController,component)||!session.reflection.registry.Instance(component)||!session.reflection.registry.IsA(component,"DNEWhatIfInteractionComponent"))return;
    s.interactionStatus="state_unavailable";
    if(!session.reflection.Value(component,"DNEWhatIfInteractionComponent","RegisteredActors","ArrayProperty",actors)||!ValidArray(actors,2048) ||
        !session.reflection.Value(component,"DNEWhatIfInteractionComponent","CurrentInteraction","ObjectProperty",current) ||
        !session.reflection.Value(component,"DNEWhatIfInteractionComponent","ActorInteractionFocused","ObjectProperty",session.liveFocus))return;
    s.interactionBusy=current!=0;
    if(s.interactionBusy){s.interactionStatus="busy";session.liveFocus=0;return;}
    s.interactionStatus="schema_unavailable";
    const uintptr_t actionsProperty=session.reflection.Field("WhatIfInteractionActor","Actions","ArrayProperty",12);
    const uintptr_t inner=Read<uintptr_t>(actionsProperty+0x80);
    if(!session.reflection.Field("WhatIfInteractionActor","ObjectName","NameProperty",8)||!actionsProperty ||
        !session.reflection.registry.Registered(inner)||session.reflection.registry.ClassOf(inner)!="StructProperty"||Read<int>(inner+0x44)!=0x138)return;
    s.interactionStatus="ready";
    // Listing has no interaction-distance cutoff. Actual action dispatch still
    // requires the game's real focus, open UI and freshly available action.
    for(int i=0;i<actors.count;++i) {
        uintptr_t actor=Read<uintptr_t>(actors.data+i*4);Vec3 position{};
        if(!session.reflection.registry.Instance(actor)||!session.reflection.registry.IsA(actor,"WhatIfInteractionActor")||Read<uintptr_t>(actor+0xd8)!=session.liveWorld ||
            !session.reflection.ClearBool(actor,"Actor","bDeleteMe")||!session.reflection.BoolIsSet(actor,"WhatIfInteractionActor","bIsInteractionActive") ||
            !Copy(&position,actor+0x54,sizeof(position))||!Finite(position))continue;
        const int eligibility=EligibleState(session,actor);
        if(!eligibility)continue;
        const std::wstring raw=interaction::ReadObjectLabel(std::cref(session.reflection),[&](uint32_t index){return engine::WideName(session.reflection.registry,index);},actor);
        if(raw.empty())continue;
        Target target;target.stableId=session.reflection.registry.Path(actor);target.label=session.localization.Translate(session.reflection.registry,session.base,session.localize,session.liveController,raw);target.angleRestricted=eligibility==1;target.position=position;
        if(target.stableId.empty())continue;
        interaction::ReadApproachRadius(std::cref(session.reflection),actor,component,target.approachRadius);
        uintptr_t object=0;
        // This game also represents NPCs as SkeletalMeshActorMAT, not Pawns.
        // Its authored Chara localization namespace identifies those names in
        // every language, without guessing from the displayed English text.
        if(raw.rfind(L"$GT_Sensen_Chara_",0)==0 ||
            (session.reflection.Value(actor,"WhatIfInteractionActor","ObjectActor","ObjectProperty",object)&&session.reflection.registry.Instance(object)&&session.reflection.registry.IsA(object,"Pawn")))target.kind=Kind::Person;
        if(std::none_of(s.targets.begin(),s.targets.end(),[&](const Target& other){return other.stableId==target.stableId;}))s.targets.push_back(target);
        if(actor!=session.liveFocus)continue;
        uintptr_t widget=InteractionWidget(session,actor);
        if(!widget||!session.reflection.BoolIsSet(actor,"WhatIfInteractionActor","bIsInteractionUIOpened"))continue;
        s.focusedId=target.stableId;s.focusedLabel=target.label;
        for(unsigned slot=0;slot<4;++slot) {
            engine::Name binding{};
            const auto number=std::to_string(slot+1);
            if(!Binding(session,slot,binding)||!session.reflection.BoolIsSet(widget,"WhatIfInteractionWidget",("button"+number+"_visible").c_str()))continue;
            const auto label=session.reflection.Text(widget,"WhatIfInteractionWidget",("action_name"+number).c_str());
            if(!label.empty())s.actions[slot].label=session.localization.Translate(session.reflection.registry,session.base,session.localize,session.liveController,label);
        }
    }
}
bool SafeInteractions(Session& session,Snapshot& s) {
    __try {Interactions(session,s);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){session.interactionFaulted=true;return false;}
}
}
