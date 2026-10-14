#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>
#include <algorithm>
#include <tuple>
#include "features/menus/narration.h"

namespace lis::journal {
enum class View { Other, Journal, Messages, List };
struct Item {
    std::string id; std::wstring text;
    float x=std::numeric_limits<float>::quiet_NaN(),y=std::numeric_limits<float>::quiet_NaN();
};
struct Page {
    bool ready = false;
    View kind = View::Other;
    std::string id;
    std::wstring title;
    std::wstring position;
    bool document=false;
    bool inspection=false;
    std::vector<Item> items;
    int selected = -1;
    int nativeIndex = -1, nativeCount = 0;
};
struct Query {
    std::function<std::wstring(const std::string&)> string;
    std::function<float(const std::string&)> number;
    std::function<std::wstring(const std::string&)> text;
    std::function<float(const std::string&)> getter;
    std::function<std::wstring(const std::wstring&)> translate;
};
namespace detail {
// Missing/non-numeric GFx values MUST be NaN, not zero. Text receives an
// object path and calls its parameterless GetText, not FindObject or an action.
constexpr int MaxElements = 256;
inline bool Integer(float n, int maximum) {
    return std::isfinite(n) && n >= 0 && n <= maximum && std::floor(n) == n;
}
inline bool Present(const std::wstring& s) {
    return !s.empty() && s != L"undefined" && s != L"null";
}
inline std::wstring Clean(std::wstring s) {
    if (!Present(s) || s.size() > 32768 || s.find(L"$GT_") != s.npos ||
        s.find(L"img://") != s.npos || s.find(L"tx_") == 0) return {};
    // Actual rendered getters are preferred. Do not mistake an SMS '<3' for
    // markup; unresolved styling in fallback text is not a useful readout.
    for(const auto* tag:{L"<font",L"<FONT",L"</font",L"</FONT",L"<img"})if(s.find(tag)!=s.npos)return {};
    return CleanText(s);
}
inline std::wstring Local(const Query& q, const std::wstring& s) {
    if (!Present(s)) return {};
    if (s.front() != L'$') return Clean(s);
    if (!q.translate) return {};
    auto result = q.translate(s);
    return result == s ? std::wstring{} : Clean(result);
}
inline std::string Fingerprint(const std::wstring& s) {
    std::uint64_t hash = 14695981039346656037ull;
    for (wchar_t c : s) { hash ^= static_cast<std::uint32_t>(c); hash *= 1099511628211ull; }
    return std::to_string(hash);
}
inline std::wstring Upper(std::wstring s) {
    for (auto& c : s) if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
    return s;
}
inline bool Path(const std::string& s) {
    if (s.empty() || s.size() > 512) return false;
    for (unsigned char c : s)
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '_' && c != '.' && c != '[' && c != ']') return false;
    return true;
}
inline void BookOrder(std::vector<Item>& items) {
    // Current supplied layout, not resource-name suffixes or static story data.
    // The centered book spread uses negative X for the left page. Preserve
    // unknown-position slots (e.g. the post-it) rather than invent coordinates.
    std::vector<Item> placed;
    for(const auto& item:items)if(std::isfinite(item.x)&&std::isfinite(item.y))placed.push_back(item);
    std::stable_sort(placed.begin(),placed.end(),[](const Item& a,const Item& b) {
        return std::make_tuple(a.x>=0,a.y,a.x)<std::make_tuple(b.x>=0,b.y,b.x);
    });
    size_t at=0;
    for(auto& item:items)if(std::isfinite(item.x)&&std::isfinite(item.y))item=std::move(placed[at++]);
}
} // namespace detail

