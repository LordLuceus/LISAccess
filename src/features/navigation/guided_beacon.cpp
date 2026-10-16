#include "features/navigation/guided_beacon.h"
#include "features/navigation/guided_scene.h"
#include "platform/log.h"
#include <Windows.h>
#include <mutex>

namespace lis::navigation::guidance {
namespace {
std::mutex mutex;
uint64_t generation=0,seen=0;
std::string requested;
View published;
route::Guide guide;
world::Snapshot observed;
route::Identity observedIdentity;
uint64_t observedAt=0;
bool ticking=false;
bool Fresh(uint64_t now,uint64_t at) {return at&&now>=at&&now-at<=450;}
bool Terminal(route::Status s) {
    return s!=route::Status::Off&&s!=route::Status::Planning&&s!=route::Status::Following;
}
const char* StatusName(route::Status s) {
    switch(s) {
    case route::Status::Off:return "off";case route::Status::Planning:return "planning";
    case route::Status::Following:return "following";case route::Status::Approach:return "approach";
    case route::Status::Arrived:return "arrived";case route::Status::NoRoute:return "no_route";
    case route::Status::Unsupported:return "unsupported";case route::Status::Limit:return "limit";
    default:return "unavailable";
    }
}
}
void Request(const std::string& selected) {
    // EXPERIMENTAL (2026-10-16): manually followed routes; waypoint ergonomics
    // and terrain coverage still require player feedback. No movement authority.
    std::lock_guard<std::mutex> lock(mutex);
    ++generation;requested=selected;published={};published.selected=selected;published.generation=generation;
    published.route.status=selected.empty()?route::Status::Off:route::Status::Planning;
}
void Cancel() {
    std::lock_guard<std::mutex> lock(mutex);
    if(requested.empty()&&published.route.status==route::Status::Off)return;
    ++generation;requested.clear();published={};published.generation=generation;
}
View Read() {std::lock_guard<std::mutex> lock(mutex);return published;}
void Observe(const world::Snapshot& sample) {
    if(!guided_scene::Thread())return;
    observed=sample;observedIdentity=guided_scene::CurrentIdentity();observedAt=GetTickCount64();
}
void Tick() {
    if(!guided_scene::Thread()||ticking)return;
    std::string target;uint64_t token=0;
    {
        std::lock_guard<std::mutex> lock(mutex);target=requested;token=generation;
        if(token==seen&&Terminal(published.route.status))return;
    }
    const bool newRequest=token!=seen;
    if(newRequest){guide.Reset();seen=token;if(!target.empty())guide.Start(target);}
    if(target.empty())return;
    ticking=true;
    View next;next.generation=token;next.selected=target;
    const auto began=GetTickCount64();unsigned queries=0;
    guided_scene::Frame frame;
    try {
        const auto now=GetTickCount64();
        if(!Fresh(now,observedAt)||!guided_scene::Capture(observed,observedIdentity,frame)) {
            next.route.status=route::Status::Unavailable;guide.Reset();
        } else {
            next.route=guide.Tick(frame.snapshot,frame.identity,now,[&](auto from,auto to,auto extent){return guided_scene::Trace(frame,from,to,extent);});
            queries=frame.queries;
            next.player=frame.snapshot.player;next.yaw=frame.snapshot.cameraYaw;next.radius=frame.snapshot.radius;
        }
    } catch(...) {guide.Reset();next.route={};next.route.status=route::Status::Unavailable;}
    next.sampledAt=GetTickCount64();ticking=false;
    std::string event;
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Cancellation/selection changes during a query cannot publish old guidance.
        if(generation==token&&requested==target) {
            if(next.route.status!=published.route.status||next.sampledAt-began>20)
                event=std::string("event=guided_beacon status=")+StatusName(next.route.status)+
                    " queries="+std::to_string(queries)+" tick_ms="+std::to_string(next.sampledAt-began);
            published=std::move(next);
        }
    }
    if(!event.empty())Log(event);
}
void Shutdown(){Cancel();}
}
