#pragma once
#include "engine/object_registry.h"
#include <unordered_map>
namespace lis::engine {
class Reflection {
public:
    explicit Reflection(uintptr_t base=0):registry(base,RegistryPolicy::World){}
    // A game-thread cache, not a value to copy once per scanner target.
    Reflection(const Reflection&)=delete;
    Reflection& operator=(const Reflection&)=delete;
    ObjectRegistry registry;
    void Observe(uintptr_t object,const std::string& cls,const std::string& name);
    void Reset() { fields.clear(); }
    uintptr_t Field(const char* owner,const char* name,const char* type,int size) const;
    bool Offset(const char* owner,const char* name,const char* type,int size,int& offset) const;
    bool StructOffset(const char* owner,const char* name,int& offset) const;
    template<class T> bool Value(uintptr_t object,const char* owner,const char* name,const char* type,T& value) const {
        int offset=0;
        return Offset(owner,name,type,sizeof(T),offset)&&Copy(&value,object+offset,sizeof(T));
    }
    template<class T> bool operator()(uintptr_t object,const char* owner,const char* name,const char* type,T& value) const {
        return Value(object,owner,name,type,value);
    }
    bool ClearBool(uintptr_t object,const char* owner,const char* name) const;
    bool BoolIsSet(uintptr_t object,const char* owner,const char* name) const;
    std::wstring Text(uintptr_t object,const char* owner,const char* key) const;
private:
    std::unordered_map<std::string,uintptr_t> fields;
};
}
