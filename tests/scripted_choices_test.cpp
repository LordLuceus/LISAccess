#include "features/dialogue/registered_choices.h"
#include "features/dialogue/bindings.h"
#include "features/dialogue/policy.h"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>
using namespace lis::dialogue;
struct Region {uintptr_t address;size_t size;};
int main() {
    int errors=0;
    auto check=[&](bool ok,const char* what){std::cout<<(ok?"PASS: ":"FAIL: ")<<what<<'\n';errors+=!ok;};
    // Captured principal map: free row0, allocated X->branch0 / B->branch1.
    const BindingEntry principal[]={{-1,0,-1,0},{1,0,-1,0},{2,1,1,0}};
    std::array<int,4> indices{};
    check(DecodeBindings(principal,3,6,1,indices)&&indices==std::array<int,4>{-1,0,1,-1},"principal hole does not become a phantom Y choice");
    Snapshot view;view.context=true;view.visible=true;view.identity="scripted:principal:state27:sequence2:paused";
    view.choices[1]={L"$GT_HUD_MC_E1_A2",L"REPORT NATHAN",true};
    view.choices[2]={L"$GT_HUD_MC_E1_A1",L"HIDE THE TRUTH",true};
    check(Describe(view)==L"2: REPORT NATHAN\n3: HIDE THE TRUTH","principal labels use captured native slots and shipped HUD.INT prose");
    view.gamepad=true;
    check(Describe(view)==L"X: REPORT NATHAN\nB: HIDE THE TRUTH","principal controller assignments match the live map");
    check(ScriptedChoiceWaiting(0,-1,false),"fresh-load principal waits at state0, chosen-1, unfinished");
    check(!ScriptedChoiceWaiting(1,-1,false)&&!ScriptedChoiceWaiting(2,-1,false)&&
          !ScriptedChoiceWaiting(0,0,false)&&!ScriptedChoiceWaiting(0,-1,true),"closing, answered and finished actions cannot own a choice");
    for(unsigned state=0;state<7;++state) {
        check(ChoiceRewindAllowed(false,state)==(state==0),"ordinary rewind guard remains unchanged");
        check(ChoiceRewindAllowed(true,state)==(state==0||state==2),"only scripted choices admit paused timeline; active rewind/fast-forward stay blocked");
    }
    auto newer=view;newer.identity+="new_generation";
    check(!CanActivate(view,newer,1,true,false),"scripted stale generation cannot select");
    check(!CanActivate(view,view,1,false,false)&&!CanActivate(view,view,1,true,true),"foreground and pause guards still block scripted selection");

    // Captured owner/interface pair; key IDs normalized to 1..4 for the fixture.
    constexpr uintptr_t owner=1396174848;
    std::array<ChoiceListener,2> listeners={{{owner,owner+0x80},{123456,123584}}};
    lis::engine::Array list{reinterpret_cast<uintptr_t>(listeners.data()),2,2};
    std::array<WatcherEntry,13> rows{}; // Captured watcher-map count13, bits8191.
    for(unsigned i=0;i<4;++i)rows[i]={{i+1,0},reinterpret_cast<uintptr_t>(&list),-1,0};
    for(unsigned i=4;i<rows.size();++i)rows[i].key={99,0};
    NativeChoiceMap map{};map.slots={reinterpret_cast<uintptr_t>(rows.data()),13,22};
    map.bits[0]=8191;map.bitCount=13;map.bitCapacity=128;
    std::vector<Region> regions={{map.slots.data,sizeof(rows)},{reinterpret_cast<uintptr_t>(&list),sizeof(list)},
                                 {list.data,sizeof(listeners)}};
    auto copy=[&](void* dst,uintptr_t src,size_t bytes) {
        for(auto r:regions)if(src>=r.address&&src-r.address<=r.size&&bytes<=r.size-(src-r.address)) {
            std::memcpy(dst,reinterpret_cast<void*>(src),bytes);return true;
        }
        return false;
    };
    auto mask=[](lis::engine::Name key)->unsigned {
        if(key.number)return 0;
        return key.index==0?15:(key.index<=4?1u<<(key.index-1):0);
    };
    bool active=true,acceptSecond=false;
    auto candidate=[&](ChoiceListener l){return active&&l.interface==l.object+0x80&&
        (l.object==owner||(acceptSecond&&l.object==123456));};
    RegisteredChoices found;
    auto resolve=[&]{return ReadRegisteredChoices(map,copy,mask,candidate,found);};
    check(resolve()&&found.owner==owner&&found.slots==15,"four registrations resolve to one owner, ignoring unrelated listeners");
    active=false;
    check(resolve()&&!found.owner,"inactive/unready owner cannot authorize the visible widget");
    active=true;acceptSecond=true;
    check(!resolve()&&!found.owner,"two active owners fail closed with no partial result");
    acceptSecond=false;listeners[0].interface=owner+0x26c;
    check(resolve()&&!found.owner,"actor interface cannot masquerade as the scripted interface");
    listeners[0].interface=owner+0x80;
    map.bits[0]=8190;map.freeCount=1;rows[0].listeners=1;
    check(resolve()&&found.slots==14,"free watcher row is not dereferenced or registered");
    map.freeCount=0;
    check(!resolve(),"watcher allocation-count mismatch rejected");
    map.freeCount=1;map.heapBits=1;
    check(!resolve(),"unverified heap allocation bitmap rejected");
    map.heapBits=0;rows[1].listeners=1;
    check(!resolve()&&!found.owner,"unreadable watcher list fails closed");
    rows[1].listeners=reinterpret_cast<uintptr_t>(&list);list.count=257;list.capacity=257;
    check(!resolve(),"oversized listener list rejected before reading entries");
    list.count=2;list.capacity=2;
    rows[1].key={0,0};
    check(resolve()&&found.slots==15,"native wildcard registration covers all choice bindings");
    return errors?1:0;
}
