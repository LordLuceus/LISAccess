#pragma once
#include "features/menus/popup.h"
#include "features/prompts/policy.h"

namespace lis::prompts {
struct Caption {std::wstring key,text;};
template<class StringQuery,class NumberQuery,class ControlReader>
Caption ReadComposition(const std::string& path,const StringQuery& string,const NumberQuery& number,const ControlReader& controls) {
    // Shipped DNEOpenAnimationSeq: closed=0, closing=1, opening=2, opened=3.
    const float opening=number(path+".opening_state");
    if((opening!=2&&opening!=3)||!menus::PopupVisible(path,string))return {};
    const auto sequence=path+".open_seq.sub_sequences";
    const float count=number(sequence+".length");
    if(!std::isfinite(count)||count<1||count>128||std::floor(count)!=count)return {};
    Caption result;
    std::vector<std::wstring> parts;
    for(unsigned i=0;i<static_cast<unsigned>(count);++i) {
        const auto clip=sequence+"."+std::to_string(i);
        const auto shadow=string(clip+".shadow_text");
        if(shadow==L"true"||shadow==L"1"||string(clip+".composition_name")==L"SP_Label_Shadow")continue;
        const auto text=string(clip+".SP_Label.textField.text");
        if(!menus::PopupValuePresent(text)||!menus::PopupVisible(clip+".SP_Label.textField",string))continue;
        const auto key=string(clip+".wanted_text");
        if(key.rfind(L"$GT_",0)==0) {
            if(!result.key.empty())result.key+=L"|";
            result.key+=key;
        }
        // Notifications can contain several independent instructions. Adapt each
        // leaf, never replace a whole container using its first label's ID.
        parts.push_back(Adapt(key,controls(key,text)));
    }
    result.text=DescribeMenu(parts);
    return result;
}
template<class StringQuery,class NumberQuery>
Caption ReadComposition(const std::string& path,const StringQuery& string,const NumberQuery& number) {
    return ReadComposition(path,string,number,[](const auto&,const auto& text){return text;});
}
}
