#pragma once
#include "features/prompts/controls.h"
#include "engine/reflection.h"

namespace lis::prompts {
bool ValidControlManager(const engine::Reflection& reflection,uintptr_t manager);
Controls ReadControls(const engine::Reflection& reflection,uintptr_t manager);
std::wstring LocalizedCaption(const engine::Reflection& reflection,uintptr_t base,
    uintptr_t localize,uintptr_t movie,const std::wstring& key);
}
