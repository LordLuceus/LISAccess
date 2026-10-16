#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
namespace lis::world::facing_read {
inline bool Span(uintptr_t base,size_t offset,size_t bytes) {
    const auto maximum=(std::numeric_limits<uintptr_t>::max)();
    return base>=0x10000&&offset<=maximum-base&&bytes&&bytes-1<=maximum-base-offset;
}
// Metadata validates owner/name/property kind/size before any value copy. Kept
// engine-free so unknown descriptors and failed reads can be tested directly.
template<class Reflection,class Copy,class T>
bool DescriptorNativeRead(const Reflection& reflection,Copy copy,uintptr_t base,
                          const char* owner,const char* name,const char* type,T& out) {
    int offset=0;
    if(!reflection.Offset(owner,name,type,sizeof(T),offset)||offset<0||offset>=0x10000||
       !Span(base,static_cast<size_t>(offset),sizeof(T)))return false;
    return copy(&out,base+offset,sizeof(T));
}
}
