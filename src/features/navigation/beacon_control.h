#pragma once
#include "features/navigation/guided_beacon.h"
namespace lis::navigation {
struct BeaconOutput {
    bool audible=false;
    float bearing=0,distance=0,radius=0;
    std::wstring speech;
};
// Worker-owned UI/audio policy. All routing and collision work lives on the
// game thread; this class consumes copied results only.
class BeaconControl {
    bool enabled=false,guided=true,announcedReady=false,directFallback=false;
    std::string selected;
    uint64_t started=0,lastReplanSpeech=0,fallbackAt=0;
    route::Status last=route::Status::Off;
public:
    void Configure(bool value){guided=value;}
    bool Guided() const{return guided;}
    bool Enabled() const{return enabled;}
    void Stop();
    void Retarget(const world::Target&,uint64_t now);
    std::wstring Toggle(const world::Target&,uint64_t now);
    BeaconOutput Update(const world::Snapshot&,const world::Target&,uint64_t now);
};
}
