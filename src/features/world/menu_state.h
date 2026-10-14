#pragma once
#include <cstdint>

namespace lis::world {
// Cached UObject addresses can be recycled into a different live class. Being
// registered is not proof that a former menu still owns an OpeningState field.
template<class Registry>
bool IsMenuInstance(const Registry& registry,uintptr_t object) {
    return registry.Instance(object)&&registry.IsA(object,"SensenMenuBase");
}
template<class Reflection>
bool BlockingMenu(const Reflection& reflection,uintptr_t object) {
    if(!IsMenuInstance(reflection.registry,object))return false;
    unsigned char opening=0;
    // A real menu with unreadable state must still fail closed.
    return !reflection.Value(object,"SensenMenuBase","OpeningState","ByteProperty",opening)||opening!=0;
}
}
