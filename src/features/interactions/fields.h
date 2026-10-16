#pragma once
#include "engine/abi.h"
#include <cmath>
#include <string>

namespace lis::interaction {
// Captured from build 2304417's reflected descriptors. ComponentProperty is
// pointer-sized but is NOT ObjectProperty; ObjectName is an FName, not FString.
// Keep the production reads shared with the engine-free descriptor fixtures.
template<class Reader>
bool ReadComponent(Reader read,uintptr_t controller,uintptr_t& component) {
    return read(controller,"WhatIfController","DNEWhatIfInteractionComp","ComponentProperty",component);
}
// Native interaction-zone classifier: positive FSM override, then positive
// authored actor override, otherwise component radius. This is an approach
// region, not proof of focus/action availability (which remains native UI state).
template<class Reader>
bool ReadApproachRadius(Reader read,uintptr_t actor,uintptr_t component,float& out) {
    out=0;float value=0;
    for(const auto* property:{"FSMOverrideInnerZoneRadius","OverrideInnerZoneRadius"}) {
        if(!read(actor,"WhatIfInteractionActor",property,"FloatProperty",value)||!std::isfinite(value))return false;
        if(value>0){out=value;return true;}
    }
    if(!read(component,"DNEWhatIfInteractionComponent","InnerZoneRadius","FloatProperty",value)||
       !std::isfinite(value)||value<=0)return false;
    out=value;return true;
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
