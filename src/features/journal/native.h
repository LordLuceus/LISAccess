#pragma once
#include "features/journal/reader.h"
#include <cstdint>
namespace lis::journal {
struct Frame {
    bool open=false,blocked=false,gamepad=false,tabs=false;
    int controllerIndex=-1;
    uintptr_t movie=0;
    std::string identity,root,reason;
    Page page;
};
bool SampleNative(Frame& frame);
bool StillOpen(uintptr_t movie); // game-thread only; no GFx invocation
std::wstring ChangeTab(const Frame& expected,int direction);
}
