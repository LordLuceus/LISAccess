#include "features/rewind/native.h"
#include "features/world/session.h"
#include "features/world/menu_state.h"
#include "features/prompts/policy.h"
#include "engine/gfx.h"
#include "platform/foreground.h"

namespace lis::rewind {
namespace {
using engine::Read;
uintptr_t manager=0,widgets=0;
int cursor=0;
void DiscoverRewind(world::Session& s) {
    auto& r=s.reflection.registry;
    if(r.Instance(manager)&&r.ClassOf(manager)=="WhatIfRewindManager"&&
       r.Instance(widgets)&&r.ClassOf(widgets)=="GFxUIWidgets")return;
    engine::Array objects{};if(!r.Registry(objects))return;
    const int end=(std::min)(objects.count,cursor+8192);
    for(;cursor<end;++cursor) {
        const auto p=Read<uintptr_t>(objects.data+cursor*4);
        if(!r.Instance(p))continue;
        const auto type=r.ClassOf(p);
        if(type=="WhatIfRewindManager")manager=p;
        if(type=="GFxUIWidgets")widgets=p;
    }
    if(cursor>=objects.count)cursor=0;
}
uintptr_t Player(world::Session& s) {
    auto& f=s.reflection;auto& r=f.registry;uintptr_t result=0;
    for(auto c:s.controllers) {
        uintptr_t pawn=0,ex=0,back=0;
        if(!r.Instance(c)||!r.IsA(c,"WhatIfController")||
           !f.Value(c,"Controller","Pawn","ObjectProperty",pawn)||!r.Instance(pawn)||!r.IsA(pawn,"Pawn")||
           !f.Value(c,"WhatIfController","ExPawn","ObjectProperty",ex)||ex!=pawn||
           !f.Value(pawn,"Pawn","Controller","ObjectProperty",back)||back!=c||
           Read<uintptr_t>(pawn+0xd8)!=Read<uintptr_t>(c+0xd8))continue;
        if(result)return 0;result=c;
    }
    return result;
}
bool Visible(world::Session& s,const char*& reason) {
    auto& f=s.reflection;auto& r=f.registry;int offset=0;
    reason="renderer_missing";
    struct RenderData {uintptr_t target,material;} render{};uintptr_t proxy=0;
    if(!f.BoolIsSet(manager,"WhatIfRewindManager","bRewindBarCreated")||
       !f.Value(manager,"WhatIfRewindManager","RewindBarRenderer","StructProperty",render)||
       !r.Instance(render.material)||
       !f.Value(manager,"WhatIfRewindManager","RewindBarRendererProxy","StructProperty",proxy)||
       proxy<0x10000||!Read<uintptr_t>(proxy+0x78))return false;
    reason="widget_metadata";
    if(!r.Instance(widgets)||r.ClassOf(widgets)!="GFxUIWidgets"||
       !f.StructOffset("GFxUIWidgets","AnnouncementRewindHUD",offset))return false;
    const auto widget=widgets+offset;uintptr_t owner=0,root=0;
    for(const char* name:{"bWidgetDisabled","bWidgetHiddenByMenu","bWidgetHiddenByCinematic","bWidgetHiddenByFSM"})
        if(!f.ClearBool(widget,"GFxWidget",name)){reason=name;return false;}
    reason="widget_owner";
    if(!f.Value(widget,"GFxWidget","Owner","ObjectProperty",owner)||!engine::gfx::ValidMovie(owner)||
       !f.Value(widget,"GFxWidget","Root","ObjectProperty",root)||!r.Instance(root))return false;
    if(!engine::gfx::ObjectQueriesReady())engine::gfx::Discover();
    reason="widget_path";
    const auto path=prompts::ClipPath(engine::gfx::ObjectString(root,"_target"));
    if(path.empty())return false;
    reason="clip_hidden";
    if(engine::gfx::DisplayString(owner,path+"._visible")!=L"true"||engine::gfx::Number(owner,path+"._alpha")<=0)return false;
    reason="bar_hidden";
    if(engine::gfx::DisplayString(owner,path+".RewindBar_mc._visible")!=L"true")return false;
    reason="ready";return true;
}
void ReadFrame(Frame& out) {
    auto& s=world::GameSession();auto& f=s.reflection;auto& r=f.registry;
    out.reason="discovery";
    if(!s.ready||s.thread!=GetCurrentThreadId()||!IsGameForeground())return;
    DiscoverRewind(s);const auto controller=Player(s);
    out.reason="player";
    if(!controller||!r.Instance(manager)||r.ClassOf(manager)!="WhatIfRewindManager")return;
    const auto world=Read<uintptr_t>(controller+0xd8);uintptr_t pauser=0;
    unsigned char gameplay=0,state=255;
    if(!r.Instance(world)||!f.Value(controller,"WhatIfController","CurrentGameplayType","ByteProperty",gameplay)||
       gameplay<1||gameplay>3)return;
    out.reason="pause_or_menu";
    if(!f.Value(world,"WorldInfo","Pauser","ObjectProperty",pauser)||pauser)return;
    // Tutorial/gameplay pauses can coexist with the real rewind HUD. They are
    // not menu ownership and must not suppress a visible, active rewind.
    for(const char* flag:{"bSystemPause","bDeviceLostPause","bFocusLostPause","bStreamingPause","bMenuPause"})
        if(!f.ClearBool(controller,"WhatIfController",flag)){out.reason=flag;return;}
    for(auto m:s.menus)if(world::BlockingMenu(f,m))return;
    if(!f.Value(manager,"RewindManager","CurrentState","ByteProperty",state)||state>4)return;
    out.context=true;out.manager=manager;out.world=world;out.controller=controller;out.state=state;
    out.reason="timeline_fields";
    if(!f.Value(manager,"RewindManager","ReferenceTime","FloatProperty",out.reference)||
       !std::isfinite(out.reference)||out.reference<0)return;
    const bool forced=f.BoolIsSet(manager,"WhatIfRewindManager","bForceRewindBarVisible");
    if(!forced&&!f.ClearBool(manager,"WhatIfRewindManager","bForceRewindBarVisible"))return;
    if(!f.Value(manager,"WhatIfRewindManager",forced?"OverriddenEndTime":"RewindEndTime","FloatProperty",out.end))return;
    uintptr_t action=0;
    if(f.Value(manager,"RewindManager","LastPlayerAction","StructProperty",action)) {
        if(!action)out.markerKnown=true;
        else if(action>=0x10000&&engine::Copy(&out.marker,action+4,4)&&std::isfinite(out.marker)) {
            // Native gauge writer RVAae5b10 reads this native event's time@+4.
            // It is NOT a UObject or the reflected WhatIfEvent payload@+0.
            out.markerKnown=true;out.markerPresent=out.marker>=0;
        }
    }
    out.valid=true;out.reason="ready";out.visible=!Rewinding(out)||Visible(s,out.reason);
}
bool SafeRead(Frame& out){__try{ReadFrame(out);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
}
bool Sample(Frame& out){return SafeRead(out);}
}
