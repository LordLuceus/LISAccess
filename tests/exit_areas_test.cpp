#include "features/exits/policy.h"
#include "features/world/target_list.h"
#include "features/navigation/description.h"
#include <iostream>
#include <limits>
using namespace lis;
int main() {
    int errors=0;auto check=[&](bool ok,const char* why){std::cout<<(ok?"PASS: ":"FAIL: ")<<why<<'\n';errors+=!ok;};
    const std::wstring label=exits::DestinationLabel(L"[action_interact] Go To The Main Campus");
    check(label==L"Go To The Main Campus","actual HUD.Int.GoToCampusA becomes a destination, not a remote input instruction");
    check(exits::DestinationLabel(L" Vers le campus [action_interact] ")==L"Vers le campus","localized prose is retained regardless of token position");
    check(exits::DestinationLabel(L"[action_interact]").empty()&&exits::DestinationLabel(L"[action_interact").empty()&&
          exits::DestinationLabel(L"$GT_HUD_Int_Missing").empty(),"empty, malformed and unresolved labels are not offered");
    check(exits::DestinationLabel(L"Exit [courtyard]")==L"Exit [courtyard]","ordinary bracketed prose is not erased");
    // E1_2A_Corridor_LD DNEVolume_5/BrushComponent_29: historical live Bounds
    // at +0x5c, also checked against the authored box and actor translation.
    exits::Bounds bounds{{-590.4821167f,-3197.0419922f,99.0826111f},
                         {81.2416458f,198.7959290f,99.0849915f},236.1014862f};
    const std::string id="e1_2a_corridor_ld.TheWorld.PersistentLevel.DNEVolume_5";
    world::Snapshot snapshot;
    check(exits::AddDestination(snapshot,id,label,bounds)&&snapshot.targets.size()==1,"authored campus area becomes an exit target");
    check(snapshot.targets[0].position.y< -3000&&std::fabs(snapshot.targets[0].position.y-(-2799.48999f))>300,
          "target is inside the actual region rather than at the outside brush pivot");
    check(snapshot.targets[0].kind==world::Kind::Exit&&!world::NeedsInteractionFocus(world::Kind::Exit),"exit does not promise ordinary-object focus or pitch aiming");
    check(!exits::AddDestination(snapshot,id,label,bounds)&&snapshot.targets.size()==1,"multiple active prompt nodes do not duplicate one physical exit");
    world::Snapshot custom;custom.targets={{id,L"My waypoint",{1,2,3},world::Kind::Destination}};
    check(!exits::AddDestination(custom,id,label,bounds)&&custom.targets[0].label==L"My waypoint"&&custom.targets[0].position.x==1,"explicit user landmark stays authoritative");
    auto bad=bounds;bad.origin.x=std::numeric_limits<float>::quiet_NaN();
    check(!exits::AddDestination(snapshot,"invalid",label,bad),"nonfinite runtime bounds rejected");
    bad=bounds;bad.extent.y=0;
    check(!exits::ExitBoundsValid(bad),"uninitialized or flat bounds rejected");
    bad=bounds;bad.radius=-1;
    check(!exits::ExitBoundsValid(bad),"invalid sphere bounds rejected");
    check(!exits::AddDestination(snapshot,"",label,bounds)&&!exits::AddDestination(snapshot,"empty",L"",bounds),"missing identity or label rejected");
    uintptr_t collision=0;
    auto reader=[](uintptr_t,const char* owner,const char* field,const char* type,uintptr_t& result) {
        if(std::string(owner)!="Actor"||std::string(field)!="CollisionComponent"||std::string(type)!="ComponentProperty")return false;
        result=0x123400;return true;
    };
    check(exits::ReadCollision(reader,0x10000,collision)&&collision==0x123400,"real reflected ComponentProperty type is used, not ObjectProperty");
    auto missing=[](uintptr_t,const char*,const char*,const char*,uintptr_t&){return false;};
    check(!exits::ReadCollision(missing,0x10000,collision),"missing collision metadata fails closed");
    snapshot.player=bounds.origin;snapshot.radius=30;snapshot.halfHeight=45;world::PrepareTargets(snapshot);
    auto speech=navigation::detail::Describe(snapshot,snapshot.targets[0]);
    check(speech.find(L"Exit area")!=std::wstring::npos&&speech.find(L"focused")==std::wstring::npos&&
          speech.substr(speech.size()-7)==L"1 of 1.","exit readout identifies its kind, avoids false focus instructions and ends with list position");
    check(navigation::detail::CueFor(world::Kind::Exit)==audio::Cue::Destination,"exit uses the existing destination beacon");
    check(world::NeedsInteractionFocus(world::Kind::Object)&&world::NeedsInteractionFocus(world::Kind::Person)&&
          !world::NeedsInteractionFocus(world::Kind::Destination),"ordinary objects/people keep focus aiming; waypoints stay horizontal");
    return errors?1:0;
}
