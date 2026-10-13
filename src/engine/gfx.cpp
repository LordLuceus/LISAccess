#include "engine/gfx.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include "engine/memory.h"
#include "engine/object_registry.h"
#include "platform/text.h"
#include <vector>

namespace lis::engine::gfx {
namespace {
uintptr_t base = 0, menu = 0, stringFn = 0, numberFn = 0, invokeFn = 0, objectStringFn = 0, boolFn = 0;
struct FString { wchar_t* data; int count; int capacity; };
static_assert(sizeof(FString) == 12);
ObjectRegistry Registry() { return ObjectRegistry(base, RegistryPolicy::Menu); }
std::string Name(int index) { return Registry().Name(index); }
std::string ObjectName(uintptr_t object) { return Registry().NameOf(object); }
std::string ClassName(uintptr_t object) { return Registry().ClassOf(object); }
bool Registered(uintptr_t object) { return Registry().Registered(object); }
bool Function(uintptr_t fn, const char* name, int size,const char* owner="GFxMoviePlayer") {
    return Registered(fn) && ClassName(fn) == "Function" && ObjectName(fn) == name &&
        ObjectName(Read<uintptr_t>(fn + 0x28)) == owner &&
        Read<unsigned short>(fn + 0xa2) == size && (Read<unsigned>(fn + 0x90) & 0x400);
}
// Both UObject and AActor dispatchers end in ret 0x0c: THREE stack arguments.
// The legacy result pointer is unused on this path but must still be supplied.
void Event(uintptr_t fn, void* args,uintptr_t receiver=0) {
    reinterpret_cast<engine::ProcessEvent>(base + profile::ObjectDispatch)(reinterpret_cast<void*>(receiver?receiver:menu), reinterpret_cast<void*>(fn), args, nullptr);
}
std::wstring TakeString(FString& value) {
    std::wstring text;
    if (value.data && value.count > 0 && value.count <= 8192 && value.capacity >= value.count) {
        text.resize(value.count - 1);
        if (!Copy(text.data(), reinterpret_cast<uintptr_t>(value.data), text.size()*2)) text.clear();
    }
    // This is the game's allocator wrapper, verified from FString destruction.
    if (value.data) reinterpret_cast<void(__cdecl*)(void*)>(base + profile::Free)(value.data);
    value = {};
    return text;
}
FString Borrow(std::wstring& s) { return {s.data(), static_cast<int>(s.size()+1), static_cast<int>(s.size()+1)}; }
}

void Initialize(uintptr_t executableBase) { base = executableBase; }
bool Bound() { return menu && stringFn && numberFn && invokeFn; }
bool RegisteredMenu() { return Registered(menu); }
unsigned char Opening() { return Read<unsigned char>(menu + 0x1f4); }
std::string DiscoveryState() {
    return "event=discovery_state menu=" + std::to_string(menu != 0) +
        " string=" + std::to_string(stringFn != 0) + " number=" + std::to_string(numberFn != 0) +
        " invoke=" + std::to_string(invokeFn != 0) + " opening=" + std::to_string(Opening());
}
void Discover() {
    Array a = Read<Array>(base + profile::Objects);
    if (!a.data || a.count < 100 || a.count > 1000000 || a.capacity < a.count || Name(0) != "None" || Name(2) != "IntProperty") return;
    std::vector<uintptr_t> objects(a.count);
    if (!Copy(objects.data(), a.data, objects.size() * 4)) return;
    for (uintptr_t obj : objects) {
        if (!obj) continue;
        const std::string cls = ClassName(obj);
        if(cls=="Function" && Function(obj,"GetString",24,"GFxObject"))objectStringFn=obj;
        if (cls == "Function" && ObjectName(Read<uintptr_t>(obj + 0x28)) == "GFxMoviePlayer") {
            std::string name = ObjectName(obj);
            if (name == "GetVariableString" && Function(obj, name.c_str(), 24)) stringFn = obj;
            if (name == "GetVariableNumber" && Function(obj, name.c_str(), 16)) numberFn = obj;
            if (name == "GetVariableBool" && Function(obj, name.c_str(), 16)) boolFn = obj;
            if (name == "Invoke" && Function(obj, name.c_str(), 48)) invokeFn = obj;
        }
        if (cls == "OutGameSensenMenu" && ObjectName(obj).find("Default__") != 0 &&
            Read<uintptr_t>(Read<uintptr_t>(obj) + 72 * 4) == base + profile::ObjectDispatch) menu = obj;
    }
}
std::wstring String(const std::string& path) {
    return String(menu,path);
}
std::wstring String(uintptr_t receiver,const std::string& path) {
    std::wstring input = Wide(path);
    struct Params { FString path; FString result; } p{Borrow(input), {}};
    Event(stringFn, &p,receiver);
    return TakeString(p.result);
}
float Number(const std::string& path) {
    return Number(menu,path);
}
float Number(uintptr_t receiver,const std::string& path) {
    std::wstring input = Wide(path);
    struct Params { FString path; float result; } p{Borrow(input), 0};
    Event(numberFn, &p,receiver); return p.result;
}
bool ValidMovie(uintptr_t receiver) {
    return Registry().Instance(receiver) && Registry().IsA(receiver,"GFxMoviePlayer") &&
        Read<uintptr_t>(Read<uintptr_t>(receiver)+72*4)==base+profile::ObjectDispatch &&
        Function(stringFn,"GetVariableString",24) && Function(numberFn,"GetVariableNumber",16);
}
bool ObjectQueriesReady(){return Function(objectStringFn,"GetString",24,"GFxObject");}
std::wstring DisplayString(const std::string& path){return DisplayString(menu,path);}
std::wstring DisplayString(uintptr_t receiver,const std::string& path) {
    auto ends=[&](const std::string& suffix){return path.size()>=suffix.size()&&path.compare(path.size()-suffix.size(),suffix.size(),suffix)==0;};
    if(ends("._alpha"))return std::to_wstring(Number(receiver,path));
    if(ends("._visible")||ends(".shadow_text")) {
        if(!Function(boolFn,"GetVariableBool",16))return {};
        std::wstring input=Wide(path);
        struct Params {FString path;uint32_t result;} p{Borrow(input),0};
        static_assert(sizeof(Params)==16);
        Event(boolFn,&p,receiver);
        return p.result?L"true":L"false";
    }
    return String(receiver,path);
}
std::wstring ObjectString(uintptr_t object,const std::string& member) {
    if(!ObjectQueriesReady() || !Registry().Instance(object) || !Registry().IsA(object,"GFxObject") ||
        Read<uintptr_t>(Read<uintptr_t>(object)+72*4)!=base+profile::ObjectDispatch)return {};
    std::wstring input=Wide(member);
    struct Params {FString member;FString result;} p{Borrow(input),{}};
    Event(objectStringFn,&p,object);
    return TakeString(p.result);
}
std::wstring Text(const std::string& objectPath) {
    std::wstring method = Wide(objectPath + ".GetText");
    struct Value { unsigned type; unsigned boolean; float number; FString text; };
    struct Params { FString method; Array args; Value result; } p{Borrow(method), {}, {}};
    static_assert(sizeof(Params) == 48);
    Event(invokeFn, &p);
    return TakeString(p.result.text);
}
}
