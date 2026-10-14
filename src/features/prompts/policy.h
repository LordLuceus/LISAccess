#pragma once
#include "features/menus/narration.h"
#include <algorithm>
#include <deque>
#include <map>

namespace lis::prompts {
inline std::string ClipPath(const std::wstring& target) {
    if(target.empty() || target.size()>240 || target.front()!=L'/')return {};
    std::string out="_root";
    for(wchar_t c:target) {
        if(c==L'/')out+='.';
        else if((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'$')out+=static_cast<char>(c);
        else return {};
    }
    return out.back()=='.'?std::string{}:out;
}
inline std::wstring Adapt(const std::wstring& key,const std::wstring& displayed) {
    if(CleanText(displayed).empty())return {};
    // Exact shipped identifiers, never English substring replacement. Both input
    // methods are described: native translation can switch icons within one ID.
    auto id=key;
    for(auto& c:id)if(c>=L'a'&&c<=L'z')c-=L'a'-L'A';
    // HUD.INT's [Tuto] section; DNENotification.ParseNotificationString
    // uppercases device-dependent IDs. Preserve exact whole-ID matching.
    const auto lead=L"Select ";
    if(id==L"$GT_HUD_TUTO_TUTO07"||id==L"$GT_HUD_TUTO_TUTO30")
        return std::wstring(lead)+L"the photo with Page Up or Page Down, or D pad left or right. Press F or click the left stick to face it. Once it is announced as focused, choose Look using the announced number or controller button.";
    if(id==L"$GT_HUD_TUTO_TUTO09"||id==L"$GT_HUD_TUTO_TUTO25")
        return std::wstring(lead)+L"the camera with Page Up or Page Down, or D pad left or right. Press F or click the left stick to face it. Once it is announced as focused, choose the selfie action using the announced number or controller button.";
    if(id==L"$GT_HUD_TUTO_TUTO24")
        return L"Select a target with Page Up or Page Down, or D pad left or right. Press F or click the left stick to face it. Once focused, use the announced number or controller button for the action you want. No mouse dragging is needed.";
    return CleanText(displayed);
}
struct Item {std::string channel;std::wstring key,text;};
// Transition detection and retention are independent of engine state and speech.
// Timed messages remain repeatable after disappearing. A closed/reopened message
// is a new event; a stationary message is not re-announced every timer tick.
class History {
    std::map<std::string,std::wstring> previous;
    std::deque<std::wstring> pending;
    std::wstring latest;
public:
    void Observe(const std::vector<Item>& frame) {
        std::map<std::string,std::wstring> current;
        for(const auto& item:frame) {
            const auto text=Adapt(item.key,item.text);
            if(text.empty())continue;
            auto signature=item.key+L"\n"+text;
            current[item.channel]=signature;
            const auto found=previous.find(item.channel);
            if(found!=previous.end()&&found->second==signature)continue;
            latest=text;
            if(std::find(pending.begin(),pending.end(),text)==pending.end()) {
                if(pending.size()==8)pending.pop_front();
                pending.push_back(text);
            }
        }
        previous=std::move(current);
    }
    std::wstring Take() {
        if(pending.empty())return {};
        auto text=pending.front();pending.pop_front();return text;
    }
    std::wstring Repeat() {pending.clear();return latest.empty()?L"No prompt has been read yet.":latest;}
    void Remember(const std::wstring& text){if(!CleanText(text).empty())latest=text;}
    void Background(){pending.clear();}
};
}
