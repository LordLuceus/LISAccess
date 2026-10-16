#include "features/camera/facing_policy.h"
#include <iostream>
#include <limits>
using namespace lis::world;
using namespace lis::world::facing_policy;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n";return 1;}}while(false)
Frame Base() {Frame f;f.target={100,100,20};return f;}
int main() {
    // artifacts/facing-capture-first.json final actual ViewTarget, not the pre-turn origin.
    struct Capture {Vec3 origin,target,oldOrigin;Rotation actual,gp;int yaw,pitch;};
    const Capture captures[]={
        {{-316.513885f,-801.717468f,139.426834f},{-215.576111f,-718.122192f,67.895187f},
         {-280.893036f,-832.514221f,106.952065f},{-2902,11260,0},{-3063,11224,0},7179,-5372},
        {{-290.726837f,-794.044617f,161.608429f},{-299.972656f,-700.347046f,71.103050f},
         {-316.513885f,-801.717468f,139.426834f},{-5690,14600,0},{-6178,14544,0},17354,-8474},
        {{-277.652802f,-755.368958f,173.582947f},{-321.055573f,-732.215881f,17.771074f},
         {-290.726837f,-794.044617f,161.608429f},{-10136,21133,0},{-12198,21096,0},27620,-14563}
    };
    for(const auto& c:captures) {
        auto a=Correct(c.origin,c.actual,c.gp,c.target,-14563,8192,true);
        CHECK(a.known&&a.wanted.yaw==c.yaw&&a.wanted.pitch==c.pitch);
        auto old=Correct(c.oldOrigin,c.actual,c.gp,c.target,-14563,8192,true);
        CHECK(std::abs(Signed(old.wanted.yaw-a.wanted.yaw))>2000);
    }
    CHECK(Signed(65535)==-1&&Signed(-65537)==-1&&Signed(32768)==-32768);
    CHECK(SameControl({0,(std::numeric_limits<int>::max)(),0},{0,-1,0}));
    CHECK(Applied({0,(std::numeric_limits<int>::min)(),0},{0,0,0}));
    auto wrap=Correct({},{0,32760,0},{0,32760,0},{-100,-1,0},-1000,1000,true);
    CHECK(wrap.known&&std::abs(wrap.yawError)<120&&wrap.wanted.yaw<0);
    auto clamp=Correct({},{},{},{100,0,100},-100,100,true);
    CHECK(clamp.rawPitch==8192&&clamp.wanted.pitch==100);
    CHECK(Correct({},{},{},{100,0,-100},-100,100,true).wanted.pitch==-100);
    CHECK(!Correct({},{},{},{100,0,0},100,-100,true).known);
    CHECK(!Correct({},{},{},{100,0,0},-20000,100,true).known);
    CHECK(!Correct({},{},{},{},-100,100,true).known);
    CHECK(!Correct({},{},{},{std::numeric_limits<float>::quiet_NaN(),0,0},-100,100,true).known);
    CHECK(Applied({20,97,0},{20,100,0}));
    CHECK(!Applied({20,96,0},{20,100,0})&&!Applied({20,101,0},{20,100,0}));
    CHECK(!Applied({21,100,0},{20,100,0}));
    Policy p;auto f=Base();p.Begin(f);CHECK(p.Step(f)==Result::Request);p.Submitted(f);
    for(int ticks=3;ticks>=1;--ticks) {
        f.now+=50;f.forced=true;f.remaining=ticks;
        CHECK(p.Step(f)==Result::Wait&&p.requests==1&&!p.applied);
    }
    f.gp=p.wanted;f.gp.yaw-=3;
    CHECK(p.Step(f)==Result::Wait&&!p.applied);
    f.forced=false;f.remaining=0;f.now=200;
    CHECK(p.Step(f)==Result::Wait);
    f.now=399;CHECK(p.Step(f)==Result::Wait&&p.applied);
    // POV jitter/orbit is not manual GP input; correction uses the latest pose.
    f.actual={-200,500,0};f.origin={10,-20,5};f.now=400;
    CHECK(p.Step(f)==Result::Request);
    CHECK(p.aim.wanted.yaw==Correct(f.origin,f.actual,f.gp,f.target,f.low,f.high,true).wanted.yaw);
    // Actual Camera B success (fourth capture) is ~14.7 degrees off-axis.
    f=Base();p.Begin(f);f.origin={-271.202850f,-722.322754f,173.553146f};
    f.target=captures[0].target;f.actual={-10212,5595,0};f.gp={-12145,5600,0};f.focused=true;
    CHECK(std::abs(Correct(f.origin,f.actual,f.gp,f.target,f.low,f.high,true).yawError)>182);
    CHECK(p.Step(f)==Result::Focused&&!p.running);
    f=Base();p.Begin(f);f.player.x=0.26f;CHECK(p.Step(f)==Result::Cancelled);
    // Adapter supplies context=false on camera, controller, world, pawn, target,
    // behaviour or settings identity change (including a recycled same address).
    Context context;
    for(auto member:{&Context::controller,&Context::world,&Context::pawn,&Context::camera,&Context::playerCamera,
                     &Context::behaviour,&Context::target,&Context::settings}) {
        auto changed=context;(changed.*member).address=1;
        f=Base();p.Begin(f);f.context=context.Same(changed);CHECK(p.Step(f)==Result::Cancelled);
        changed=context;(changed.*member).index=1;
        CHECK(!context.Same(changed)); // same address, recycled UObject identity
    }
    f=Base();p.Begin(f);p.Step(f);p.Submitted(f);f.gp.yaw=4;
    CHECK(p.Step(f)==Result::Cancelled&&!p.applied);
    f=Base();p.Begin(f);p.Step(f);p.Submitted(f);f.gp=p.wanted;p.Step(f);
    f.gp.yaw+=4;CHECK(p.Step(f)==Result::Cancelled);
    f=Base();p.Begin(f);f.known=false;CHECK(p.Step(f)==Result::Unavailable);
    f=Base();p.Begin(f);p.Step(f);p.Submitted(f);f.forced=true;f.remaining=3;f.now=2500;
    CHECK(p.Step(f)==Result::Limited&&!p.applied); // no false "Turned" report
    f=Base();p.Begin(f);p.requests=Policy::MaxCorrections+1;p.settled=f.gp;
    CHECK(p.Step(f)==Result::Limited);
    f=Base();p.Begin(f);f.target={100,0,0};CHECK(p.Step(f)==Result::Aligned); // absent native focus
    f=Base();f.target={100,0,100};f.high=100;f.gp.pitch=100;p.Begin(f);
    CHECK(p.Step(f)==Result::Limited); // identical clamped request cannot help
    f=Base();f.pitch=false;f.gp.pitch=-6000;p.Begin(f);CHECK(p.Step(f)==Result::Request);
    CHECK(p.aim.wanted.pitch==-6000); // Destination/Exit yaw only
    f=Base();p.Begin(f);p.Step(f);p.Submitted(f);f.gp=p.wanted;f.now=200;p.Step(f);
    f.now=400;p.Step(f);p.Submitted(f);f.gp=p.wanted;f.now=600;p.Step(f);
    f.now=800;CHECK(p.Step(f)==Result::Request); // no premature one-step progress cutoff
    std::cout<<"PASS facing policy\n";
}
