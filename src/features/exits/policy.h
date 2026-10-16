#pragma once
#include "features/world/world.h"
#include "features/menus/narration.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace lis::exits {
struct Bounds {world::Vec3 origin,extent;float radius;};
static_assert(sizeof(Bounds)==28);
inline bool ExitBoundsValid(const Bounds& b) {
    auto finite=[](world::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    return finite(b.origin)&&finite(b.extent)&&std::isfinite(b.radius)&&b.radius>0&&
        b.extent.x>0&&b.extent.y>0&&b.extent.z>0;
}
template<class Reader>
bool ReadCollision(Reader read,uintptr_t actor,uintptr_t& component) {
    return read(actor,"Actor","CollisionComponent","ComponentProperty",component);
}
inline std::wstring DestinationLabel(std::wstring text) {
    // These are destination names, not instructions to activate from afar.
    // Keep localized prose; omit only authored action-token placeholders.
    for(size_t at=text.find(L"[action_");at!=std::wstring::npos;at=text.find(L"[action_")) {
        const size_t end=text.find(L']',at);
        if(end==std::wstring::npos)return {};
        text.erase(at,end-at+1);
    }
    text=CleanText(text);
    if(text.rfind(L"$GT_",0)==0)return {};
    return text;
}
inline bool AddDestination(world::Snapshot& s,const std::string& id,const std::wstring& label,const Bounds& bounds) {
    if(id.empty()||label.empty()||!ExitBoundsValid(bounds))return false;
    if(std::any_of(s.targets.begin(),s.targets.end(),[&](const auto& t){return t.stableId==id;}))return false;
    // Actor.Location is only the brush pivot. For the corridor exit it is
    // OUTSIDE its volume. The engine's world-space bounds locate the real area.
    world::Target target{id,label,bounds.origin,world::Kind::Exit};
    target.volumeExtent=bounds.extent;s.targets.push_back(std::move(target));return true;
}
}
