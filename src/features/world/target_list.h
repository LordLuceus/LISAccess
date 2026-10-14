#pragma once
#include "features/world/world.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
namespace lis::world {
inline std::wstring TargetLetter(size_t index) {
    std::wstring result;
    do {
        result.insert(result.begin(),static_cast<wchar_t>(L'A'+index%26));
        if(index<26)break;
        index=index/26-1;
    } while(true);
    return result;
}
// Apply once to a newly sampled, unsuffixed list. Like Hortator, letters follow
// stable object identity, never the distance order. Membership/name changes can
// change a duplicate group; merely moving the player cannot swap its letters.
inline void PrepareTargets(Snapshot& s) {
    std::unordered_map<std::wstring,std::vector<size_t>> groups;
    for(size_t i=0;i<s.targets.size();++i)groups[s.targets[i].label].push_back(i);
    for(auto& entry:groups) {
        auto& indices=entry.second;
        if(indices.size()<2)continue;
        std::sort(indices.begin(),indices.end(),[&](size_t a,size_t b){return s.targets[a].stableId<s.targets[b].stableId;});
        for(size_t i=0;i<indices.size();++i)s.targets[indices[i]].label+=L", "+TargetLetter(i);
    }
    // Ground distance matches the spoken bearing/distance; height is separate.
    auto distance=[&](const Target& t){return std::hypot(t.position.x-s.player.x,t.position.y-s.player.y);};
    std::sort(s.targets.begin(),s.targets.end(),[&](const Target& a,const Target& b){
        const auto da=distance(a),db=distance(b);
        return da!=db?da<db:a.stableId<b.stableId;
    });
    for(const auto& t:s.targets)if(t.stableId==s.focusedId){s.focusedLabel=t.label;break;}
}
}
