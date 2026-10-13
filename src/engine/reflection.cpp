#include "engine/reflection.h"
namespace lis::engine {
uintptr_t Reflection::Field(const char* owner, const char* name, const char* type, int size) const {
    auto it = fields.find(std::string(owner) + "." + name);
    if (it == fields.end()) return 0;
    uintptr_t p = it->second;
    if (!registry.Registered(p) || registry.ClassOf(p) != type || registry.NameOf(p)!=name || registry.NameOf(Read<uintptr_t>(p+0x28))!=owner || Read<int>(p + 0x40) != 1 || Read<int>(p + 0x44) != size) return 0;
    int offset = Read<int>(p + 0x60);
    return offset >= 0 && offset < 0x10000 ? p : 0;
}
bool Reflection::Offset(const char* owner, const char* name, const char* type, int size, int& offset) const {
    uintptr_t f = Field(owner, name, type, size);
    if (!f) return false;
    offset = Read<int>(f + 0x60); return true;
}
bool Reflection::ClearBool(uintptr_t object, const char* owner, const char* name) const {
    uintptr_t f = Field(owner, name, "BoolProperty", 4);
    uint32_t value = 0, mask = Read<uint32_t>(f + 0x80);
    return f && mask && Copy(&value, object + Read<int>(f + 0x60), 4) && !(value & mask);
}
bool Reflection::BoolIsSet(uintptr_t object,const char* owner,const char* name) const {
    uintptr_t f=Field(owner,name,"BoolProperty",4);
    uint32_t value=0,mask=Read<uint32_t>(f+0x80);
    return f&&mask&&Copy(&value,object+Read<int>(f+0x60),4)&&(value&mask);
}
struct String {uintptr_t data=0;int count=0,capacity=0;};
std::wstring Reflection::Text(uintptr_t object,const char* owner,const char* key) const {
    String value{};
    if(!Value(object,owner,key,"StrProperty",value) || !value.data || value.count<1 || value.count>8192 || value.capacity<value.count) return {};
    std::wstring out(static_cast<size_t>(value.count-1),L'\0');
    return Copy(out.data(),value.data,out.size()*sizeof(wchar_t)) ? out : std::wstring{};
}
void Reflection::Observe(uintptr_t p,const std::string& cls,const std::string& name) {
    if(cls.find("Property")!=std::string::npos&&fields.size()<50000)
        fields[registry.NameOf(Read<uintptr_t>(p+0x28))+"."+name]=p;
}
bool Reflection::StructOffset(const char* owner,const char* name,int& out) const {
    auto it=fields.find(std::string(owner)+"."+name);
    if(it==fields.end())return false;
    int size=Read<int>(it->second+0x44);
    return size>=12&&size<4096&&Offset(owner,name,"StructProperty",size,out);
}
}