inline Page ReadPage(const Query& q, const std::string& focus) {
    using namespace detail;
    Page p;
    if (!q.string || !q.number || !q.text || !Path(focus)) return p;
    const auto s = [&](const std::string& field) { return q.string(focus + "." + field); };
    const auto n = [&](const std::string& field) { return q.number(focus + "." + field); };
    const auto t = [&](const std::string& field) { return Clean(q.text(focus + "." + field)); };
    const auto command = s("PageID");
    const bool sms = command == L"CMD_SMS";
    const bool journal = command == L"CMD_JOURNAL";
    const bool character = command == L"CMD_CHARACTERVIEWERPAGE";
    const bool contacts = command == L"CMD_SMSCHARACTERSELECT";
    // Files and world inspections use DNEObjectViewerPage, NOT DNEUIPage. Enter/A exposes the
    // game's own transcription; do not read its hidden description beforehand.
    if(command==L"CMD_FILESELECT"||command==L"CMD_OBJECT_VIEWER") {
        p.inspection=command==L"CMD_OBJECT_VIEWER";
        p.title=p.inspection?L"Object inspection":L"Files";p.document=true;
        const float count=n("ObjectImages.length"),index=n("current_page");
        if(n("scroll_locked")!=0||!Integer(count,MaxElements)||!Integer(index,MaxElements)||index>=count)return p;
        const bool reading=n("DescriptionVisible")==1;
        p.id=focus+(p.inspection?":object:":":file:")+Fingerprint(s("ObjectName"))+":"+std::to_string(static_cast<int>(index))+":"+std::to_string(reading);
        std::wstring body;
        if(reading&&n("DescriptionMovieClip._visible")==1)body=t("DescriptionMovieClip.Label");
        if(body.empty())body=reading?L"Document text unavailable":Present(s("ObjectDescriptions."+std::to_string(static_cast<int>(index))))?L"Image. A text version is available.":L"Image. No text version is available.";
        p.items.push_back({p.id,body});p.selected=0;p.nativeIndex=static_cast<int>(index);p.nativeCount=static_cast<int>(count);p.ready=true;
        return p;
    }
    const bool list = contacts || command == L"CMD_CHARACTERSELECT" ||
        command == L"CMD_PICTURESELECT" || command == L"CMD_FILESELECT" ||
        command == L"CMD_FILELOCATIONSELECT" || command == L"CMD_PICTUREEPISODESELECT" ||
        command == L"CMD_LOGDAYSELECT";
    if (!sms && !journal && !character && !list) return p;
    p.kind = sms ? View::Messages : list ? View::List : View::Journal;
    p.title = sms ? L"Messages" : contacts ? L"Contacts" : journal ? L"Journal" :
        character || command == L"CMD_CHARACTERSELECT" ? L"Characters" :
        command == L"CMD_LOGDAYSELECT" ? L"Journal days" :
        command == L"CMD_FILESELECT" || command == L"CMD_FILELOCATIONSELECT" ? L"Files" : L"Photos";
    // DNEUIPage.CanChangeMenuPage: no old elements and LoadingFinished.
    const float old = n("OldPageElements.length");
    if (n("LoadingFinished") != 1 || (std::isfinite(old) && old != 0)) return p;
    const float count = n("PageElements.length");
    if (!Integer(count, MaxElements)) return p;
    p.id = focus + ":" + Fingerprint(command);
    if (journal) {
        const float day = n("BrowseInfo.CurrentDay"), page = n("BrowseInfo.CurrentPage");
        // JournalParams.JournalPerDay explicitly defines PREQUEL as day -1.
        if (!(day==-1||Integer(day, 10000)) || !Integer(page, 10000)) return p;
        p.id += ":" + std::to_string(static_cast<int>(day)) + ":" + std::to_string(static_cast<int>(page));
        // NamedImpl/ClipImpl attaches named clips directly to composition owner.
        auto label = t("PageIndexMovie.TEXT");
        p.position=label;
    } else if (character) {
        auto name = s("BrowseInfo.CharacterName");
        if (!Present(name)) return p;
        p.id += ":" + Fingerprint(name);
    }
    if (list) {
        // DNEMenuPageBase.GetCurrentSelectionIndex returns current_selection;
        // selection_clips excludes locked entries, so it is NOT a PageElements index.
        const float size = n("selection_clips.length");
        const float index = q.getter ? q.getter(focus + ".GetCurrentSelectionIndex") :
            std::numeric_limits<float>::quiet_NaN();
        if (!Integer(size, MaxElements)) return p;
        if(size==0){p.ready=true;return p;}
        if (!Integer(index, MaxElements) || index >= size) return p;
        p.nativeIndex=static_cast<int>(index);p.nativeCount=static_cast<int>(size);
        const auto clip = "selection_clips." + std::to_string(static_cast<int>(index));
        const auto element = clip + ".ui_element";
        if (n(clip + "._visible") != 1) return p;
        const auto valid = n(element + ".ValidData");
        if (valid != 0 && valid != 1) return p;
        std::wstring label;
        if (valid == 0) label = L"Locked";
        else {
            label = contacts ? t(clip + ".FromText") : std::wstring{};
            if (label.empty()) label = Local(q, s(element + ".DataTitle"));
            if (label.empty()) label = L"Text unavailable";
            if (contacts) {
                const auto unread = n(element + ".UnreadSMSCount");
                if (!Integer(unread, 100000)) return p;
                label += L". " + std::to_wstring(static_cast<int>(unread)) + L" unread";
            } else if (command != L"CMD_CHARACTERSELECT" && command != L"CMD_LOGDAYSELECT") {
                label += L". Image text unavailable";
            }
        }
        p.id += ":" + Fingerprint(s("BrowseInfo.CurrentPage"));
        p.items.push_back({p.id + ":" + std::to_string(static_cast<int>(index)) + ":" +
            Fingerprint(s(element + ".DataName")) + ":" + Fingerprint(label), label});
        p.selected = 0;
        p.ready = true;
        return p;
    }
    std::wstring receiver, emitter, receiverTitle, emitterTitle;
    if (sms) {
        if(!Present(s("ReceiverDesc.DataName"))||!Present(s("EmitterDesc.DataName")))return p;
        receiver = Upper(s("ReceiverDesc.DataName")); emitter = Upper(s("EmitterDesc.DataName"));
        if (!Present(receiver) || !Present(emitter)) return p;
        receiverTitle = Local(q, s("ReceiverDesc.DataTitle"));
        emitterTitle = Local(q, s("EmitterDesc.DataTitle"));
        p.id += ":" + Fingerprint(receiver) + ":" + Fingerprint(emitter);
        if (!emitterTitle.empty()) p.title += L". " + emitterTitle;
    }
    std::wstring variant;
    for (int i = 0; i < static_cast<int>(count); ++i) {
        const auto e = "PageElements." + std::to_string(i);
        std::wstring body;
        if (sms) {
            // Never inspect FadeInSMS or ResponseContent: their bodies exist before delivery.
            // Old historical elements legitimately have both flags undefined.
            const auto timer = n(e + ".UpdateTextTimer");
            const auto timerString = s(e + ".UpdateTextTimer");
            const auto fading = n(e + ".fade_in_message");
            if (std::isfinite(timer) || Present(timerString) || fading == 1) continue;
            if (std::isfinite(fading) && fading != 0) continue;
            // LabelGeneric owns LABEL and DATE; outer composition owns AUTOUPDATETEXT.
            body = t(e + ".movie_clip.AUTOUPDATETEXT.LABEL");
            if (body.empty()) body = Local(q, s(e + ".DataTitle"));
            if (body.empty()) body = L"Message text unavailable";
            const auto from = Upper(s(e + ".From"));
            auto sender = from == receiver ? receiverTitle : from == emitter ? emitterTitle : std::wstring{};
            if (sender.empty()) sender = L"Unknown sender";
            const auto date = t(e + ".movie_clip.AUTOUPDATETEXT.DATE");
            // SMSDate is [format, seven arguments], not a translatable scalar.
            // When virtualized, omit date rather than expose an array or guess its format.
            body = sender + (date.empty()?L": ":L", "+date+L": ") + body;
            if (Present(s(e + ".ThumbnailName"))) body += L". Attached image; image text unavailable";
        } else {
            if (n(e + ".movie_clip._visible") != 1) continue;
            const auto type = n(e + ".ElementType");
            if (type == 3) {
                // TextFunc returns SimpleText as the custom composition's root_frame.
                body = t(e + ".movie_clip.root_frame");
                if(body.empty())body=Local(q,s(e+".ResourceValue"));
                if (body.empty()) body = L"Text unavailable";
            } else if (type == 0 || type == 1 || type == 2) {
                // Do not bury prose under one announcement per decorative image.
                continue;
            } else if (journal && i == 0 && n("HasPostIt") == 1) {
                // This is the current native-supplied, visible post-it only,
                // never a lookup through the game's future objective database.
                body = Local(q,s(e+".PostItText"));
                if(!body.empty())body=Local(q,L"$GT_Menus_Journal_OBJECTIVETITLE")+L": "+body;
                else body = L"Objective text unavailable";
            } else continue;
            variant += s(e + ".ResourceValue") + L"\n" + body + L"\n";
        }
        // Virtualizing a date clip must not move the logical SMS cursor.
        const auto identity = sms ? s(e + ".From") + L"\n" + s(e + ".DataTitle") : body;
        p.items.push_back({p.id + ":" + std::to_string(i) + ":" + Fingerprint(identity), body});
        if(!sms){p.items.back().x=n(e+".PositionX");p.items.back().y=n(e+".PositionY");}
    }
    if (!sms) {BookOrder(p.items);p.id += ":" + Fingerprint(variant);}
    p.ready = true;
    p.selected = p.items.empty() ? -1 : sms ? static_cast<int>(p.items.size()) - 1 : 0;
    return p;
}
} // namespace lis::journal
