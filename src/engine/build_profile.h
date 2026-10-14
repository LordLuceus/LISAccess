#pragma once
#include <cstdint>

namespace lis::engine::profile {
// RVAs for SHA-256-verified Steam build 2304417, never absolute process addresses.
inline constexpr uintptr_t Names=0x11e43b4, Objects=0x11d0598;
inline constexpr uintptr_t ObjectDispatch=0x7b4b0, ActorDispatch=0x2a1b00;
inline constexpr uintptr_t Free=0x259e0;
inline constexpr uintptr_t ActionEligibility=0x9f6a40, InteractionPress=0x9918b0;
inline constexpr uintptr_t DialogPress=0x991600;
// IA_InputBox interface at IA_Dialogue+0x80; distinct from the actor interface.
inline constexpr uintptr_t ScriptedDialogPress=0x991f70, ScriptedDialogInterface=0x102f274;
inline constexpr uintptr_t ChoiceBindings=0x1236d20;
}
