#include "engine/object_registry.h"
#include "engine/build_profile.h"
#include "engine/reflection.h"
#include <Windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <type_traits>

namespace {
int failures=0;
void Check(bool value,const char* name) {
    std::cout<<(value?"PASS: ":"FAIL: ")<<name<<'\n';
    if(!value)++failures;
}
template<class T,size_t N> void Put(std::array<unsigned char,N>& bytes,size_t offset,T value) {
    std::memcpy(bytes.data()+offset,&value,sizeof(value));
}
}
int main() {
    using namespace lis::engine;
    static_assert(!std::is_copy_constructible_v<Reflection>,"Do not copy the metadata cache per target");
    // All addresses below point into this test process, never a running game.
    // Reserve the real profile span so the production RVA calculations are tested.
    auto* image=static_cast<unsigned char*>(VirtualAlloc(nullptr,profile::Names+sizeof(Array),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!image)return 1;
    const auto base=reinterpret_cast<uintptr_t>(image);
    std::array<unsigned char,0x70> object{},klass{};
    std::array<unsigned char,200> none{},fixture{};
    std::memcpy(none.data()+8,"None",5);std::memcpy(fixture.data()+8,"Fixture",8);
    std::array<uintptr_t,2> names{reinterpret_cast<uintptr_t>(none.data()),reinterpret_cast<uintptr_t>(fixture.data())};
    std::array<uintptr_t,101> objects{};
    const auto actor=reinterpret_cast<uintptr_t>(object.data());
    const auto type=reinterpret_cast<uintptr_t>(klass.data());
    objects[0]=actor;objects[1]=type;
    Put(object,0x20,0);Put(object,0x2c,1);Put(object,0x34,type);
    Put(klass,0x20,1);Put(klass,0x2c,1);
    auto& nameArray=*reinterpret_cast<Array*>(image+profile::Names);
    auto& objectArray=*reinterpret_cast<Array*>(image+profile::Objects);
    nameArray={reinterpret_cast<uintptr_t>(names.data()),2,2};
    objectArray={reinterpret_cast<uintptr_t>(objects.data()),101,101};
    ObjectRegistry menu(base,RegistryPolicy::Menu),world(base,RegistryPolicy::World);
    Check(menu.Registered(actor)&&world.Registered(actor),"both readers accept a registered fixture");
    Check(menu.NameOf(actor)=="Fixture"&&world.ClassOf(actor)=="Fixture","shared name and class lookup");
    Check(world.Instance(actor)&&world.IsA(actor,"Fixture")&&!world.IsA(actor,"Other"),"instance and ancestry checks");
    Put(object,0x30,3);
    Check(world.Path(actor)=="Fixture_2","native instance number suffix retained");
    Put(object,8,uint32_t{0x200});Check(!world.Instance(actor),"excluded object flags retained");Put(object,8,uint32_t{0});
    objectArray.count=100;
    Check(menu.Registered(actor)&&!world.Registered(actor),"world discovery minimum is not imposed on menus");
    objectArray.count=101;objectArray.capacity=100;
    Check(menu.Registered(actor)&&!world.Registered(actor),"world capacity guard retained without changing menus");
    objectArray.count=250000;objectArray.capacity=250000;
    Check(world.Registered(actor),"world upper bound inclusive");
    objectArray.count=250001;objectArray.capacity=250001;
    Check(menu.Registered(actor)&&!world.Registered(actor),"distinct registry size ceilings retained");
    objectArray.count=1000000;objectArray.capacity=1000000;
    Check(!menu.Registered(actor),"menu upper bound exclusive");
    objectArray.count=objectArray.capacity=101;
    Put(object,0x20,-1);Check(!menu.Registered(actor)&&!world.Registered(actor),"negative registry index rejected");Put(object,0x20,0);
    nameArray.capacity=1;
    Check(menu.Name(1)=="Fixture"&&world.Name(1).empty(),"different original name-array guards retained");
    Check(world.Name(-1).empty()&&menu.Name(2).empty(),"out-of-range name indexes rejected");
    uint32_t sentinel=42;
    Check(!Copy(&sentinel,0,sizeof(sentinel))&&sentinel==42&&Read<uint32_t>(0)==0,"low-address reads fail safely");
    Check(!Copy(&sentinel,actor,1024*1024+1),"oversized copies rejected before accessing destination");
    VirtualFree(image,0,MEM_RELEASE);
    return failures?1:0;
}
