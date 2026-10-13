#pragma once
#include <array>
#include <cstdint>
namespace lis::dialogue {
// Build 2304417's captured TSet<Map<int,int>> entries; allocation bits, not
// array positions or stored dialogue definitions, determine which rows exist.
struct BindingEntry {int32_t slot,choice,nextHash,hashIndex;};
static_assert(sizeof(BindingEntry)==16);
inline bool DecodeBindings(const BindingEntry* entries,int count,uint32_t bits,int freeCount,std::array<int,4>& out) {
    out.fill(-1);
    if(count<0||count>16||freeCount<0||freeCount>count||(count&&!entries))return false;
    std::array<bool,4> seen{};int used=0;
    for(int i=0;i<count;++i) {
        if(!(bits&(1u<<i)))continue;
        const auto& entry=entries[i];
        if(entry.slot<0||entry.slot>=4||seen[entry.slot]||entry.choice < -1||entry.choice>4096)return false;
        seen[entry.slot]=true;out[entry.slot]=entry.choice;++used;
    }
    return used==count-freeCount;
}
}
