#pragma once
#include "features/menus/narration.h"
#include <cmath>
#include <cwchar>
#include <string>
#include <vector>

namespace lis::menus {
inline bool IsStartupSequence(const std::wstring& page){return page==L"CMD_BOOTSEQUENCE";}
inline std::string PopupRoot(const std::wstring& page,const std::string& focus) {
    return IsStartupSequence(page)?focus+".SequenceRoot_mc":focus;
}
class StartupDelivery {
    bool following=false;
public:
    bool Interrupt(const std::wstring& page,bool hasText) {
        if(!hasText)return true;
        if(IsStartupSequence(page)){following=true;return false;}
        const bool result=!following;following=false;return result;
    }
};
inline bool PopupValuePresent(const std::wstring& value) {
    return !value.empty() && value != L"undefined" && value != L"null";
}

// MovieClip visibility is inherited. The sequence below is flat, so check
// display parents too, not just the leaf. Stop at the movie root; fail closed
// on unexpectedly deep trees. String queries distinguish absent from zero.
template<class StringQuery>
bool PopupVisible(std::string path, const StringQuery& string) {
    for (unsigned depth = 0; depth < 32; ++depth) {
        auto visible = string(path + "._visible");
        if (!PopupValuePresent(visible) || visible == L"false" || visible == L"0") return false;
        auto alpha = string(path + "._alpha");
        if (PopupValuePresent(alpha)) {
            wchar_t* end = nullptr;
            double value = std::wcstod(alpha.c_str(), &end);
            if (end != alpha.c_str() && *end == 0 && (!std::isfinite(value) || value <= 0)) return false;
        }
        if (!PopupValuePresent(string(path + "._parent._name"))) return true;
        path += "._parent";
    }
    return false;
}

// DNEComposedClip.PrepareClipForComposition registers every descendant in
// context.open_seq.sub_sequences, in factory order (including anonymous clips).
// It is a flat list, NOT a display-child list: do not recursively visit it.
// DNESimpleText names its foreground DNETextLR leaf SP_Label_Parent and its
// decorative duplicate SP_Label_Shadow. DNETextLR.GetText returns exactly
// SP_Label.textField.text, so no ActionScript invocation is needed here.
template<class StringQuery, class NumberQuery>
std::vector<std::wstring> PopupText(const std::string& focus,
                                  const StringQuery& string, const NumberQuery& number) {
    std::vector<std::wstring> parts;
    if (!PopupVisible(focus, string)) return parts;
    const std::string sequence = focus + ".open_seq.sub_sequences";
    float count = number(sequence + ".length");
    if (!std::isfinite(count) || count < 1 || count > 128 || std::floor(count) != count) return parts;
    for (unsigned i = 0; i < static_cast<unsigned>(count); ++i) {
        const std::string clip = sequence + "." + std::to_string(i);
        if (string(clip + ".composition_name") != L"SP_Label_Parent") continue;
        if (!PopupVisible(clip + ".SP_Label.textField", string)) continue;
        auto text = string(clip + ".SP_Label.textField.text");
        if (!PopupValuePresent(text)) continue;
        text = CleanText(text);
        if (text.empty()) continue;
        bool duplicate = false;
        for (const auto& part : parts) if (part == text) duplicate = true;
        if (!duplicate) parts.push_back(text);
    }
    return parts;
}
}
