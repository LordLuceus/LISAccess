#pragma once
#include "features/menus/narration.h"
#include <array>
#include <string>

namespace lis::dialogue {
struct Choice {std::wstring key,text;bool offered=false;};
struct Snapshot {
    bool context=false,visible=false,timed=false;
    bool gamepad=false; // presentation only; switching device does not change an answer
    // Native owner supplies dialog ID, page and a lifetime/rewind generation.
    // A page number alone cannot distinguish reopening or rewinding a dialog.
    std::string identity;
    std::array<Choice,4> choices;
};

inline std::wstring Describe(const Snapshot& snapshot) {
    if(!snapshot.context||!snapshot.visible)return {};
    // DNEDialogWidget.OnItemValidated: indices 0..3 are Y, X, B, A.
    // Keep these native indices even when an earlier slot has no readable text.
    const wchar_t* keyboard[]={L"1: ",L"2: ",L"3: ",L"4: "};
    const wchar_t* controller[]={L"Y: ",L"X: ",L"B: ",L"A: "};
    std::wstring out;
    for(std::size_t i=0;i<snapshot.choices.size();++i) {
        const auto& choice=snapshot.choices[i];
        if(!choice.offered)continue;
        const auto text=CleanText(choice.text);
        if(text.empty())continue;
        if(!out.empty())out+=L'\n';
        out+=snapshot.gamepad?controller[i]:keyboard[i];out+=text;
    }
    // Preserve the game's punctuation and prose; never add trailing periods.
    if(!out.empty()&&snapshot.timed)out=L"Timed choice\n"+out;
    return out;
}

// slot is the zero-based native widget index, NOT a compacted answer index.
// This is only the engine-free part of activation validation. The caller must
// also revalidate native objects, ownership, bindings and the input lifecycle.
inline bool CanActivate(const Snapshot& expected,const Snapshot& current,int slot,
                        bool foreground,bool menuBlocked) {
    if(!foreground||menuBlocked||slot<0||slot>=4)return false;
    if(!expected.context||!expected.visible||!current.context||!current.visible)return false;
    if(expected.identity.empty()||expected.identity!=current.identity)return false;
    if(expected.timed!=current.timed)return false;
    for(std::size_t i=0;i<current.choices.size();++i) {
        const auto& before=expected.choices[i];
        const auto& now=current.choices[i];
        if(before.offered!=now.offered)return false;
        // Compare raw strings, not cleaned descriptions: stale bindings should
        // fail closed even when their displayed prose happens to look the same.
        if(now.offered&&(before.key!=now.key||before.text!=now.text))return false;
    }
    const auto& choice=current.choices[static_cast<std::size_t>(slot)];
    return choice.offered&&!CleanText(choice.text).empty();
}
}
