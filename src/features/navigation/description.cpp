#include "features/navigation/description.h"
#include "features/navigation/math.h"
#include <algorithm>
#include <cmath>

namespace lis::navigation::detail {
float Distance(const world::Snapshot& s, const world::Target& t) {
    return std::hypot(t.position.x-s.player.x,t.position.y-s.player.y);
}
float Bearing(const world::Snapshot& s, const world::Target& t) {
    return navmath::Bearing(t.position.x-s.player.x,t.position.y-s.player.y,s.cameraYaw);
}
std::wstring Describe(const world::Snapshot& s, const world::Target& t) {
    const float distance=Distance(s,t);
    std::wstring text=t.label;
    auto append=[&](const std::wstring& part){
        if(part.empty())return;
        if(!text.empty())text+=text.back()==L'.'?L" ":L". ";
        text+=part;
    };
    if(t.kind!=world::Kind::Destination) {
        if(t.stableId==s.focusedId){append(L"Focused");append(ActionText(s));}
        else append(L"Not focused. Face this target before choosing an action.");
        if(t.angleRestricted)append(L"Try facing or approaching it from another direction.");
    }
    std::wstring position=std::wstring(navmath::Direction(Bearing(s,t)))+L", "+navmath::Distance(distance,s.radius);
    if(t.position.z-s.player.z > s.halfHeight*2)position+=L", above";
    else if(s.player.z-t.position.z > s.halfHeight*2)position+=L", below";
    append(position);
    const auto found=std::find_if(s.targets.begin(),s.targets.end(),[&](const auto& item){return item.stableId==t.stableId;});
    if(found!=s.targets.end())append(std::to_wstring(found-s.targets.begin()+1)+L" of "+std::to_wstring(s.targets.size()));
    if(!text.empty()&&text.back()!=L'.')text+=L'.';
    return text;
}
audio::Cue CueFor(world::Kind kind) {
    if(kind==world::Kind::Person) return audio::Cue::Person;
    if(kind==world::Kind::Destination) return audio::Cue::Destination;
    return audio::Cue::Object;
}
const wchar_t* PreviewName(int i) {
    const wchar_t* names[]={L"Obstacle",L"Person",L"Object",L"Destination"};
    return names[i];
}
std::wstring ActionText(const world::Snapshot& s) {
    std::wstring text;
    static const wchar_t* pad[]={L"Y",L"X",L"B",L"A"};
    for(size_t i=0;i<s.actions.size();++i)if(!s.actions[i].label.empty()) {
        if(!text.empty())text+=L". ";
        std::wstring label=s.actions[i].label;
        while(!label.empty()&&(label.back()==L'.'||label.back()==L' '))label.pop_back();
        text+=label+L", "+(s.gamepad ? std::wstring(pad[i]) : std::to_wstring(i+1));
    }
    return text;
}
std::wstring FocusText(const world::Snapshot& s) {
    std::wstring text=s.focusedLabel;const auto actions=ActionText(s);
    if(!actions.empty()){if(!text.empty())text+=text.back()==L'.'?L" ":L". ";text+=actions;}
    return text;
}
}
