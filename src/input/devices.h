#pragma once
#include "input/commands.h"
#include <Windows.h>
#include <Xinput.h>

namespace lis::input {
// Worker-thread hardware polling only. No engine objects or feature actions.
class Devices {
public:
    Devices()=default;
    Devices(const Devices&)=delete;
    Devices& operator=(const Devices&)=delete;
    void Initialize();
    void Shutdown();
    Presses ReadKeyboard(bool foreground,uint64_t now);
    Presses ReadController(int controllerIndex,bool active,uint64_t now,bool repeatAllowed=false);
private:
    using GetPadState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
    HMODULE module_=nullptr;
    GetPadState getPadState_=nullptr;
    Keyboard keyboard_;
    Controller controller_;
    int padIndex_=0, previousPadIndex_=-1;
    bool padWasConnected_=false;
};
}
