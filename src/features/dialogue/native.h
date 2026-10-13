#pragma once
#include "features/dialogue/policy.h"
namespace lis::dialogue {
struct NativeFrame {
    Snapshot view;
    uintptr_t actor=0;
    bool blocked=true;
    std::string reason="initializing";
    std::wstring problem;
};
bool SampleNative(NativeFrame& frame); // game thread, independent SEH boundary
std::wstring ActivateNative(const Snapshot& expected,unsigned slot); // explicit user request only
}
