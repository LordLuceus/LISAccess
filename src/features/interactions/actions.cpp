#include "features/interactions/actions.h"
#include "features/interactions/scanner.h"
#include "features/world/session.h"
#include "platform/log.h"
#include "platform/foreground.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include "features/navigation/collision.h"
#include <algorithm>
#include <cmath>
#include <cctype>
namespace lis::world {
using engine::Copy;
using engine::Read;
using engine::Array;
bool SendAction(Session& session,uintptr_t actor,engine::Name binding) {
    __try {
        const uintptr_t listener=actor+0x26c;
        const uintptr_t method=Read<uintptr_t>(Read<uintptr_t>(listener)+8);
        if(method!=session.base+engine::profile::InteractionPress)return false;
        // That concrete consumer does not read its third stack word. It resolves
        // the real BindingActionMap and calls the native guarded request path.
        reinterpret_cast<engine::InteractionPress>(method)(reinterpret_cast<void*>(listener),binding,nullptr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){session.interactionFaulted=true;return false;}
}
std::wstring ActivateFocused(const std::string& expectedFocus,unsigned slot) {
    auto& session=GameSession();
    if(session.thread!=GetCurrentThreadId()||!IsGameForeground()||session.faulted||session.interactionFaulted||slot>=4)return L"Interaction unavailable.";
    Snapshot fresh;
    if(!SafeSample(session,fresh)||fresh.focusedId.empty()||fresh.focusedId!=expectedFocus)return L"The focused object changed. Face the target again.";
    if(fresh.actions[slot].label.empty())return L"That action is not available.";
    engine::Name binding{};
    if(!Binding(session,slot,binding)||!session.reflection.registry.Instance(session.liveFocus)||!SendAction(session,session.liveFocus,binding))
        return session.interactionFaulted ? L"Interaction failed and has been disabled. Navigation is still available." : L"This object's input interface is not supported.";
    return {};
}
}
