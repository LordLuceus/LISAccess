#include "features/journal/tab_dispatch.h"
#include <iostream>
#include <vector>
#include <limits>
#include <utility>
#include <cstddef>
#include <cstring>
using namespace lis::engine::gfx;
int main() {
    int errors=0;auto check=[&](bool ok,const char* message){std::cout<<(ok?"PASS: ":"FAIL: ")<<message<<'\n';errors+=!ok;};
    check(KeyReturn(0,0,0,false)==KeyResult::Unhandled&&KeyReturn(1,0,0,false)==KeyResult::Unhandled,"AS undefined and null are normal fall-through");
    check(KeyReturn(4,0,0,false)==KeyResult::Unhandled&&KeyReturn(4,1,0,false)==KeyResult::Handled,"false falls through; true consumes");
    check(KeyReturn(2,0,0,false)==KeyResult::Unhandled&&KeyReturn(2,0,-1,false)==KeyResult::Handled&&
        KeyReturn(2,0,std::numeric_limits<float>::quiet_NaN(),false)==KeyResult::Unhandled,"AS numeric truthiness preserves zero and NaN semantics");
    check(KeyReturn(3,0,0,false)==KeyResult::Unhandled&&KeyReturn(3,0,0,true)==KeyResult::Handled,"AS string truthiness");
    check(KeyReturn(99,0,0,false)==KeyResult::Failed,"unknown value tags are not accepted as unhandled");
    struct WireString {std::uint32_t data;int count,capacity;};
    using WireValue=ScriptValue<WireString>;
    static_assert(sizeof(WireValue)==24&&sizeof(WireValue::type)==1);
    static_assert(offsetof(WireValue,boolean)==4&&offsetof(WireValue,number)==8&&offsetof(WireValue,text)==12);
    WireValue native{};
    // Actual reflected layout, with deliberately dirty native return padding.
    // A zero-initialized synthetic enum/BOOL cannot exercise this fault.
    std::memset(&native,0xa5,sizeof(native));native.type=4;native.boolean=0xfffffffe;
    unsigned oldType=0;std::memcpy(&oldType,&native,sizeof(oldType));
    check(oldType!=4&&native.boolean!=0,"fixture would break the old whole-word type/Boolean decoder");
    check(KeyReturn(native.type,native.boolean,0,false)==KeyResult::Unhandled,"dirty padding and unused Boolean bits cannot turn false into failure or true");
    native.boolean|=1;
    check(KeyReturn(native.type,native.boolean,0,false)==KeyResult::Handled,"true Boolean survives dirty padding");
    native.type=0;
    check(KeyReturn(native.type,native.boolean,0,false)==KeyResult::Unhandled,"undefined proxy return survives dirty native padding");
    native.type=2;native.number=0;
    check(KeyReturn(native.type,native.boolean,native.number,false)==KeyResult::Unhandled,"numeric zero ignores unrelated native fields");
    std::vector<std::pair<int,int>> calls;
    // Shape from native DoHidePage + DNEMenuPageFocusProxy: live page above two
    // covered placeholders without onKeyFunc, root tab handler at stack bottom.
    auto result=lis::journal::DispatchTabKeys(4,1,[&](int i,int key) {
        calls.emplace_back(i,key);
        return KeyReturn(i==1||i==2?0:4,i==0,0,false);
    });
    check(result==KeyResult::Handled&&calls==std::vector<std::pair<int,int>>{{3,103},{3,1103},{2,103},{2,1103},{1,103},{1,1103},{0,103}},"right tab reaches root through native missing-handler proxies");
    calls.clear();
    result=lis::journal::DispatchTabKeys(3,-1,[&](int i,int key){calls.emplace_back(i,key);return KeyResult::Handled;});
    check(result==KeyResult::Handled&&calls==std::vector<std::pair<int,int>>{{2,100}},"loading/current-page handler can consume left tab without reaching root");
    int invoked=0;
    result=lis::journal::DispatchTabKeys(4,1,[&](int,int){++invoked;return KeyResult::Failed;});
    check(result==KeyResult::Failed&&invoked==1,"bridge failure stops dispatch rather than bypassing a page");
    invoked=0;result=lis::journal::DispatchTabKeys(2,-1,[&](int,int){++invoked;return KeyResult::Unhandled;});
    check(result==KeyResult::Unhandled&&invoked==4,"fully unhandled stack is bounded and does not claim success");
    invoked=0;
    for(auto p:std::vector<std::pair<int,int>>{{0,1},{17,1},{2,0}})
        check(lis::journal::DispatchTabKeys(p.first,p.second,[&](int,int){++invoked;return KeyResult::Handled;})==KeyResult::Failed,"invalid stack/direction rejected");
    check(invoked==0,"invalid request never invokes script");
    return errors?1:0;
}
