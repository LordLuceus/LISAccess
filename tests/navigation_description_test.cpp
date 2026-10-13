#include "features/navigation/description.h"
#include <iostream>

int main() {
    using namespace lis;
    world::Snapshot s;
    s.focusedId="photo";s.focusedLabel=L"Photo";
    s.actions[0].label=L"Look. ";s.actions[3].label=L"Take selfie";
    if(navigation::detail::FocusText(s)!=L"Photo. Look, 1. Take selfie, 4")return 1;
    s.gamepad=true;
    if(navigation::detail::FocusText(s)!=L"Photo. Look, Y. Take selfie, A")return 2;
    s.focusedLabel=L"Photo.";
    if(navigation::detail::FocusText(s)!=L"Photo. Look, Y. Take selfie, A")return 3;
    s.radius=21;s.halfHeight=44;
    world::Target target{"photo",L"Photo",{100,0,0},world::Kind::Object,false};
    auto text=navigation::detail::Describe(s,target);
    if(text.find(L". Focused. Look, Y. Take selfie, A")==std::wstring::npos)return 4;
    s.focusedId.clear();target.angleRestricted=true;
    text=navigation::detail::Describe(s,target);
    if(text.find(L"Not focused. Face this target before choosing an action.")==std::wstring::npos ||
        text.find(L"Try facing or approaching it from another direction.")==std::wstring::npos)return 5;
    target.kind=world::Kind::Destination;
    text=navigation::detail::Describe(s,target);
    if(text.find(L"focused")!=std::wstring::npos || text.find(L"approaching")!=std::wstring::npos)return 6;
    if(navigation::detail::CueFor(world::Kind::Person)!=audio::Cue::Person ||
        navigation::detail::CueFor(world::Kind::Object)!=audio::Cue::Object ||
        navigation::detail::CueFor(world::Kind::Destination)!=audio::Cue::Destination)return 7;
    std::cout<<"PASS: extracted focus/action/target descriptions preserve speech and cue mapping\n";
}
