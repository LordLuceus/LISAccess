#pragma once
#include "engine_abi.h"
#include <string>

namespace lis::interaction {
// Captured from build 2304417's reflected descriptors. ComponentProperty is
// pointer-sized but is NOT ObjectProperty; ObjectName is an FName, not FString.
// Keep the production reads shared with the engine-free descriptor fixtures.
template<class Reader>
bool ReadComponent(Reader read,uintptr_t controller,uintptr_t& component) {
    return read(controller,"WhatIfController","DNEWhatIfInteractionComp","ComponentProperty",component);
}
template<class Reader,class ResolveName>
std::wstring ReadObjectLabel(Reader read,ResolveName resolve,uintptr_t actor) {
    engine::Name value{};
    if(!read(actor,"WhatIfInteractionActor","ObjectName","NameProperty",value)||!value.index)return {};
    std::wstring text=resolve(value.index);
    if(!text.empty()&&value.number)text+=L"_"+std::to_wstring(value.number-1);
    return text;
}
}
