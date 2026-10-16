#include "features/navigation/beacon_control.h"
#include "features/navigation/math.h"
#include <cmath>
namespace lis::navigation {
namespace {
bool Fresh(uint64_t now,uint64_t at){return at&&now>=at&&now-at<=250;}
bool Failure(route::Status status) {
    return status==route::Status::NoRoute||status==route::Status::Unsupported||
        status==route::Status::Limit||status==route::Status::Unavailable;
}
std::wstring FailureText(route::Status status) {
    switch(status) {
    case route::Status::NoRoute:return L"No verified route to this target. Beacon off.";
    case route::Status::Unsupported:return L"Route guidance is unavailable on this terrain. Beacon off.";
    case route::Status::Limit:return L"Could not verify a route within the search limit. Beacon off.";
    default:return L"Route guidance unavailable. Beacon off.";
    }
}
BeaconOutput Aim(world::Vec3 player,float yaw,float radius,world::Vec3 waypoint) {
    BeaconOutput out;const float dx=waypoint.x-player.x,dy=waypoint.y-player.y;
    if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(yaw)||!std::isfinite(radius)||radius<=0)return out;
    out.bearing=navmath::Bearing(dx,dy,yaw);out.distance=std::hypot(dx,dy);out.radius=radius;
    out.audible=std::isfinite(out.distance)&&std::isfinite(out.bearing);return out;
}
}
void BeaconControl::Stop(){enabled=false;directFallback=false;selected.clear();last=route::Status::Off;guidance::Cancel();}
void BeaconControl::Retarget(const world::Target& target,uint64_t now) {
    if(!enabled||selected==target.stableId)return;
    selected=target.stableId;started=now;last=route::Status::Planning;announcedReady=false;lastReplanSpeech=0;
    directFallback=false;fallbackAt=0;
    if(guided)guidance::Request(selected);else guidance::Cancel();
}
std::wstring BeaconControl::Toggle(const world::Target& target,uint64_t now) {
    if(enabled){Stop();return L"Beacon off.";}
    enabled=true;selected.clear();Retarget(target,now);
    return guided?L"Finding a route.":L"Beacon on. Direction only, not a clear route.";
}
BeaconOutput BeaconControl::Update(const world::Snapshot& sample,const world::Target& target,uint64_t now) {
    BeaconOutput out;
    if(!enabled)return out;
    if(!sample.active||sample.interactionBusy||target.stableId!=selected) {Stop();return out;}
    if(!guided)return Aim(sample.player,sample.cameraYaw,sample.radius,target.position);
    if(directFallback) {
        // Give the mode-change announcement a head start. Always use the latest
        // target/player snapshot, never the last routed waypoint.
        if(now<fallbackAt){Stop();return out;}
        if(now-fallbackAt<1000)return out;
        return Aim(sample.player,sample.cameraYaw,sample.radius,target.position);
    }
    const auto view=guidance::Read();
    if(view.selected!=selected||!view.sampledAt) {
        if(now<started||now-started>1000){Stop();out.speech=FailureText(route::Status::Unavailable);}
        return out;
    }
    if(!Fresh(now,view.sampledAt)){Stop();out.speech=FailureText(route::Status::Unavailable);return out;}
    const auto status=view.route.status;
    if(Failure(status)) {
        directFallback=true;fallbackAt=now;guidance::Cancel();
        out.speech=L"Switching to direct target. Direction only, not a clear route.";
        return out;
    }
    if(status==route::Status::Approach) {
        Stop();out.speech=sample.gamepad?L"Near the target. Click the left stick to face it, or aim manually.":
            L"Near the target. Press F to face it, or aim manually.";
        return out;
    }
    if(status==route::Status::Arrived) {
        Stop();
        // Normal focused-object narration supplies the action labels. Do not
        // interrupt that announcement with a less useful generic arrival line.
        if(!world::NeedsInteractionFocus(target.kind)) {
            out.speech=L"Reached "+target.label;
            if(out.speech.empty()||out.speech.back()!=L'.')out.speech+=L'.';
        }
        return out;
    }
    if(status!=last) {
        if(status==route::Status::Following&&!announcedReady){out.speech=L"Route ready. Follow the beacon.";announcedReady=true;}
        else if(status==route::Status::Planning&&last==route::Status::Following&&
            (!lastReplanSpeech||(now>=lastReplanSpeech&&now-lastReplanSpeech>=3000))) {
            out.speech=L"Rechecking the route.";lastReplanSpeech=now;
        }
        last=status;
    }
    if(status==route::Status::Following&&view.route.hasWaypoint) {
        auto aim=Aim(view.player,view.yaw,view.radius,view.route.waypoint);
        aim.speech=std::move(out.speech);return aim;
    }
    return out;
}
}
