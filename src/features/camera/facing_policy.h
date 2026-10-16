#pragma once
#include "features/world/world.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace lis::world::facing_policy {
// Opaque main-thread identity tokens, never dereferenced by the policy.
struct Identity {
    uintptr_t address=0,cls=0;int index=0,name=0,number=0;
    bool operator==(const Identity& b) const {return address==b.address&&cls==b.cls&&index==b.index&&name==b.name&&number==b.number;}
};
struct Context {
    Identity controller,world,pawn,camera,playerCamera,behaviour,target,settings;
    bool Same(const Context& b) const {
        return controller==b.controller&&world==b.world&&pawn==b.pawn&&camera==b.camera&&playerCamera==b.playerCamera&&
            behaviour==b.behaviour&&target==b.target&&settings==b.settings;
    }
};
struct Rotation {int pitch=0,yaw=0,roll=0;};
inline int Signed(int v) {int n=static_cast<int>(static_cast<unsigned>(v)&65535u);return n>=32768?n-65536:n;}
inline bool Finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline double Distance(Vec3 a,Vec3 b){return std::hypot(std::hypot(double(a.x)-b.x,double(a.y)-b.y),double(a.z)-b.z);}
struct Aim {bool known=false;Rotation wanted;int rawPitch=0,yawError=0,pitchError=0;};
inline Aim Correct(Vec3 origin,Rotation actual,Rotation gp,Vec3 target,int low,int high,bool pitch) {
    Aim a;
    if(!Finite(origin)||!Finite(target)||Distance(origin,target)<1||low>high||low< -16384||high>16384)return a;
    constexpr double units=65536.0/(2*3.14159265358979323846);
    const double x=double(target.x)-origin.x,y=double(target.y)-origin.y,z=double(target.z)-origin.z;
    a.known=true;
    a.yawError=Signed(int(std::lround(std::atan2(y,x)*units))-Signed(actual.yaw));
    a.pitchError=Signed(int(std::lround(std::atan2(z,std::hypot(x,y))*units))-Signed(actual.pitch));
    a.rawPitch=Signed(gp.pitch)+a.pitchError;
    a.wanted={pitch?std::clamp(a.rawPitch,low,high):gp.pitch,Signed(Signed(gp.yaw)+a.yawError),gp.roll};
    return a;
}
// Verified GP yaw rounds DOWN by 0..3 units; POV idle jitter is not input.
inline bool Applied(Rotation actual,Rotation requested) {
    const int yaw=Signed(Signed(requested.yaw)-Signed(actual.yaw));
    return yaw>=0&&yaw<=3&&Signed(Signed(actual.pitch)-Signed(requested.pitch))==0&&Signed(Signed(actual.roll)-Signed(requested.roll))==0;
}
inline bool SameControl(Rotation a,Rotation b) {
    return Signed(Signed(a.yaw)-Signed(b.yaw))==0&&Signed(Signed(a.pitch)-Signed(b.pitch))==0&&Signed(Signed(a.roll)-Signed(b.roll))==0;
}
enum class Result {Wait,Request,Focused,Cancelled,Unavailable,Aligned,Limited};
struct Frame {
    uint64_t now=0;
    bool context=true,focused=false,known=true,forced=false,pitch=true;
    int remaining=0;
    Vec3 player,origin,target;
    Rotation actual,gp;
    int low=-16384,high=16384;
};
struct Policy {
    static constexpr unsigned MaxCorrections=8;
    static constexpr uint64_t Deadline=2500,Interval=200;
    bool running=false,applied=false,pending=false,consumed=false,requestSeen=false;
    unsigned requests=0;
    uint64_t start=0,consumedAt=0;
    Vec3 player;
    Rotation before,wanted,settled;
    Aim aim;
    void Begin(const Frame& f) { *this={};running=true;start=f.now;player=f.player;settled=f.gp; }
    void Submitted(const Frame& f) {
        before=f.gp;wanted=aim.wanted;pending=true;consumed=false;requestSeen=false;++requests;
    }
    Result End(Result r){running=false;return r;}
    Result Step(const Frame& f) {
        if(!running)return Result::Wait;
        if(!f.context||!Finite(f.player)||Distance(player,f.player)>0.25)return End(Result::Cancelled);
        if(f.focused)return End(Result::Focused);
        if(!f.known)return End(Result::Unavailable);
        if(pending) {
            if(Applied(f.gp,wanted)){requestSeen=true;settled=f.gp;}
            else if(requestSeen||!SameControl(f.gp,before))return End(Result::Cancelled);
        } else if(requests&&!SameControl(f.gp,settled))return End(Result::Cancelled);
        if(f.now-start>=Deadline)return End(Result::Limited);
        // Never reset the native dampers while the three forced ticks are pending.
        if(f.forced||f.remaining>0)return Result::Wait;
        if(pending) {
            if(!Applied(f.gp,wanted))return Result::Wait;
            pending=false;consumed=true;applied=true;consumedAt=f.now;
            return Result::Wait; // observe a later actual frame before correcting
        }
        if(consumed&&f.now-consumedAt<Interval)return Result::Wait;
        aim=Correct(f.origin,f.actual,f.gp,f.target,f.low,f.high,f.pitch);
        if(!aim.known)return End(Result::Unavailable);
        if(std::abs(aim.yawError)<=182&&(!f.pitch||std::abs(aim.pitchError)<=182))return End(Result::Aligned);
        // A saturated pitch alone cannot be repaired by repeating the setter.
        if(Applied(f.gp,aim.wanted))return End(Result::Limited);
        if(requests>=MaxCorrections+1)return End(Result::Limited);
        return Result::Request;
    }
};
}
