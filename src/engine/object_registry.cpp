#include "engine/object_registry.h"
#include "engine/build_profile.h"

namespace lis::engine {
bool ObjectRegistry::Registry(Array& a) const {
    return Copy(&a,base_+profile::Objects,sizeof(a)) && a.data>=0x10000 &&
        a.count>100 && a.count<=250000 && a.capacity>=a.count;
}
bool ObjectRegistry::Registered(uintptr_t object) const {
    if(policy_==RegistryPolicy::Menu) {
        const Array a=Read<Array>(base_+profile::Objects);
        const int index=Read<int>(object+0x20);
        return object && index>=0 && index<a.count && a.count<1000000 && Read<uintptr_t>(a.data+index*4)==object;
    }
    Array a{};int index=-1;
    return object && Registry(a) && Copy(&index,object+0x20,4) && index>=0 &&
        index<a.count && Read<uintptr_t>(a.data+index*4)==object;
}
std::string ObjectRegistry::Name(int index) const {
    const Array a=Read<Array>(base_+profile::Names);
    if(index<0 || index>=a.count || a.count>1000000 ||
        (policy_==RegistryPolicy::World && a.capacity<a.count))return {};
    char text[192]{};
    if(!Copy(text,Read<uintptr_t>(a.data+index*4)+8,sizeof(text)-1))return {};
    return text;
}
std::string ObjectRegistry::NameOf(uintptr_t object) const {return Name(Read<int>(object+0x2c));}
std::string ObjectRegistry::ClassOf(uintptr_t object) const {return NameOf(Read<uintptr_t>(object+0x34));}
bool ObjectRegistry::Instance(uintptr_t object) const {
    uint32_t flags=0;
    return Registered(object) && Copy(&flags,object+8,4) && !(flags&(0x200|0x400)) && NameOf(object).find("Default__")!=0;
}
bool ObjectRegistry::IsA(uintptr_t object,const char* type) const {
    uintptr_t c=Read<uintptr_t>(object+0x34);
    for(int n=0;c && n<64;++n,c=Read<uintptr_t>(c+0x48))
        if(Registered(c) && NameOf(c)==type)return true;
    return false;
}
std::string ObjectRegistry::Path(uintptr_t object) const {
    std::string result;
    for(int n=0;object && n<16;++n,object=Read<uintptr_t>(object+0x28)) {
        if(!Registered(object))return {};
        std::string part=NameOf(object);
        const int number=Read<int>(object+0x30);
        if(number>0)part+="_"+std::to_string(number-1);
        result=part+(result.empty()?"":"."+result);
    }
    return result;
}
}
