#pragma once
#include <cstdint>
namespace lis::engine {
// Verified on Steam build2304417: UObject RVA7b4b0 and AActor RVA2a1b00
// both use ECX for this and ret 0x0c for three stack arguments.
using ProcessEvent = void(__thiscall*)(void* object, void* function, void* parameters, void* legacyResult);
struct Name {uint32_t index=0,number=0;};
static_assert(sizeof(Name)==8);
// Verified at the live native consumer: interface this = actor+0x26c;
// FName by value uses two stack words, then an opaque source pointer (ret 0x0c).
using InteractionPress = uint32_t(__thiscall*)(void* listener,Name binding,void* source);
using ActionEligibility = int(__thiscall*)(void* actor,void* action); // ret 4
}
