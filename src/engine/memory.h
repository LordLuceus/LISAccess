#pragma once
#include <cstddef>
#include <cstdint>

namespace lis::engine {
// Read-only, bounded copies. Native queries/actions have separate, checked APIs.
bool Copy(void* destination, uintptr_t source, size_t bytes);
template<class T> T Read(uintptr_t address) {
    T value{};
    Copy(&value,address,sizeof(value));
    return value;
}
struct Array { uintptr_t data; int count, capacity; };
static_assert(sizeof(uintptr_t)==4 && sizeof(Array)==12,"Verified x86 build only");
}
