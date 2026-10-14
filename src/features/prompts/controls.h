#pragma once
#include "features/menus/narration.h"
#include <map>

namespace lis::prompts {
inline std::wstring ControlKey(std::wstring text) {
    for(auto& c:text)if(c>=L'A'&&c<=L'Z')c+=L'a'-L'A';
    return text;
}
struct Controls {
    int mode=-1; // DynamicInputSwitchManager: keyboard0, gamepad1
    bool ready=false;
    std::map<std::wstring,std::wstring> actions;
};
inline const wchar_t* ButtonName(const std::wstring& token) {
    // DNETooltipRegister's abstract glyph names, translated to this Windows
    // build's XInput controls. These are buttons, NOT default action bindings.
    static const std::map<std::wstring,const wchar_t*> names={
        {L"cross",L"A"},{L"circle",L"B"},{L"square",L"X"},{L"triangle",L"Y"},
        {L"l1",L"LB"},{L"l2",L"LT"},{L"r1",L"RB"},{L"r2",L"RT"},
        {L"l",L"left stick"},{L"r",L"right stick"},
        {L"l3",L"left stick click"},{L"r3",L"right stick click"},
        {L"start",L"Start"},{L"select",L"Back"},
        {L"previous",L"D pad left"},{L"next",L"D pad right"},{L"mouse",L"mouse"}};
    const auto found=names.find(token);return found==names.end()?nullptr:found->second;
}
struct ExpandedControls {std::wstring text,skeleton;bool hasControl=false,complete=true;};
inline ExpandedControls ExpandControls(const std::wstring& source,const Controls& controls,unsigned depth=0) {
    ExpandedControls out;
    if(depth>8||source.size()>8192){out.complete=false;return out;}
    for(size_t i=0;i<source.size();) {
        if(source[i]!=L'['){out.text+=source[i];out.skeleton+=source[i++];continue;}
        const auto end=source.find(L']',i+1);
        if(end==std::wstring::npos){out.text+=source.substr(i);out.skeleton+=source.substr(i);break;}
        const auto token=ControlKey(source.substr(i+1,end-i-1));
        if(token.rfind(L"action_",0)==0) {
            out.hasControl=true;
            auto found=controls.actions.find(token);
            if(!controls.ready||found==controls.actions.end()){out.complete=false;return out;}
            else {
                const auto nested=ExpandControls(found->second,controls,depth+1);
                if(!nested.complete||CleanText(nested.text).empty()){out.complete=false;return out;}
                out.text+=nested.text;
            }
        } else if(const auto* name=ButtonName(token)){out.hasControl=true;out.text+=name;}
        else if(token==L"whiteletter"||token==L"new"||token==L"butterfly"||token==L"bulle") {
            // Non-control image tags present in shipped HUD translations. The
            // existing plain-text reader already omits these decorative images.
        } else {const auto literal=source.substr(i,end-i+1);out.text+=literal;out.skeleton+=literal;}
        if(out.text.size()>8192){out.complete=false;return out;}
        i=end+1;
    }
    return out;
}
inline std::wstring WithoutControlGlyphs(const std::wstring& text) {
    std::wstring plain;
    for(auto c:text)if(c!=0xfffc&&!(c>=0xe000&&c<=0xf8ff))plain+=c;
    return CleanText(plain);
}
struct ControlCaption {std::wstring text;bool changed=false,unavailable=false;};
inline ControlCaption ControllerCaption(const std::wstring& source,const std::wstring& displayed,const Controls& controls) {
    ControlCaption out{displayed};
    if(controls.mode!=1||CleanText(displayed).empty()||source.empty()||source.rfind(L"$GT_",0)==0)return out;
    const auto resolved=ExpandControls(source,controls);
    if(!resolved.hasControl)return out;
    const auto plain=WithoutControlGlyphs(displayed),spoken=CleanText(resolved.text);
    // Do not substitute a different language, stale variant or unexpanded game
    // variable. Only restore controls when the surrounding visible prose agrees.
    if(resolved.complete&&(plain==CleanText(resolved.skeleton)||plain==spoken)) {
        out.text=spoken;out.changed=out.text!=displayed;return out;
    }
    out.unavailable=true;
    out.text=CleanText(displayed);
    if(!out.text.empty()&&out.text.back()!=L'.'&&out.text.back()!=L'!'&&out.text.back()!=L'?')out.text+=L'.';
    out.text+=L" Controller button names are unavailable.";
    return out;
}
}
