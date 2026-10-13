#include "input/devices.h"

namespace lis::input {
void Devices::Initialize() {
    for(const wchar_t* library:{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"}) {
        module_=LoadLibraryExW(library,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module_)continue;
        getPadState_=reinterpret_cast<GetPadState>(GetProcAddress(module_,"XInputGetState"));
        if(getPadState_)break;
        FreeLibrary(module_);module_=nullptr;
    }
}
void Devices::Shutdown() {
    if(module_){FreeLibrary(module_);module_=nullptr;getPadState_=nullptr;}
}
Presses Devices::ReadKeyboard(bool foreground,uint64_t now) {
    Presses raw{};
    for(size_t i=0;i<Count;++i)raw[i]=(GetAsyncKeyState(Keys[i])&0x8000)!=0;
    const Modifiers modifiers{(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0,(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0,(GetAsyncKeyState(VK_MENU)&0x8000)!=0};
    return keyboard_.Update(raw,modifiers,foreground,now);
}
Presses Devices::ReadController(int controllerIndex,bool active,uint64_t now) {
    if(controllerIndex>=0&&controllerIndex<4)padIndex_=controllerIndex;
    XINPUT_STATE pad{};
    const bool connected=getPadState_&&getPadState_(static_cast<DWORD>(padIndex_),&pad)==ERROR_SUCCESS;
    if(connected&&(!padWasConnected_||previousPadIndex_!=padIndex_))controller_.Update(pad.Gamepad.wButtons,false,now);
    const auto presses=controller_.Update(connected?pad.Gamepad.wButtons:0,active&&connected,now);
    padWasConnected_=connected;previousPadIndex_=padIndex_;
    return presses;
}
}
