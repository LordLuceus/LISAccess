#include "features/interactions/fields.h"
#include <cstring>
#include <iostream>
#include <limits>

namespace {
// Minimal descriptor/value fixtures from the captured classroom metadata, not
// guessed layouts. No executable/game assets or live process are required.
struct Reader {
    bool available=true;
    lis::engine::Name objectName{17,0};
    template<class T> bool operator()(uintptr_t object,const char* owner,const char* name,const char* type,T& out) const {
        if(!available)return false;
        const uintptr_t component=0x345600;
        if(object==0x10000&&std::strcmp(owner,"WhatIfController")==0&&std::strcmp(name,"DNEWhatIfInteractionComp")==0 &&
            std::strcmp(type,"ComponentProperty")==0&&sizeof(T)==4) {std::memcpy(&out,&component,4);return true;}
        if(object==0x20000&&std::strcmp(owner,"WhatIfInteractionActor")==0&&std::strcmp(name,"ObjectName")==0 &&
            std::strcmp(type,"NameProperty")==0&&sizeof(T)==8) {std::memcpy(&out,&objectName,8);return true;}
        return false;
    }
};
auto resolve=[](uint32_t index)->std::wstring{return index==17 ? L"$GT_Sensen_Int_Photo01" : L"";};
}
int main() {
    static_assert(sizeof(uintptr_t)==4&&sizeof(lis::engine::Name)==8);
    Reader reader;uintptr_t component=0;
    if(!lis::interaction::ReadComponent(reader,0x10000,component)||component!=0x345600)return 1;
    if(lis::interaction::ReadObjectLabel(reader,resolve,0x20000)!=L"$GT_Sensen_Int_Photo01")return 2;
    // Reproduce BOTH old mistakes against the same fixture: each must reject.
    if(reader(0x10000,"WhatIfController","DNEWhatIfInteractionComp","ObjectProperty",component))return 3;
    struct String {uint32_t data,count,capacity;} string{};
    if(reader(0x20000,"WhatIfInteractionActor","ObjectName","StrProperty",string))return 4;
    reader.objectName.number=3;
    if(lis::interaction::ReadObjectLabel(reader,resolve,0x20000)!=L"$GT_Sensen_Int_Photo01_2")return 5;
    reader.objectName={0,0};
    if(!lis::interaction::ReadObjectLabel(reader,resolve,0x20000).empty())return 6;
    reader.available=false;
    if(lis::interaction::ReadComponent(reader,0x10000,component)||!lis::interaction::ReadObjectLabel(reader,resolve,0x20000).empty())return 7;
    // Real shipped Mail_63 / AnsweringMachine_79 overrides are 100, not the
    // discarded universal 72. Remaining values below exercise precedence.
    float fsm=-1,authored=100,defaultRadius=80,radius=999;const char* missing="";
    auto zone=[&](uintptr_t object,const char* owner,const char* name,const char* type,float& out) {
        if(std::strcmp(type,"FloatProperty")||!std::strcmp(name,missing))return false;
        if(object==1&&!std::strcmp(owner,"WhatIfInteractionActor")) {
            if(!std::strcmp(name,"FSMOverrideInnerZoneRadius")){out=fsm;return true;}
            if(!std::strcmp(name,"OverrideInnerZoneRadius")){out=authored;return true;}
        }
        if(object==2&&!std::strcmp(owner,"DNEWhatIfInteractionComponent")&&!std::strcmp(name,"InnerZoneRadius")){out=defaultRadius;return true;}
        return false;
    };
    auto readZone=[&]{return lis::interaction::ReadApproachRadius(zone,1,2,radius);};
    if(!readZone()||radius!=100)return 8;
    fsm=120;missing="OverrideInnerZoneRadius";
    if(!readZone()||radius!=120)return 9; // overridden inputs are not required
    missing="";fsm=0;authored=-1;
    if(!readZone()||radius!=80)return 10;
    defaultRadius=0;
    if(readZone()||radius!=0)return 11;
    defaultRadius=80;missing="FSMOverrideInnerZoneRadius";
    if(readZone()||radius!=0)return 12;
    missing="";fsm=std::numeric_limits<float>::quiet_NaN();
    if(readZone()||radius!=0)return 13;
    std::cout<<"PASS: captured component/FName contracts, both old-type regressions, suffix and unavailable reads\n";
}
