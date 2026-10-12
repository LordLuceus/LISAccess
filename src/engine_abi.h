#pragma once
namespace lis::engine {
// Verified on Steam build2304417: UObject RVA7b4b0 and AActor RVA2a1b00
// both use ECX for this and ret 0x0c for three stack arguments.
using ProcessEvent = void(__thiscall*)(void* object, void* function, void* parameters, void* legacyResult);
}
