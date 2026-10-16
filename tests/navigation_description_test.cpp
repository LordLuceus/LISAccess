#include "features/navigation/description.h"
#include <iostream>

int main() {
    int preview=-1;
    for(int i=0;i<static_cast<int>(lis::audio::Cue::Count);++i) {
        preview=lis::navigation::detail::NextPreview(preview);
        if(preview!=i||std::wstring(lis::navigation::detail::PreviewName(preview)).empty())return 1;
    }
    if(lis::navigation::detail::NextPreview(preview)!=0||
       std::wstring(lis::navigation::detail::PreviewName(4))!=L"Rewind approach"||
       std::wstring(lis::navigation::detail::PreviewName(5))!=L"Rewind crossing"||
       std::wstring(lis::navigation::detail::PreviewName(-1))!=L"Unknown sound")return 1;
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
        text.find(L"Try approaching it from another side.")==std::wstring::npos)return 5;
    target.kind=world::Kind::Destination;
    text=navigation::detail::Describe(s,target);
    if(text.find(L"focused")!=std::wstring::npos || text.find(L"approaching")!=std::wstring::npos)return 6;
    if(navigation::detail::CueFor(world::Kind::Person)!=audio::Cue::Person ||
        navigation::detail::CueFor(world::Kind::Object)!=audio::Cue::Object ||
        navigation::detail::CueFor(world::Kind::Destination)!=audio::Cue::Destination)return 7;
    s.targets={{"other",L"Photo, A",{1,0,0}},{"photo",L"Photo, B",{100,0,0}}};
    s.focusedId="photo";s.focusedLabel=L"Photo, B";
    text=navigation::detail::Describe(s,s.targets[1]);
    const std::wstring suffix=L"2 of 2.";
    if(text.substr(text.size()-suffix.size())!=suffix || text.rfind(L"Photo, B",0)!=0)return 8;
    if(text.find(L"Take selfie, A")>text.find(L"ahead")||text.find(L"ahead")>text.find(suffix))return 9;
    if(navigation::detail::FocusText(s).rfind(L"Photo, B.",0)!=0)return 10;
    s.targets[1].label=L"Photo.";s.focusedId.clear();
    text=navigation::detail::Describe(s,s.targets[1]);
    if(text.find(L"..")!=std::wstring::npos || text.substr(text.size()-suffix.size())!=suffix)return 11;
    std::cout<<"PASS: extracted focus/action/target descriptions preserve speech and cue mapping\n";
}
