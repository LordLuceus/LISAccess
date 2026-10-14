#pragma once
#include "engine/abi.h"
#include "engine/memory.h"
#include <cstdint>

namespace lis::dialogue {
// Verified x86 TSet/Map_Mirror header. Allocation bits, not stale free rows,
// determine which entries exist. Hash storage is not needed for bounded scans.
struct NativeChoiceMap {
    engine::Array slots;
    uint32_t bits[4];uintptr_t heapBits;
    int bitCount,bitCapacity,firstFree,freeCount;
    uint32_t hashInline;uintptr_t hashHeap;int hashSize;
};
static_assert(sizeof(NativeChoiceMap)==60);
struct WatcherEntry {engine::Name key;uintptr_t listeners;int nextHash,hashIndex;};
struct ChoiceListener {uintptr_t object,interface;};
static_assert(sizeof(WatcherEntry)==20&&sizeof(ChoiceListener)==8);
struct RegisteredChoices {uintptr_t owner=0;unsigned slots=0;};
inline bool ChoiceArrayValid(const engine::Array& a,int maximum) {
    return a.count>=0&&a.count<=maximum&&a.capacity>=a.count&&(!a.count||a.data>=0x10000);
}
inline bool InlineChoiceMapValid(const NativeChoiceMap& m,int maximum) {
    if(!ChoiceArrayValid(m.slots,maximum)||m.heapBits||m.bitCount!=m.slots.count||
       m.freeCount<0||m.freeCount>m.slots.count)return false;
    if(!m.slots.count)return !m.freeCount;
    if(m.bitCapacity!=128||m.slots.count>128)return false;
    int used=0;
    for(int i=0;i<m.slots.count;++i)used+=(m.bits[i/32]>>(i%32))&1;
    return used==m.slots.count-m.freeCount;
}
// Reader and candidate validation are supplied by the native adapter; tests use
// captured map shapes without a running game. A second active owner is ambiguous.
template<class Copy,class KeyMask,class Candidate>
bool ReadRegisteredChoices(const NativeChoiceMap& map,const Copy& copy,
                           const KeyMask& mask,const Candidate& candidate,RegisteredChoices& out) {
    out={};
    if(!InlineChoiceMapValid(map,128))return false;
    RegisteredChoices found;
    for(int i=0;i<map.slots.count;++i) {
        if(!(map.bits[i/32]&(1u<<(i%32))))continue;
        WatcherEntry entry{};
        if(!copy(&entry,map.slots.data+i*sizeof(entry),sizeof(entry)))return false;
        const unsigned keys=mask(entry.key);
        if(!keys)continue;
        engine::Array listeners{};
        if(!entry.listeners||!copy(&listeners,entry.listeners,sizeof(listeners))||
           !ChoiceArrayValid(listeners,256))return false;
        for(int j=0;j<listeners.count;++j) {
            ChoiceListener listener{};
            if(!copy(&listener,listeners.data+j*sizeof(listener),sizeof(listener)))return false;
            if(!candidate(listener))continue;
            if(found.owner&&found.owner!=listener.object)return false;
            found.owner=listener.object;found.slots|=keys;
        }
    }
    out=found;return true;
}
inline bool ChoiceRewindAllowed(bool scripted,unsigned state) {
    // A fresh-load principal choice normally freezes at ERMS_RewindPaused (2).
    // Only the registered scripted owner may use that state. Active rewinding
    // and fast-forwarding remain blocked for BOTH owner kinds.
    return state==0||(scripted&&state==2);
}
inline bool ScriptedChoiceWaiting(int wait,int chosen,bool finished) {
    return wait==0&&chosen==-1&&!finished;
}
}
