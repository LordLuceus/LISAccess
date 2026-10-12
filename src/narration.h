#pragma once
#include <string>
#include <vector>
#include <cwctype>

namespace lis {
inline std::wstring CleanText(const std::wstring& value) {
    std::wstring out;
    bool space = false;
    for (wchar_t c : value) {
        if (std::iswspace(c) || c == 0xa0) { space = !out.empty(); continue; }
        if (space) out += L' ';
        out += c; space = false;
    }
    // An unresolved localization token is not a player-facing label.
    if (out.rfind(L"$GT_", 0) == 0) return {};
    return out;
}
inline std::wstring DescribeMenu(const std::vector<std::wstring>& pieces, int modalType = -1) {
    std::wstring text;
    std::vector<std::wstring> seen;
    for (const auto& piece : pieces) {
        auto p = CleanText(piece);
        if (p.empty()) continue;
        bool duplicate = false;
        for (const auto& s : seen) if (s == p) duplicate = true;
        if (duplicate) continue;
        seen.push_back(p);
        if (!text.empty()) text += L", ";
        text += p;
    }
    if (text.empty()) return {};
    auto sentence = [&](const wchar_t* value) {
        if (text.back() != L'.' && text.back() != L'?' && text.back() != L'!') text += L'.';
        text += L' '; text += value;
    };
    if (modalType == 0) sentence(L"Enter to confirm. Escape to cancel.");
    else if (modalType == 1) sentence(L"Enter to confirm.");
    else if (modalType == 2) sentence(L"Escape to cancel.");
    else if (modalType == 3) sentence(L"Enter to continue.");
    return text;
}
}
