#include "features/dialogue/native.h"
#include "features/dialogue/bindings.h"
#include "features/world/session.h"
#include "features/interactions/fields.h"
#include "features/interactions/scanner.h"
#include "engine/build_profile.h"
#include "platform/foreground.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace lis::dialogue {
namespace {
using engine::Read;
using engine::Copy;
uintptr_t widgets=0,rewindManager=0;
int cursor=0;
bool inputFaulted=false;

// Reuse the world reader's reflected metadata, not its traversal/physics gate.
// Dialogue deliberately disables movement and can run in cinematic mode.
void DiscoverUi(world::Session& s) {
    auto& r=s.reflection.registry;
    if(r.Instance(widgets)&&r.ClassOf(widgets)=="GFxUIWidgets"&&
       r.Instance(rewindManager)&&r.ClassOf(rewindManager)=="WhatIfRewindManager")return;
    engine::Array objects{};if(!r.Registry(objects))return;
    const int end=(std::min)(objects.count,cursor+8192);
    for(;cursor<end;++cursor) {
        const uintptr_t p=Read<uintptr_t>(objects.data+cursor*4);
        if(!r.Instance(p))continue;
        const auto type=r.ClassOf(p);
        if(type=="GFxUIWidgets")widgets=p;
        if(type=="WhatIfRewindManager")rewindManager=p;
    }
    if(cursor>=objects.count)cursor=0;
}

struct ChoiceMap {
    engine::Array slots;
    uint32_t bits[4];uintptr_t heapBits;
    int bitCount,bitCapacity,firstFree,freeCount;
    uint32_t hashInline;uintptr_t hashHeap;int hashSize;
};
static_assert(sizeof(ChoiceMap)==60);
bool Bindings(world::Session& s,uintptr_t actor,std::array<int,4>& choices) {
    ChoiceMap map{};choices.fill(-1);
    if(!s.reflection.Value(actor,"WhatIfDialogActor","BindingChoiceIndexMap","StructProperty",map)||
       !world::ValidArray(map.slots,16))return false;
    if(!map.slots.count&&!map.bitCount&&!map.freeCount)return true;
    if(map.heapBits||map.bitCount!=map.slots.count||
       map.bitCapacity!=128||map.freeCount<0||map.freeCount>map.slots.count)return false;
    std::array<BindingEntry,16> entries{};
    if(map.slots.count&&!Copy(entries.data(),map.slots.data,map.slots.count*sizeof(BindingEntry)))return false;
    return DecodeBindings(entries.data(),map.slots.count,map.bits[0],map.freeCount,choices);
}
struct Value {uint8_t type,padding[3];uint32_t boolean;float number;engine::Array text;};
static_assert(sizeof(Value)==24);
std::wstring Text(const Value& value) {
    if(value.type!=3||!world::ValidArray(value.text,8192)||value.text.count<1)return {};
    std::wstring text(static_cast<size_t>(value.text.count),L'\0');
    if(!Copy(text.data(),value.text.data,text.size()*sizeof(wchar_t))||text.back()!=L'\0')return {};
    text.pop_back();return text;
}
uintptr_t Player(world::Session& s) {
    auto& f=s.reflection;auto& r=f.registry;uintptr_t result=0;
    for(uintptr_t c:s.controllers) {
        uintptr_t pawn=0,ex=0,back=0;
        if(!r.Instance(c)||!r.IsA(c,"WhatIfController")||
           !f.Value(c,"Controller","Pawn","ObjectProperty",pawn)||!r.Instance(pawn)||!r.IsA(pawn,"Pawn")||
           !f.Value(c,"WhatIfController","ExPawn","ObjectProperty",ex)||ex!=pawn||
           !f.Value(pawn,"Pawn","Controller","ObjectProperty",back)||back!=c||
           Read<uintptr_t>(pawn+0xd8)!=Read<uintptr_t>(c+0xd8))continue;
        if(result)return 0;
        result=c;
    }
    return result;
}
void ReadFrame(NativeFrame& out) {
    auto& s=world::GameSession();auto& f=s.reflection;auto& r=f.registry;
    if(s.thread!=GetCurrentThreadId())return;
    DiscoverUi(s);if(!s.ready)return;
    out.reason="player_context";
    const uintptr_t controller=Player(s);unsigned char gameplay=0;
    if(!controller||!f.Value(controller,"WhatIfController","CurrentGameplayType","ByteProperty",gameplay)||gameplay!=3)return;
    out.view.context=true;
    uintptr_t input=0;
    // Same authoritative mode flag as object-action narration; not whether a
    // controller merely happens to be connected, nor the traversal snapshot.
    if(f.Value(controller,"WhatIfController","ExInput","ObjectProperty",input)&&r.Instance(input))
        out.view.gamepad=f.BoolIsSet(input,"WhatIfInput","bPlayingWithGamepad");
    out.reason="dialogue_owner";
    uintptr_t component=0,actor=0;
    if(!interaction::ReadComponent(std::cref(f),controller,component)||!r.Instance(component)||!r.IsA(component,"DNEWhatIfInteractionComponent")||
       !f.Value(component,"DNEWhatIfInteractionComponent","CurrentDialog","ObjectProperty",actor)||!r.Instance(actor)||!r.IsA(actor,"WhatIfDialogActor"))return;
    out.reason="world_pause";
    const uintptr_t world=Read<uintptr_t>(controller+0xd8);uintptr_t pauser=0;
    if(!r.Instance(world)||Read<uintptr_t>(actor+0xd8)!=world||
       !f.Value(world,"WorldInfo","Pauser","ObjectProperty",pauser)||pauser)return;
    for(const char* flag:{"bSystemPause","bDeviceLostPause","bFocusLostPause","bStreamingPause","bMenuPause","bTutoPause","bGameplayPause","bBehaviorPause"})
        if(!f.ClearBool(controller,"WhatIfController",flag)){out.reason=flag;return;}
    out.reason="menu";
    for(uintptr_t m:s.menus)if(r.Instance(m)) {
        unsigned char opening=0;
        if(!f.Value(m,"SensenMenuBase","OpeningState","ByteProperty",opening)||opening)return;
    }
    unsigned char rewind=255;int sequence=0;
    out.reason="rewind";
    // ERewindManagerState: 0 ERMS_Playing; 1..4 rewind/pause/fast-forward.
    if(!r.Instance(rewindManager)||r.ClassOf(rewindManager)!="WhatIfRewindManager"||
       !f.Value(rewindManager,"RewindManager","CurrentState","ByteProperty",rewind)||rewind!=0||
       !f.Value(rewindManager,"RewindManager","RewindSequenceCounter","IntProperty",sequence))return;
    int offset=0;uintptr_t owner=0,root=0;
    out.reason="widget_owner";
    if(!r.Instance(widgets)||r.ClassOf(widgets)!="GFxUIWidgets"||!f.StructOffset("GFxUIWidgets","AnnouncementDialogue",offset))return;
    const uintptr_t widget=widgets+offset;unsigned char opening=0;
    if(!f.Value(widget,"GFxWidget","OpeningState","ByteProperty",opening)||opening!=2||
       !f.Value(widget,"GFxWidget","Owner","ObjectProperty",owner)||!r.Instance(owner)||!r.IsA(owner,"GFxUIWidgetPlayer")||
       !f.Value(widget,"GFxWidget","Root","ObjectProperty",root)||!r.Instance(root)||!r.IsA(root,"GFxObject"))return;
    uintptr_t ownedWidgets=0;
    if(!f.Value(owner,"GFxUIWidgetPlayer","UIWidgets","ObjectProperty",ownedWidgets)||ownedWidgets!=widgets)return;
    for(const char* flag:{"bWidgetDisabled","bWidgetHiddenByFSM","bWidgetHiddenByCinematic","bWidgetHiddenByMenu"})
        if(!f.ClearBool(widget,"GFxWidget",flag)){out.reason=flag;return;}
    out.reason="waiting_for_choices";
    int wait=0,event=0,chosen=0,page=-1;uintptr_t node=0;engine::Name id{};
    if(!f.Value(actor,"WhatIfDialogActor","WaitClosingState","IntProperty",wait)||
       !f.Value(actor,"WhatIfDialogActor","WIDA_WAIT_EVENT","IntProperty",event)||wait!=event||
       !f.Value(actor,"WhatIfDialogActor","ChoiceIndexChoosen","IntProperty",chosen)||chosen!=-1||
       !f.Value(actor,"WhatIfDialogActor","CurrentDialogNode","StructProperty",node)||!node||
       !f.Value(actor,"WhatIfDialogActor","CurrentChoicePageIndex","IntProperty",page)||page<0||
       !f.Value(actor,"WhatIfDialogActor","CurrentDialogId","NameProperty",id))return;
    out.blocked=false;out.actor=actor;
    out.reason="choice_data";
    out.problem=L"Dialogue choices could not be read safely. Native dialogue controls still work.";
    std::array<int,4> indices{};engine::Array args{};Value values[12]{};
    if(!Bindings(s,actor,indices)||!f.Value(actor,"WhatIfDialogActor","SavedInvokeArgs","ArrayProperty",args)||
       !world::ValidArray(args,64)||args.count!=12||!Copy(values,args.data,sizeof(values)))return;
    const uintptr_t arrayProperty=f.Field("WhatIfDialogActor","SavedInvokeArgs","ArrayProperty",12);
    const uintptr_t inner=Read<uintptr_t>(arrayProperty+0x80);
    if(!r.Registered(inner)||r.ClassOf(inner)!="StructProperty"||Read<int>(inner+0x44)!=sizeof(Value))return;
    out.view.identity=std::to_string(actor)+":"+std::to_string(node)+":"+std::to_string(page)+":"+
        r.Name(id.index)+":"+std::to_string(id.number)+":"+std::to_string(sequence);
    bool any=false;
    for(unsigned i=0;i<4;++i) {
        if(values[i].type!=3||values[i+4].type!=4)return;
        auto& choice=out.view.choices[i];choice.key=Text(values[i]);
        out.view.identity+=":"+std::to_string(indices[i]);
        choice.offered=(values[i+4].boolean&1)&&indices[i]>=0&&!choice.key.empty();
        if(!choice.offered)continue;
        choice.text=CleanText(s.localization.Translate(r,s.base,s.localize,controller,choice.key,L""));
        if(choice.text.empty()||choice.text.rfind(L"$GT_",0)==0)return;
        any=true;
    }
    float timeout=0;
    if(!f.Value(actor,"WhatIfDialogActor","TotalChoiceTimeout","FloatProperty",timeout)||!std::isfinite(timeout))return;
    out.view.timed=timeout>0;out.view.visible=any;out.problem.clear();out.reason=any?"ready":"no_offered_choices";
}
bool Send(world::Session& s,uintptr_t actor,unsigned slot) {
    __try {
        if(inputFaulted)return false;
        int offset=0;engine::Name binding{};
        if(!s.reflection.Offset("WhatIfGameplayActor","VfTable_IInputListenerInterface","StructProperty",4,offset)||
           offset!=0x26c||!world::Binding(s,slot,binding))return false;
        const uintptr_t listener=actor+offset,method=Read<uintptr_t>(Read<uintptr_t>(listener)+8);
        if(method!=s.base+engine::profile::DialogPress)return false;
        // Captured vtable + static handler: FName and ignored third word, ret 0x0c.
        // Native code validates the map, records the choice and closes its own UI.
        using Press=uint32_t(__thiscall*)(void*,engine::Name,void*);
        reinterpret_cast<Press>(method)(reinterpret_cast<void*>(listener),binding,nullptr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){inputFaulted=true;return false;}
}
}
bool SampleNative(NativeFrame& frame) {
    __try {ReadFrame(frame);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
std::wstring ActivateNative(const Snapshot& expected,unsigned slot) {
    NativeFrame current;
    if(!SampleNative(current))return L"Dialogue inspection failed; no choice was sent.";
    if(!CanActivate(expected,current.view,static_cast<int>(slot),IsGameForeground(),current.blocked))
        return L"The dialogue choices changed or are unavailable. Read them again before choosing.";
    if(!Send(world::GameSession(),current.actor,slot))return inputFaulted
        ? L"Dialogue input failed and has been disabled. Check the game before choosing again."
        : L"This dialogue input interface is unsupported. No choice was sent.";
    return {};
}
}
