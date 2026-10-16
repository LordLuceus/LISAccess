#include "features/camera/facing.h"
#include "features/camera/facing_native.h"
#include "platform/foreground.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
namespace lis::world {
namespace {
using namespace facing_policy;
struct Attempt {
    Policy policy;
    facing_native::NativeFrame identity;
    std::string id;
    std::wstring label;
    bool pitch=false;
};
Attempt attempt;
std::string cancelled;
bool OnThread(){return GameSession().thread==GetCurrentThreadId();}
bool Ready(){auto& s=GameSession();return OnThread()&&IsGameForeground()&&s.ready&&!s.faulted&&!s.faceFaulted;}
bool RequestCameraRotation(Session& session,uintptr_t behaviour,Rotation rotation) {
    __try {
        if(!session.reflection.registry.Registered(session.faceFunction)||facing_native::NativeRead<uint16_t>(session.faceFunction+0xa2)!=16||facing_native::NativeRead<uint16_t>(session.faceFunction+0x94)!=0||
            facing_native::NativeRead<uintptr_t>(facing_native::NativeRead<uintptr_t>(behaviour)+72*4)!=session.base+engine::profile::ObjectDispatch)return false;
        struct Params {uint32_t activate;Rotation rotation;} p{1,rotation};
        static_assert(sizeof(Params)==16);
        reinterpret_cast<engine::ProcessEvent>(session.base+engine::profile::ObjectDispatch)(reinterpret_cast<void*>(behaviour),reinterpret_cast<void*>(session.faceFunction),&p,nullptr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){session.faceFaulted=true;return false;}
}
bool Submit(const facing_native::NativeFrame& native) {
    const auto& v=native.frame;const auto& a=attempt.policy.aim;
    try {
        if(!RequestCameraRotation(GameSession(),native.behaviour.address,a.wanted))return false;
    }catch(...){GameSession().faceFaulted=true;return false;}
    attempt.policy.Submitted(v);return true;
}
std::wstring Finish(Result result,std::string& completed) {
    completed=attempt.id;
    const auto label=attempt.label;
    const bool applied=attempt.policy.applied,pitch=attempt.pitch;
    attempt={};
    if(result==Result::Focused||result==Result::Cancelled)return {}; // native action narration owns success
    if(!pitch&&result==Result::Aligned)return applied?L"Turned toward "+label+L".":L"Already facing "+label+L".";
    if(applied)return L"Turned toward "+label+(pitch?L", but it is not focused.":L", but direction alignment is incomplete.");
    return L"Could not finish turning toward "+label+L".";
}
bool Observe(const Snapshot& snapshot,const Target& target,facing_native::NativeFrame& native) {
    if(!facing_native::Read(GameSession(),target.stableId,attempt.identity.target,native,NeedsInteractionFocus(target.kind)))return false;
    native.frame.now=GetTickCount64();
    native.frame.pitch=NeedsInteractionFocus(target.kind);
    // Non-interaction targets retain their sampled direction point (e.g. exit volumes).
    if(!native.frame.pitch)native.frame.target=target.position;
    native.frame.focused=native.frame.pitch&&snapshot.focusedId==target.stableId;
    return true;
}
}
void CancelFacing() {
    if(!OnThread())return;
    if(!attempt.id.empty())cancelled=attempt.id;
    attempt={}; // never call the native setter with activate=false
}
bool FacingActive(){return OnThread()&&attempt.policy.running;}
std::wstring FaceTarget(const std::string& expectedTarget,bool& submitted) {
    submitted=false;
    if(!OnThread())return L"Facing unavailable here.";
    CancelFacing();
    cancelled.clear(); // the caller is synchronously replacing the previous request
    if(!Ready())return L"Facing unavailable here.";
    Snapshot fresh;
    if(!SafeSample(GameSession(),fresh)||!fresh.active||fresh.interactionBusy)return L"Facing unavailable while the camera controls are locked.";
    auto target=std::find_if(fresh.targets.begin(),fresh.targets.end(),[&](const Target& t){return t.stableId==expectedTarget;});
    if(target==fresh.targets.end())return L"That target is no longer available.";
    if(target->stableId==fresh.focusedId)return L"Already focused on "+target->label+L".";
    facing_native::NativeFrame native;
    if(!Observe(fresh,*target,native))return native.error;
    if(native.frame.forced||native.frame.remaining>0)return L"Wait for the camera to finish moving.";
    if(Distance(native.frame.origin,native.frame.target)<1)return L"The target is too close to aim at.";
    attempt.id=target->stableId;attempt.label=target->label;attempt.pitch=NeedsInteractionFocus(target->kind);attempt.identity=native;
    attempt.policy.Begin(native.frame);
    const auto result=attempt.policy.Step(native.frame);
    if(result!=Result::Request) {
        std::string completed;
        if(result==Result::Aligned&&attempt.pitch) {attempt={};return L"Already looking toward "+target->label+L", but it is not focused.";}
        return Finish(result,completed);
    }
    if(!Submit(native)){attempt={};return L"The camera request failed.";}
    submitted=true;
    return L"Turning toward "+target->label+L".";
}
std::wstring TickFacing(const Snapshot& fresh,std::string& completed) {
    completed.clear();
    if(!OnThread())return {};
    if(!cancelled.empty()){completed=cancelled;cancelled.clear();if(!attempt.policy.running)return {};}
    if(!attempt.policy.running)return {};
    if(!Ready()||!fresh.active||fresh.interactionBusy)return Finish(Result::Cancelled,completed);
    // Focus may be off-centre: never turn again to satisfy an invented success cone.
    if(attempt.pitch&&fresh.focusedId==attempt.id)return Finish(Result::Focused,completed);
    const auto target=std::find_if(fresh.targets.begin(),fresh.targets.end(),[](const Target& t){return t.stableId==attempt.id;});
    if(target==fresh.targets.end())return Finish(Result::Cancelled,completed);
    facing_native::NativeFrame native;
    if(!Observe(fresh,*target,native))return Finish(GameSession().faceFaulted?Result::Unavailable:Result::Cancelled,completed);
    native.frame.context=attempt.identity.Same(native)&&attempt.pitch==NeedsInteractionFocus(target->kind);
    const auto result=attempt.policy.Step(native.frame);
    if(result==Result::Wait)return {};
    if(result!=Result::Request)return Finish(result,completed);
    if(!Submit(native))return Finish(Result::Unavailable,completed);
    return {};
}
}
