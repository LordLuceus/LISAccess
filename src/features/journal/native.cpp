#include "features/journal/native.h"
#include "features/journal/tab_dispatch.h"
#include "features/world/session.h"
#include "features/world/menu_state.h"
#include "features/prompts/policy.h"
#include "engine/gfx.h"
#include "platform/foreground.h"
#include "platform/text.h"

namespace lis::journal {
namespace {
uint64_t generation=0;
uintptr_t lastMovie=0;
bool wasOpen=false;
bool tabFaulted=false;
void ReadFrame(Frame& out) {
    out={};auto& s=world::GameSession();auto& f=s.reflection;auto& r=f.registry;
    if(s.thread!=GetCurrentThreadId()){out.reason="thread";return;}
    // World's regular bounded discovery populates this metadata even while the
    // player is paused. Never borrow its traversal gate for diary inspection.
    out.reason="discovery";
    if(!s.ready)return;
    unsigned char state=0;bool other=false;
    for(auto m:s.menus) {
        if(!world::IsMenuInstance(r,m))continue;
        unsigned char opening=0;
        if(!f.Value(m,"SensenMenuBase","OpeningState","ByteProperty",opening)||!opening)continue;
        if(r.ClassOf(m)!="SensenMenu"){other=true;continue;}
        out.open=true;
        if(out.movie){out.blocked=true;out.reason="ambiguous_diary";return;}
        out.movie=m;state=opening;
    }
    if(!out.open){wasOpen=false;out.reason="closed";return;}
    if(!wasOpen||lastMovie!=out.movie)++generation;
    wasOpen=true;lastMovie=out.movie;
    out.blocked=other;
    out.identity=std::to_string(out.movie)+":"+std::to_string(generation);
    if(other||state!=2){out.reason=other?"other_menu":"transition";return;}
    out.reason="movie";
    if(!engine::gfx::ValidMovie(out.movie))return;
    out.reason="typed_getters";
    if(!engine::gfx::ValuesReady()||!engine::gfx::ObjectQueriesReady())return;
    out.reason="root";
    uintptr_t root=0,controller=0;
    if(!f.Value(out.movie,"SensenMenuBase","MainMovieClip","ObjectProperty",root))return;
    out.root=prompts::ClipPath(engine::gfx::ObjectString(root,"_target"));
    if(out.root.empty())return;
    for(auto c:s.controllers) {
        uintptr_t pawn=0,ex=0,back=0;
        if(!r.Instance(c)||!r.IsA(c,"WhatIfController")||
            !f.Value(c,"Controller","Pawn","ObjectProperty",pawn)||!r.Instance(pawn)||!r.IsA(pawn,"Pawn")||
            !f.Value(c,"WhatIfController","ExPawn","ObjectProperty",ex)||ex!=pawn||
            !f.Value(pawn,"Pawn","Controller","ObjectProperty",back)||back!=c||
            engine::Read<uintptr_t>(pawn+0xd8)!=engine::Read<uintptr_t>(c+0xd8))continue;
        if(controller){controller=0;break;}controller=c;
    }
    uintptr_t input=0;
    if(controller&&f.Value(controller,"WhatIfController","ExInput","ObjectProperty",input)&&r.Instance(input)&&r.IsA(input,"WhatIfInput")) {
        out.gamepad=f.BoolIsSet(input,"WhatIfInput","bPlayingWithGamepad");
        f.Value(input,"WhatIfInput","ActiveControllerId","IntProperty",out.controllerIndex);
    }
    const auto movie=out.movie;
    Query q{
        [movie](const auto& p){return engine::gfx::String(movie,p);},
        [movie](const auto& p){return engine::gfx::CheckedNumber(movie,p);},
        [movie](const auto& p){return engine::gfx::Text(movie,p);},
        [movie](const auto& p){return engine::gfx::GetterNumber(movie,p);},
        [&](const auto& key){return controller?s.localization.Translate(r,s.base,s.localize,controller,key,L""):std::wstring{};}
    };
    const auto count=q.number("_root.focus_arrays.length");
    out.reason="focus";
    if(!detail::Integer(count,16)||count<1)return;
    const auto focus="_root.focus_arrays."+std::to_string(static_cast<int>(count)-1);
    const auto pageId=q.string(focus+".PageID");
    out.page.inspection=pageId==L"CMD_OBJECT_VIEWER";
    if(out.page.inspection)out.page.title=L"Object inspection";
    out.identity+=":"+out.root+":"+focus+":"+Utf8(pageId);
    // A popup above the actual diary page owns input, including R/arrow keys.
    if(pageId!=q.string(out.root+".current_page.PageID")||!detail::Present(pageId)) {
        out.blocked=true;out.reason="popup";return;
    }
    if(q.number(out.root+"._visible")!=1||q.number(focus+"._visible")!=1){out.reason="hidden";return;}
    const auto mode=q.number(out.root+".InputMode");
    if(mode==0||mode==1)out.gamepad=mode==1;
    out.tabs=q.number(out.root+".TabsVisible")==1&&q.number(out.root+".BackgroundVisible")==1;
    out.page=ReadPage(q,focus);
    out.identity+=":"+out.page.id;
    out.reason=out.page.ready?"ready":"page_not_ready loading="+std::to_string(q.number(focus+".LoadingFinished"))+
        " count="+std::to_string(q.number(focus+".PageElements.length"))+" old="+std::to_string(q.number(focus+".OldPageElements.length"))+
        " selection="+std::to_string(q.number(focus+".current_selection"));
}
std::wstring Tab(const Frame& expected,int direction) {
    if(!IsGameForeground())return {};
    Frame fresh;ReadFrame(fresh);
    if(!fresh.open||fresh.blocked||fresh.identity!=expected.identity)
        return L"The journal page changed. Try again.";
    if(!fresh.tabs)return L"Tabs are unavailable on this page. Go back first.";
    if(direction!=-1&&direction!=1)return L"Invalid journal request.";
    // Mirror DNEFocusUtils.DispatchKeyFunc, including the nonrepeat pass and
    // keyboard index/from_joystick arguments. Let the page consume input while
    // loading; only then may the root send native PREVIOUS/NEXT_MENU_PAGE.
    const auto count=engine::gfx::CheckedNumber(fresh.movie,"_root.focus_arrays.length");
    if(!detail::Integer(count,16)||count<1)return L"Journal focus is unavailable.";
    const auto result=DispatchTabKeys(static_cast<int>(count),direction,[&](int i,int key) {
        return engine::gfx::DispatchKey(fresh.movie,"_root.focus_arrays."+std::to_string(i)+".onKeyFunc",static_cast<float>(key));
    });
    if(result==engine::gfx::KeyResult::Failed)return L"Could not switch journal tabs.";
    return result==engine::gfx::KeyResult::Handled?std::wstring{}:L"No journal tab change is available.";
}
void TabResult(const Frame& expected,int direction,std::wstring& result){result=Tab(expected,direction);}
bool SafeTab(const Frame& expected,int direction,std::wstring& result) {
    __try {TabResult(expected,direction,result);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
bool StillOpen(uintptr_t movie) {
    auto& s=world::GameSession();
    return s.thread==GetCurrentThreadId()&&s.reflection.registry.Instance(movie)&&s.reflection.registry.ClassOf(movie)=="SensenMenu"&&
        engine::Read<unsigned char>(movie+0x1f4)==2;
}
bool SampleNative(Frame& frame) {
    __try {ReadFrame(frame);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
std::wstring ChangeTab(const Frame& expected,int direction) {
    if(tabFaulted)return L"Journal tab switching has been disabled after an input fault.";
    std::wstring result;
    if(!SafeTab(expected,direction,result)){tabFaulted=true;return L"Journal tab input failed and was disabled.";}
    return result;
}
}
