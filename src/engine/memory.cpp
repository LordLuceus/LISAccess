#include "engine/memory.h"
#include <Windows.h>
#include <cstring>

namespace lis::engine {
bool Copy(void* destination,uintptr_t source,size_t bytes) {
    if(source<0x10000 || bytes>1024*1024)return false;
    __try {std::memcpy(destination,reinterpret_cast<void*>(source),bytes);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
