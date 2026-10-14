#include "features/world/menu_state.h"
#include "features/dialogue/policy.h"
#include "features/dialogue/bindings.h"
#include <iostream>
#include <map>
#include <string>

struct Object {const char* type;bool registered;uint32_t flags;unsigned char opening;bool readable=true;};
struct Registry {
    std::map<uintptr_t,Object> objects;
    bool Instance(uintptr_t p) const {
        auto i=objects.find(p);return i!=objects.end()&&i->second.registered&&!(i->second.flags&0x600);
    }
    bool IsA(uintptr_t p,const char* base) const {
        auto i=objects.find(p);if(i==objects.end()||std::string(base)!="SensenMenuBase")return false;
        const std::string type=i->second.type;
        return type=="SensenMenu"||type=="PauseMenu"||type=="OutGameSensenMenu";
    }
};
struct Reflection {
    Registry registry;mutable int reads=0;
    bool Value(uintptr_t p,const char* owner,const char* name,const char* type,unsigned char& out) const {
        ++reads;auto i=registry.objects.find(p);
        if(i==registry.objects.end()||!i->second.readable||std::string(owner)!="SensenMenuBase"||
           std::string(name)!="OpeningState"||std::string(type)!="ByteProperty")return false;
        out=i->second.opening;return true;
    }
};
int main() {
    int errors=0;auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS: ":"FAIL: ")<<label<<'\n';errors+=!ok;};
    // Passive capture from the failing Alyssa conversation, PID7680, build2304417.
    // The mod's cached menu vector contained this registered scenery object:
    // e1_3c_vicroom_ld.TheWorld.PersistentLevel.InterpActor_34. Reading the old
    // menu offset gave128, even though the object was no longer a menu.
    constexpr uintptr_t recycled=957860096,closed=1077138688,pause=1387205632;
    Reflection f;f.registry.objects.emplace(recycled,Object{"InterpActor",true,1048600,128});
    f.registry.objects.emplace(closed,Object{"SensenMenu",true,1048576,0});
    f.registry.objects.emplace(pause,Object{"PauseMenu",true,1048576,2});
    unsigned char oldOpening=0;
    check(f.registry.Instance(recycled)&&f.Value(recycled,"SensenMenuBase","OpeningState","ByteProperty",oldOpening)&&oldOpening==128,
        "captured recycled actor reproduces old false menu blocker");
    f.reads=0;
    check(!lis::world::BlockingMenu(f,recycled)&&f.reads==0,"recycled live object is rejected before reading a menu-only offset");
    check(!lis::world::BlockingMenu(f,closed),"closed genuine diary does not block choices");
    check(lis::world::BlockingMenu(f,pause),"open genuine pause menu still blocks choices");
    f.registry.objects.at(pause).opening=1;check(lis::world::BlockingMenu(f,pause),"opening menu blocks");
    f.registry.objects.at(pause).opening=3;check(lis::world::BlockingMenu(f,pause),"closing menu blocks");
    f.registry.objects.at(pause).readable=false;check(lis::world::BlockingMenu(f,pause),"unreadable genuine menu fails closed");
    f.registry.objects.at(pause).flags|=0x200;f.reads=0;
    check(!lis::world::BlockingMenu(f,pause)&&f.reads==0,"dying objects are not read as menus");
    check(!lis::world::BlockingMenu(f,0),"missing object is ignored");
    f.registry.objects.at(closed).type="InterpActor";f.registry.objects.at(closed).opening=128;f.reads=0;
    check(!lis::world::BlockingMenu(f,closed)&&f.reads==0,"class is revalidated after an address is reused");

    using namespace lis::dialogue;
    const BindingEntry captured[]={{2,0,-1,0},{1,1,0,0}};std::array<int,4> slots{};
    check(DecodeBindings(captured,2,3,0,slots)&&slots==std::array<int,4>{-1,1,0,-1},"Alyssa's captured bindings retain their native slots");
    Snapshot s;s.context=true;s.visible=true;s.identity="E1_3B_CampusB_CHAlyssa|page0";
    s.choices[1]={L"$GT_HUD_Dial_E1_3B_CampusB_CHAlyssa_MaxTalkAlyssa_001",L"How are you?",true};
    s.choices[2]={L"$GT_HUD_Dial_E1_3B_CampusB_CHAlyssa_MaxTalkAlyssa_000",L"What are you reading?",true};
    check(Describe(s)==L"2: How are you?\n3: What are you reading?","real Alyssa text uses existing dialogue narration without a special-case reader");
    check(CanActivate(s,s,1,true,lis::world::BlockingMenu(f,recycled)),"recycled menu no longer disables the real offered choice");
    f.registry.objects.at(pause)={"PauseMenu",true,1048576,2};
    check(!CanActivate(s,s,1,true,lis::world::BlockingMenu(f,pause)),"same choice remains blocked by a real pause menu");
    return errors?1:0;
}
