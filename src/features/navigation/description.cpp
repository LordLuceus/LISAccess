#include "features/navigation/description.h"
#include "features/navigation/math.h"
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
    std::wstring text=t.label+L", "+navmath::Direction(Bearing(s,t))+L", "+navmath::Distance(distance,s.radius);
    if(t.position.z-s.player.z > s.halfHeight*2) text+=L", above";
    else if(s.player.z-t.position.z > s.halfHeight*2) text+=L", below";
    if(t.kind!=world::Kind::Destination) {
        if(t.stableId==s.focusedId){text+=L". Focused";const auto actions=ActionText(s);if(!actions.empty())text+=L". "+actions;}
        else text+=L". Not focused. Face this target before choosing an action.";
        if(t.angleRestricted)text+=L" Try facing or approaching it from another direction.";
    }
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
