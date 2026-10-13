#pragma once
#include "engine/object_registry.h"
#include <unordered_map>
namespace lis::engine {
class Localization {
public:
    std::wstring Translate(const ObjectRegistry& registry,uintptr_t base,uintptr_t localize,uintptr_t controller,const std::wstring& key,const wchar_t* unavailable=L"Unnamed object");
private:
    std::unordered_map<std::wstring,std::wstring> translations;
};
std::wstring WideName(const ObjectRegistry& registry,uint32_t index);
}
