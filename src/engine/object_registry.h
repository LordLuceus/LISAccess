#pragma once
#include "engine/memory.h"
#include <string>

namespace lis::engine {
// Preserve the two existing readers' validation policies during this refactor.
// Consolidating their implementations is NOT permission to weaken world guards
// or silently strengthen the accepted menu reader's discovery requirements.
enum class RegistryPolicy { Menu, World };
class ObjectRegistry {
public:
    ObjectRegistry(uintptr_t base,RegistryPolicy policy):base_(base),policy_(policy){}
    bool Registry(Array& array) const;
    bool Registered(uintptr_t object) const;
    std::string Name(int index) const;
    std::string NameOf(uintptr_t object) const;
    std::string ClassOf(uintptr_t object) const;
    bool Instance(uintptr_t object) const;
    bool IsA(uintptr_t object,const char* type) const;
    std::string Path(uintptr_t object) const;
private:
    uintptr_t base_;
    RegistryPolicy policy_;
};
}
