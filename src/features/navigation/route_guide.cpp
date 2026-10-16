#include "features/navigation/route_guide.h"
#include <algorithm>
#include <chrono>
#include <limits>

namespace lis::navigation::route {
namespace {
float Distance(world::Vec3 a,world::Vec3 b) { return std::hypot(a.x-b.x,a.y-b.y); }
world::Vec3 Body(routing::Vec3 p,float height) {
    return {static_cast<float>(p.x),static_cast<float>(p.y),static_cast<float>(p.z)+height};
}
bool Same(Identity a,Identity b) {
    return a.controller==b.controller&&a.pawn==b.pawn&&a.world==b.world;
}
bool SamePoint(world::Vec3 a,world::Vec3 b) { return a.x==b.x&&a.y==b.y&&a.z==b.z; }
bool ExitPlane(const world::Target& target,float actualOriginZ) {
    const auto e=target.volumeExtent;
    return FinitePoint(e)&&e.x>0&&e.y>0&&e.z>0&&
        std::abs(double(actualOriginZ)-target.position.z)<=double(e.z);
}
bool InsideExit(const world::Target& target,world::Vec3 player) {
    return ExitPlane(target,player.z)&&
        std::abs(double(player.x)-target.position.x)<=double(target.volumeExtent.x)&&
        std::abs(double(player.y)-target.position.y)<=double(target.volumeExtent.y);
}
Status Failure(Clearance c) {
    return c==Clearance::Unknown||c==Clearance::Invalid?Status::Unavailable:Status::Unsupported;
}
// Bounded shortcut verification. Every piece retains the original swept body
// plus midpoint/end floor checks; no visibility-only diagonal shortcuts.
Clearance Connect(world::Vec3 a,world::Vec3 b,const Ground& g,const Trace& trace,
                  unsigned& budget) {
    if(std::fabs(a.z-b.z)>1)return Clearance::Unsupported;
    const unsigned pieces=std::max(1u,static_cast<unsigned>(std::ceil(Distance(a,b)/(2*g.radius))));
    if(pieces>budget)return Clearance::Invalid;
    const auto start=a;
    for(unsigned i=1;i<=pieces;++i) {
        const float t=static_cast<float>(i)/static_cast<float>(pieces);
        world::Vec3 next{start.x+(b.x-start.x)*t,start.y+(b.y-start.y)*t,b.z};
        --budget;
        const auto result=CheckFlatSegment(a,next,g.radius,g.halfHeight,trace,g.headMargin);
        if(result.status!=Clearance::Clear)return result.status;
        a=result.end;
    }
    return Clearance::Clear;
}
float SegmentDistance(world::Vec3 p,world::Vec3 a,world::Vec3 b) {
    const float dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;
    const float t=den>0?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/den,0.f,1.f):0;
    return Distance(p,{a.x+t*dx,a.y+t*dy,a.z});
}
}
void Guide::Reset() { *this=Guide{}; }
void Guide::Start(const std::string& selected) {
    Reset();selected_=selected;status_=selected.empty()?Status::Off:Status::Planning;
}
Result Guide::Fail(Status status,const char* cause) {
    // A repeated failure must not replace retained evidence with reset zeros.
    if(planner_.GetStatus()!=routing::Status::Invalid)diagnostic_.stats=planner_.Stats();
    diagnostic_.cause=cause;
    planner_.Reset();status_=status;return {status,{},false,replanning_};
}
bool Guide::Begin(const world::Target& target,const Ground& ground,uint64_t now) {
    if(diagnostic_.searches)++diagnostic_.restarts;
    if(replanning_&&++restarts_>8){Fail(Status::Limit,"restart_limit");return false;}
    if(!planningSince_)planningSince_=now;
    target_=target.position;volumeExtent_=target.volumeExtent;approachRadius_=target.approachRadius;
    kind_=target.kind;ground_=ground;
    const bool interactive=world::NeedsInteractionFocus(kind_);
    // Location Z must describe either this floor or this calibrated actor-origin
    // plane. Arbitrary elevated locations cannot silently become XY goals.
    // Exit Z is a brush centre, not a point destination. Tick independently
    // validates its bounds against the actual (not normalized body) origin Z.
    if(!interactive&&kind_!=world::Kind::Exit&&std::fabs(target_.z-ground.floor.z)>2&&
       std::fabs(target_.z-ground.body.z-ground.headMargin)>2) {
        Fail(Status::Unsupported,"target_height");return false;
    }
    if(interactive&&std::fabs(target_.z-ground.body.z)>ground.halfHeight*3) {
        Fail(Status::Unsupported,"target_height");return false;
    }
    routing::Goal goal{{target_.x,target_.y,ground.floor.z},0,
        interactive?std::nextafter(double(approachRadius_),0.0):std::max(32.0,double(ground.radius)*1.5),1};
    routing::Config config;config.maxMilliseconds=100;config.maxEdges=4096;config.maxNodes=1024;
    config.maxCells=32;
    BeginOnGround(planner_,ground,goal,config);status_=Status::Planning;
    ++diagnostic_.searches;diagnosticStart_=now;diagnostic_.searchAge=0;
    diagnostic_.stats=planner_.Stats();diagnostic_.cause=planner_.StopReason();
    return true;
}
Result Guide::Tick(const world::Snapshot& s,Identity id,uint64_t now,const Trace& trace) {
    diagnostic_.player=s.player;diagnostic_.ground={};diagnostic_.target={};
    if(selected_.empty())return {};
    const auto target=std::find_if(s.targets.begin(),s.targets.end(),[&](const world::Target& t){return t.stableId==selected_;});
    if(target!=s.targets.end())diagnostic_.target=target->position;
    if(planner_.GetStatus()!=routing::Status::Invalid&&now>=diagnosticStart_)
        diagnostic_.searchAge=now-diagnosticStart_;
    if(clocked_&&now<lastTime_)return Fail(Status::Unavailable,"backward_clock");
    lastTime_=now;clocked_=true;
    if(!s.active||s.interactionBusy||!id.controller||!id.pawn||!id.world||!trace||
       !FinitePoint(s.player)||!ValidBody(s.radius,s.halfHeight)||target==s.targets.end()||
       !FinitePoint(target->position)||s.interactionsFailed||s.exitsFailed) {
        sampled_=false;planningSince_=0;restarts_=0;return Fail(Status::Unavailable,"bad_context");
    }
    const auto ground=CalibrateGround(s.player,s.radius,s.halfHeight,trace);
    diagnostic_.ground=ground;
    if(ground.status!=Clearance::Clear)return Fail(Failure(ground.status),"ground");
    if(world::NeedsInteractionFocus(target->kind)&&
       (!std::isfinite(target->approachRadius)||target->approachRadius<=0)) {
        approachRadius_=target->approachRadius;
        return Fail(Status::Unsupported,"approach_zone_unavailable");
    }
    if(target->kind==world::Kind::Exit&&!ExitPlane(*target,s.player.z))return Fail(Status::Unsupported,"exit_height");
    const bool changed=!sampled_||!Same(id,identity_);
    const bool moved=sampled_&&Distance(lastPlayer_,s.player)>s.radius*8;
    const bool boundsChanged=target->kind==world::Kind::Exit&&
        (!SamePoint(volumeExtent_,target->volumeExtent)||!SamePoint(target_,target->position));
    const bool zoneChanged=world::NeedsInteractionFocus(target->kind)&&approachRadius_!=target->approachRadius;
    const bool relocated=zoneChanged||boundsChanged||Distance(target_,target->position)>12||std::fabs(target_.z-target->position.z)>2||kind_!=target->kind;
    const bool bodyChanged=ground_.radius!=ground.radius||ground_.halfHeight!=ground.halfHeight||
        std::fabs(ground_.floor.z-ground.floor.z)>1||std::fabs(ground_.headMargin-ground.headMargin)>1;
    identity_=id;lastPlayer_=s.player;sampled_=true;
    if(changed) {planningSince_=0;restarts_=0;}
    const bool planningMoved=status_==Status::Planning&&Distance(ground_.body,ground.body)>s.radius;
    if(changed||moved||relocated||bodyChanged||planningMoved||status_==Status::Unavailable) {
        diagnostic_.restartCause=!diagnostic_.searches?"initial":changed?"identity":moved?"player_jump":
            relocated?"target":bodyChanged?"body":planningMoved?"planning_movement":"recovery";
        replanning_=planner_.GetStatus()!=routing::Status::Invalid||changed;
        if(!Begin(*target,ground,now))return {status_};
    }
    if(status_==Status::Planning) {
        if(now-planningSince_>15000)return Fail(Status::Limit,"wall_timeout");
        bool unreadable=false;
        const auto state=planner_.Step([&](const routing::EdgeQuery& q) {
            const auto result=ProbeFloorEdge(q,ground_,[&](world::Vec3 a,world::Vec3 b,world::Vec3 e) {
                const auto hit=trace(a,b,e);if(!ValidHit(hit))unreadable=true;return hit;
            });
            return result;
        },PlanningWorkPerTick,PlanningSliceMilliseconds);
        diagnostic_.stats=planner_.Stats();diagnostic_.cause=planner_.StopReason();
        if(unreadable)return Fail(Status::Unavailable,"query_failure");
        // Planner persists unknown-edge evidence across ticks and returns Limit
        // on exhausted unknown search, while still exploring supported detours.
        if(state==routing::Status::NoRoute)return Fail(Status::NoRoute,planner_.StopReason());
        if(state==routing::Status::Limit)return Fail(Status::Limit,planner_.StopReason());
        if(state==routing::Status::Invalid)return Fail(Status::Unavailable,planner_.StopReason());
        if(state==routing::Status::Found) {
            status_=Status::Following;planningSince_=0;
            // Keep planning and shortcut work in separate ticks.
        }
        return {Status::Planning,{},false,replanning_};
    }
    if(status_!=Status::Following&&status_!=Status::Approach&&status_!=Status::Arrived)return {status_};
    return Follow(s,*target,ground,trace);
}
Result Guide::Follow(const world::Snapshot& s,const world::Target& target,const Ground& g,const Trace& trace) {
    const auto& path=planner_.Path();
    if(path.empty())return Fail(Status::Unavailable,"invalid_path");
    const auto end=Body(path.back(),g.halfHeight);
    const bool interactive=world::NeedsInteractionFocus(target.kind);
    float nearest=std::numeric_limits<float>::max();std::size_t index=0;
    for(std::size_t i=0;i<path.size();++i) {
        const auto a=Body(path[i],g.halfHeight),b=Body(path[i?i-1:i],g.halfHeight);
        const float d=SegmentDistance(g.body,a,b);
        if(d<=nearest){nearest=d;index=i;}
    }
    auto replan=[&](const char* cause) {
        diagnostic_.restartCause=cause;
        replanning_=true;
        if(!Begin(target,g,lastTime_))return Result{status_};
        return Result{Status::Planning,{},false,true};
    };
    if(nearest>g.radius*2)return replan("off_route");
    unsigned budget=8;
    const bool exit=target.kind==world::Kind::Exit;
    if(Distance(g.body,end)<=24||(exit&&Distance(g.body,target.position)<=24)) {
        auto destination=interactive?end:world::Vec3{target.position.x,target.position.y,g.body.z};
        const auto clear=Connect(g.body,destination,g,trace,budget);
        if(clear==Clearance::Blocked)return replan("obstruction");
        if(clear!=Clearance::Clear)return Fail(Failure(clear),clear==Clearance::Unknown?"query_failure":"geometry_failure");
        if(interactive) {
            // The endpoint is inside the strict inner zone, but the 24-unit
            // waypoint tolerance alone cannot establish actual zone membership.
            if(std::hypot(double(s.player.x)-target.position.x,double(s.player.y)-target.position.y)>=target.approachRadius)
                return {Status::Following,end,true,replanning_};
            bool action=false;for(const auto& a:s.actions)action|=!a.label.empty();
            status_=s.focusedId==selected_&&action?Status::Arrived:Status::Approach;
            restarts_=0;
            return {status_};
        }
        if(exit?InsideExit(target,s.player):Distance(g.body,destination)<=24) {
            status_=Status::Arrived;return {status_};
        }
        return {Status::Following,destination,true,replanning_};
    }
    // Look ahead up to three diameters; verify from the ACTUAL player each tick.
    // Failed far candidates fall back to a nearer corner within the same budget.
    std::size_t far=index;
    for(std::size_t i=index;i<path.size()&&i<=index+3;++i)
        if(Distance(g.body,Body(path[i],g.halfHeight))<=g.radius*6)far=i;
    for(std::size_t i=far;;--i) {
        const auto waypoint=Body(path[i],g.halfHeight);
        const float distance=Distance(g.body,waypoint);
        if(distance>4&&distance<=g.radius*6&&std::ceil(distance/(2*g.radius))<=budget) {
            const auto clear=Connect(g.body,waypoint,g,trace,budget);
            if(clear==Clearance::Clear) {
                status_=Status::Following;replanning_=false;restarts_=0;return {status_,waypoint,true,false};
            }
            if(clear!=Clearance::Blocked&&clear!=Clearance::Unsupported)
                return Fail(Failure(clear),clear==Clearance::Unknown?"query_failure":"geometry_failure");
        }
        if(i==index)break;
    }
    return replan("obstruction");
}
}
