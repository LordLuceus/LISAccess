#include "features/camera/facing.h"
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
struct Rotator {int pitch=0,yaw=0,roll=0;};
int SignedRotation(int angle) {return static_cast<int16_t>(static_cast<uint32_t>(angle)&65535u);}
bool RequestCameraRotation(Session& session,uintptr_t behaviour,Rotator rotation) {
    __try {
        if(!session.reflection.registry.Registered(session.faceFunction)||Read<uint16_t>(session.faceFunction+0xa2)!=16||Read<uint16_t>(session.faceFunction+0x94)!=0 ||
            Read<uintptr_t>(Read<uintptr_t>(behaviour)+72*4)!=session.base+engine::profile::ObjectDispatch)return false;
        struct Params {uint32_t activate;Rotator rotation;} p{1,rotation};
        static_assert(sizeof(Params)==16);
        reinterpret_cast<engine::ProcessEvent>(session.base+engine::profile::ObjectDispatch)(reinterpret_cast<void*>(behaviour),reinterpret_cast<void*>(session.faceFunction),&p,nullptr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){session.faceFaulted=true;return false;}
}
std::wstring FaceTarget(const std::string& expectedTarget,bool& submitted) {
    auto& session=GameSession();
    // EXPERIMENTAL (2026-10-12): explicit normal-camera request only. Native
    // collision/offsets may prevent focus; submission is not proof of arrival.
    submitted=false;
    if(session.thread!=GetCurrentThreadId()||!IsGameForeground()||session.faulted||session.faceFaulted)return L"Facing unavailable here.";
    Snapshot fresh;
    if(!SafeSample(session,fresh)||fresh.interactionBusy)return L"Facing unavailable while the camera controls are locked.";
    auto target=std::find_if(fresh.targets.begin(),fresh.targets.end(),[&](const Target& t){return t.stableId==expectedTarget;});
    if(target==fresh.targets.end())return L"That target is no longer available.";
    if(target->stableId==fresh.focusedId)return L"Already focused on "+target->label+L".";
    uintptr_t camera=0,behaviour=0;Array behaviours{};
    if(!session.reflection.Value(session.liveController,"WhatIfController","ExCamera","ObjectProperty",camera)||!session.reflection.registry.Instance(camera) ||
        !session.reflection.Value(camera,"WhatIfCamera","GameplayCameraBehaviours","ArrayProperty",behaviours)||!ValidArray(behaviours,32))return L"Camera mode unavailable.";
    for(int i=0;i<behaviours.count;++i) {
        uintptr_t candidate=Read<uintptr_t>(behaviours.data+i*4);
        if(!session.reflection.registry.Instance(candidate)||!session.reflection.BoolIsSet(candidate,"CameraBehaviour","bActive"))continue;
        // Do not take over fixed, dialogue, derived or blended camera behaviours.
        if(behaviour||session.reflection.registry.ClassOf(candidate)!="CBA_3RD_Basic")return L"Facing is unavailable in this camera mode.";
        behaviour=candidate;
    }
    if(!behaviour||session.reflection.BoolIsSet(behaviour,"CBA_3RD_Basic","bWantNextCameraRotationForced"))return L"Wait for the camera to finish moving.";
    uintptr_t settings=0;Rotator down{},up{},gameRotation{},viewRotation{};Vec3 view{};

    int low=0,high=0,boundary=0,cache=0,pov=0,position=0,rotation=0;
    if(!session.reflection.Value(behaviour,"CBA_3RD_Basic","CurPOVSettings","ObjectProperty",settings)||!session.reflection.registry.Instance(settings) ||
        !session.reflection.StructOffset("CBPOVSettings","SettingsLookDown",low)||!session.reflection.StructOffset("CBPOVSettings","SettingsLookUp",high) ||
        !session.reflection.Offset("SCameraPOVSettings","PitchBoundary","StructProperty",12,boundary) ||
        !Copy(&down,settings+low+boundary,12)||!Copy(&up,settings+high+boundary,12) ||
        !session.reflection.StructOffset("Camera","CameraCache",cache)||!session.reflection.StructOffset("TCameraCache","POV",pov) ||
        !session.reflection.Offset("TPOV","Location","StructProperty",12,position)||!session.reflection.Offset("TPOV","Rotation","StructProperty",12,rotation) ||
        !Copy(&view,camera+cache+pov+position,12)||!Finite(view)||!Copy(&viewRotation,camera+cache+pov+rotation,12) ||
        !session.reflection.Value(camera,"WhatIfCamera","GPCamRotation","StructProperty",gameRotation))return L"Camera limits could not be read.";
    int minimum=SignedRotation(down.pitch),maximum=SignedRotation(up.pitch);
    if(session.reflection.BoolIsSet(behaviour,"CBA_3RD_Basic","bOverrideNextPitch")) {
        if(!session.reflection.Value(behaviour,"CBA_3RD_Basic","NextPitchMin","IntProperty",minimum)||!session.reflection.Value(behaviour,"CBA_3RD_Basic","NextPitchMax","IntProperty",maximum))return L"Camera limits could not be read.";
        minimum=SignedRotation(minimum);maximum=SignedRotation(maximum);
    }
    if(minimum>maximum||minimum< -16384||maximum>16384)return L"Unsupported camera limits.";
    Vec3 delta=Sub(target->position,view);
    if(Length(delta)<1)return L"The target is too close to aim at.";
    const float units=65536/(2*Pi);
    const int yaw=static_cast<int>(std::lround(std::atan2(delta.y,delta.x)*units));
    const int pitch=static_cast<int>(std::lround(std::atan2(delta.z,std::hypot(delta.x,delta.y))*units));
    Rotator wanted=gameRotation;
    wanted.yaw=SignedRotation(SignedRotation(gameRotation.yaw)+SignedRotation(yaw-SignedRotation(viewRotation.yaw)));
    if(NeedsInteractionFocus(target->kind))
        wanted.pitch=std::clamp(SignedRotation(gameRotation.pitch)+SignedRotation(pitch-SignedRotation(viewRotation.pitch)),minimum,maximum);
    if(!RequestCameraRotation(session,behaviour,wanted))return L"The camera request failed.";
    submitted=true;
    return L"Turning toward "+target->label+L".";
}
}
