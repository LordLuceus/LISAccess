#pragma once
#include "features/dialogue/policy.h"
namespace lis::dialogue {
struct NativeFrame {
    Snapshot view;
    uintptr_t owner=0; // WhatIfDialogActor or registered IA_Dialogue UObject.
    bool scripted=false;
    bool blocked=true;
    std::string reason="initializing";
    std::wstring problem;
};
bool SampleNative(NativeFrame& frame); // game thread, independent SEH boundary
std::wstring ActivateNative(const Snapshot& expected,unsigned slot); // explicit user request only
}
