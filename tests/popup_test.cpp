#include "features/menus/popup.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>

void check(bool condition, const char* name) {
    if (!condition) { std::cerr << "FAILED: " << name << '\n'; std::exit(1); }
}
int main() {
    using namespace lis::menus;
    std::map<std::string, std::wstring> values;
    float count = 4;
    unsigned queries = 0;
    auto string = [&](const std::string& path) {
        ++queries;
        auto it = values.find(path);
        return it == values.end() ? std::wstring{} : it->second;
    };
    auto number = [&](const std::string&) { return count; };
    const std::string sequence = "focus.open_seq.sub_sequences.";
    values["focus._visible"] = L"true";
    values["focus._alpha"] = L"100";
    auto leaf = [&](unsigned i, const wchar_t* name, const wchar_t* text) {
        auto path = sequence + std::to_string(i);
        values[path + ".composition_name"] = name;
        path += ".SP_Label.textField";
        values[path + "._visible"] = L"true";
        values[path + "._alpha"] = L"100";
        values[path + ".text"] = text;
    };
    leaf(0, L"SP_Label_Shadow", L"Decoration");
    leaf(1, L"SP_Label_Parent", L"  Localized autosave notice\n");
    leaf(2, L"SP_Label_Parent", L"Continue");
    leaf(3, L"SP_Label_Parent", L"Continue");
    auto read = [&] { return lis::DescribeMenu(PopupText("focus", string, number)); };
    check(read() == L"Localized autosave notice, Continue", "anonymous composition, factory order, shadow exclusion, deduplication");
    values["focus._alpha"] = L"0";
    check(read().empty(), "transparent opening popup deferred");
    values["focus._alpha"] = L"100";
    values["focus._visible"] = L"false";
    check(read().empty(), "hidden focus suppressed");
    values["focus._visible"] = L"true";
    const auto hidden = sequence + "1.SP_Label.textField";
    values[hidden + "._parent._name"] = L"hidden container";
    values[hidden + "._parent._visible"] = L"false";
    check(read() == L"Continue", "hidden display ancestor suppressed in flat sequence");
    values[hidden + "._parent._name"] = L"";
    values[hidden + ".text"] = L"$GT_unresolved";
    check(read() == L"Continue", "untranslated token excluded");
    for (float invalid : {129.f, -1.f, 1.5f, std::numeric_limits<float>::quiet_NaN()}) {
        count = invalid;
        check(read().empty(), "invalid sequence size rejected");
    }
    count = 128;
    queries = 0;
    read();
    check(queries < 200, "bounded flat sequence query budget");
    auto deep = [](const std::string& path) -> std::wstring {
        if (path.find("_name") != std::string::npos) return L"parent";
        return L"true";
    };
    check(!PopupVisible("cyclic", deep), "ancestor depth bounded");
    check(!PopupValuePresent(L"undefined") && !PopupValuePresent(L"null") && PopupValuePresent(L"0"), "absent modal metadata differs from confirm type zero");
    check(IsStartupSequence(L"CMD_BOOTSEQUENCE")&&!IsStartupSequence(L"CMD_OUTGAMEMENU"),"boot page is message content, not selected-menu content");
    check(PopupRoot(L"CMD_BOOTSEQUENCE","focus")=="focus.SequenceRoot_mc"&&PopupRoot(L"","focus")=="focus","boot composition uses its dynamically replaced sequence root");
    values[hidden+".text"]=L"Localized autosave notice";count=4;
    std::map<std::string,std::wstring> boot;
    for(const auto& pair:values)boot["focus.SequenceRoot_mc"+pair.first.substr(5)]=pair.second;
    auto bootString=[&](const std::string& path){auto it=boot.find(path);return it==boot.end()?std::wstring{}:it->second;};
    check(lis::DescribeMenu(PopupText(PopupRoot(L"CMD_BOOTSEQUENCE","focus"),bootString,number))==L"Localized autosave notice, Continue","boot page reads the dynamic composition without selection metadata");
    StartupDelivery delivery;
    check(!delivery.Interrupt(L"CMD_BOOTSEQUENCE",true),"first timed startup notice queues");
    check(!delivery.Interrupt(L"CMD_BOOTSEQUENCE",true),"following notice does not cut off the previous one");
    delivery.Interrupt(L"CMD_BOOTSEQUENCE",false);
    check(!delivery.Interrupt(L"CMD_OUTGAMETITLE",true),"automatic title readout does not cut off the autosave explanation");
    check(delivery.Interrupt(L"CMD_OUTGAMESLOT",true),"subsequent user menu navigation keeps normal interruption");
    std::cout << "PASS: popup extraction policy\n";
}
