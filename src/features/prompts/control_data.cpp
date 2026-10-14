#include "features/prompts/control_data.h"
#include "engine/localization.h"
#include "engine/abi.h"
#include <Windows.h>

namespace lis::prompts {
bool ValidControlManager(const engine::Reflection& f,uintptr_t manager) {
    return f.registry.Instance(manager)&&f.registry.ClassOf(manager)=="DynamicInputSwitchManager";
}
Controls ReadControls(const engine::Reflection& f,uintptr_t manager) {
    Controls out;unsigned char mode=255;
    if(!ValidControlManager(f,manager)||!f.Value(manager,"DynamicInputSwitchManager","CurrentControlMode","ByteProperty",mode)||mode>1)return out;
    out.mode=mode;if(mode!=1)return out;
    engine::Array rows{};int nameOffset=-1,textOffset=-1;
    const auto field=f.Field("DynamicInputSwitchManager","GamepadActionList","ArrayProperty",12);
    const auto inner=engine::Read<uintptr_t>(field+0x80);
    // Reflected GamepadActionDescription: FName@0 + FString@8, stride20.
    if(!field||!f.registry.Registered(inner)||f.registry.ClassOf(inner)!="StructProperty"||engine::Read<int>(inner+0x44)!=20||
       !f.Offset("GamepadActionDescription","ActionName","NameProperty",8,nameOffset)||nameOffset!=0||
       !f.Offset("GamepadActionDescription","Description","StrProperty",12,textOffset)||textOffset!=8||
       !f.Value(manager,"DynamicInputSwitchManager","GamepadActionList","ArrayProperty",rows)||
       rows.count<1||rows.count>128||rows.capacity<rows.count||rows.data<0x10000)return out;
    for(int i=0;i<rows.count;++i) {
        const auto row=rows.data+i*20;engine::Name action{};engine::Array text{};
        if(!f.Value(row,"GamepadActionDescription","ActionName","NameProperty",action)||action.number||
           !f.Value(row,"GamepadActionDescription","Description","StrProperty",text)||text.count<1||text.count>512||
           text.capacity<text.count||text.data<0x10000)return out;
        auto name=ControlKey(engine::WideName(f.registry,action.index));
        std::wstring description(static_cast<size_t>(text.count-1),L'\0');
        if(name.empty()||!engine::Copy(description.data(),text.data,description.size()*sizeof(wchar_t))||
           !out.actions.emplace(name,description).second)return out;
    }
    out.ready=true;return out;
}
namespace {
void Translate(const engine::Reflection& f,uintptr_t base,uintptr_t localize,uintptr_t movie,
    const std::wstring& key,std::wstring& out) {
    // Fresh translation: changing the game's language must not reuse old prose.
    engine::Localization translator;out=translator.Translate(f.registry,base,localize,movie,key,L"");
}
bool SafeTranslate(const engine::Reflection& f,uintptr_t base,uintptr_t localize,uintptr_t movie,
    const std::wstring& key,std::wstring& out) {
    __try {Translate(f,base,localize,movie,key,out);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
std::wstring LocalizedCaption(const engine::Reflection& f,uintptr_t base,uintptr_t localize,
    uintptr_t movie,const std::wstring& key) {
    std::wstring text;
    if(!SafeTranslate(f,base,localize,movie,key,text))return {};
    return text;
}
}
