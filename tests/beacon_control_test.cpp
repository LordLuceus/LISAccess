#include "features/navigation/beacon_control.h"
#include <cstdlib>
#include <iostream>
using namespace lis;
using namespace lis::navigation;
namespace {
guidance::View view;
unsigned requests=0,cancels=0;
std::string requested;
void Check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
void Result(route::Status s,uint64_t at=1100) {
    view={};view.selected="target";view.sampledAt=at;view.player={0,0,44};view.radius=21;
    view.route={s,{84,0,40},s==route::Status::Following,false};
}
}
namespace lis::navigation::guidance {
void Request(const std::string& id){++requests;requested=id;view={};}
void Cancel(){++cancels;view={};}
View Read(){return view;}
}
int main() {
    world::Snapshot s;s.active=true;s.player={0,0,44};s.radius=21;s.halfHeight=40;
    world::Target target{"target",L"Door",{400,0,44},world::Kind::Object};
    BeaconControl beacon;
    Check(beacon.Toggle(target,1000)==L"Finding a route."&&requests==1,"existing toggle requests a route");
    Check(!beacon.Update(s,target,1001).audible,"planning never sounds from literal target");
    Result(route::Status::Following);
    auto cue=beacon.Update(s,target,1100);
    Check(cue.audible&&cue.distance==84&&cue.bearing==0,"audio uses waypoint not target position");
    Check(!cue.speech.empty()&&beacon.Update(s,target,1101).speech.empty(),"ready announced once");
    Result(route::Status::Planning,1200);view.route.replanning=true;
    cue=beacon.Update(s,target,1200);
    Check(!cue.audible&&cue.speech==L"Rechecking the route.","replanning suppresses obsolete cue");
    Result(route::Status::Following,1250);beacon.Update(s,target,1250);
    Check(!beacon.Update(s,target,1501).audible&&!beacon.Enabled(),"stale waypoint stops beacon");
    beacon.Toggle(target,2000);Result(route::Status::Approach,2100);
    cue=beacon.Update(s,target,2100);
    Check(!cue.audible&&!beacon.Enabled()&&cue.speech.find(L"Press F")!=std::wstring::npos&&cue.speech.find(L"stick")==std::wstring::npos,"keyboard approach names only its face control");
    beacon.Toggle(target,2200);s.gamepad=true;Result(route::Status::Approach,2300);
    cue=beacon.Update(s,target,2300);
    Check(cue.speech.find(L"left stick")!=std::wstring::npos&&cue.speech.find(L"Press F")==std::wstring::npos,"controller approach names only its face control");
    for(auto status:{route::Status::NoRoute,route::Status::Unsupported,route::Status::Limit,route::Status::Unavailable}) {
        beacon.Stop();
        beacon.Toggle(target,3000);Result(status,3100);cue=beacon.Update(s,target,3100);
        Check(!cue.audible&&beacon.Enabled()&&cue.speech==L"Switching to direct target. Direction only, not a clear route.","failed route announces direct fallback before any target cue");
        Check(!beacon.Update(s,target,4099).audible,"fallback gives the announcement a head start");
        target.position={0,500,44};cue=beacon.Update(s,target,4100);
        Check(cue.audible&&cue.distance==500&&cue.bearing>1.5f&&cue.speech.empty(),"fallback uses fresh target bearing and never repeats the announcement");
        target.position={400,0,44};
    }
    const auto beforeRetry=requests;
    beacon.Toggle(target,4200);Check(!beacon.Enabled(),"fallback toggles off normally");
    beacon.Toggle(target,4300);Check(requests==beforeRetry+1&&!beacon.Update(s,target,4301).audible,"off then on retries routing instead of retaining fallback");
    Result(route::Status::NoRoute,4400);beacon.Update(s,target,4400);
    auto other=target;other.stableId="different";
    beacon.Retarget(other,4500);
    Check(requested=="different"&&requests==beforeRetry+2&&!beacon.Update(s,other,4501).audible,"new target retries routing after fallback");
    beacon.Stop();beacon.Toggle(target,4600);Result(route::Status::NoRoute,4700);beacon.Update(s,target,4700);
    s.active=false;cue=beacon.Update(s,target,5800);
    Check(!cue.audible&&!beacon.Enabled(),"inactive navigation stops fallback instead of preserving direct audio");s.active=true;
    beacon.Toggle(target,6000);Result(route::Status::NoRoute,6100);beacon.Update(s,target,6100);
    s.interactionBusy=true;cue=beacon.Update(s,target,7200);
    Check(!cue.audible&&!beacon.Enabled(),"interaction also stops fallback");s.interactionBusy=false;
    beacon.Stop();
    beacon.Toggle(target,4000);Result(route::Status::Arrived,4100);s.focusedId=target.stableId;s.actions[0].label=L"Open";
    cue=beacon.Update(s,target,4100);
    Check(!beacon.Enabled()&&cue.speech.empty(),"arrival does not interrupt focused action labels");
    target.kind=world::Kind::Destination;target.label=L"Exit.";
    beacon.Toggle(target,4200);Result(route::Status::Arrived,4300);cue=beacon.Update(s,target,4300);
    Check(cue.speech==L"Reached Exit.","location completion has one period");
    beacon.Toggle(target,5000);target.stableId="other";beacon.Retarget(target,5001);
    Check(requested=="other"&&!beacon.Update(s,target,5002).audible,"selection change cannot play old result");
    beacon.Stop();beacon.Configure(false);const auto before=requests;
    Check(beacon.Toggle(target,6000).find(L"Direction only")!=std::wstring::npos&&requests==before,"explicit direct mode does not request routing");
    cue=beacon.Update(s,target,6100);Check(cue.audible&&cue.distance==400,"direct mode retains original bearing behaviour");
    s.interactionBusy=true;cue=beacon.Update(s,target,6101);Check(!cue.audible&&!beacon.Enabled(),"interaction invalidates UI guidance");
    Check(cancels>0,"cancellation reaches runtime");
    std::cout<<"beacon UI lifecycle PASS\n";
}
