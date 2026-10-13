#include "engine/localization.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include <Windows.h>
namespace lis::engine {
std::wstring WideName(const ObjectRegistry& registry,uint32_t index) {
    const std::string name=registry.Name(static_cast<int>(index));
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),static_cast<int>(name.size()),nullptr,0);
    if(count<=0)return {};
    std::wstring result(static_cast<size_t>(count),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),static_cast<int>(name.size()),result.data(),count);
    return result;
}
struct String {uintptr_t data=0;int count=0,capacity=0;};
std::wstring Localization::Translate(const ObjectRegistry& registry,uintptr_t base,uintptr_t localize,uintptr_t controller,const std::wstring& key,const wchar_t* unavailable) {
    if(key.rfind(L"$GT_",0)!=0)return key;
    auto cached=translations.find(key);if(cached!=translations.end())return cached->second;
    const auto a=key.find(L'_',4),b=a==std::wstring::npos?a:key.find(L'_',a+1);
    if(b==std::wstring::npos || !registry.Registered(localize) || Read<uint16_t>(localize+0xa2)!=48 || Read<uint16_t>(localize+0xa4)!=36 || Read<uint16_t>(localize+0x94)!=0) return unavailable;
    std::wstring package=key.substr(4,a-4),section=key.substr(a+1,b-a-1),item=key.substr(b+1);
    auto borrow=[](std::wstring& text){return String{reinterpret_cast<uintptr_t>(text.data()),static_cast<int>(text.size()+1),static_cast<int>(text.size()+1)};};
    struct Params {String section,key,package,result;} p{borrow(section),borrow(item),borrow(package),{}};
    static_assert(sizeof(Params)==48);
    reinterpret_cast<engine::ProcessEvent>(base+profile::ActorDispatch)(reinterpret_cast<void*>(controller),reinterpret_cast<void*>(localize),&p,nullptr);
    std::wstring result;
    if(p.result.data && p.result.count>0 && p.result.count<=8192 && p.result.capacity>=p.result.count) {
        result.resize(p.result.count-1);
        if(!Copy(result.data(),p.result.data,result.size()*sizeof(wchar_t)))result.clear();
    }
    if(p.result.data)reinterpret_cast<void(__cdecl*)(void*)>(base+profile::Free)(reinterpret_cast<void*>(p.result.data));
    if(result.empty() || result.rfind(L"$GT_",0)==0)return unavailable;
    if(translations.size()<2048)translations.emplace(key,result);
    return result;
}
}
