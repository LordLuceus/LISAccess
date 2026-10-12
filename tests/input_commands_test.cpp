#include "input_commands.h"
#include <iostream>
#include <algorithm>
using namespace lis::input;
int main() {
    int failures=0;
    auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS: ":"FAIL: ")<<name<<'\n';if(!ok)++failures;};
    auto idx=[](Command c){return static_cast<size_t>(c);};
    Keyboard k;Presses raw{};
    check(std::find(Keys.begin(),Keys.end(),0x23)==Keys.end(),"End remains native diary");
    raw[idx(Command::Describe)]=raw[idx(Command::Beacon)]=true;
    auto p=k.Update(raw,{},true,0);
    check(p[idx(Command::Describe)]&&!p[idx(Command::Beacon)],"plain Home describes only");
    k.Update({}, {},true,1);
    p=k.Update(raw,{true,false,false},true,2);
    check(!p[idx(Command::Describe)]&&p[idx(Command::Beacon)],"Shift Home toggles only beacon");
    p=k.Update(raw,{},true,3);
    check(std::none_of(p.begin(),p.end(),[](bool b){return b;}),"releasing modifier does not fire held Home");
    k={};raw={};raw[idx(Command::Next)]=true;
    check(k.Update(raw,{},true,0)[idx(Command::Next)],"scanner first press");
    check(!k.Update(raw,{},true,449)[idx(Command::Next)],"repeat waits for initial delay");
    check(k.Update(raw,{},true,450)[idx(Command::Next)],"scanner repeats while held");
    check(!k.Update(raw,{},true,629)[idx(Command::Next)]&&k.Update(raw,{},true,630)[idx(Command::Next)],"repeat cadence");
    k={};raw={};raw[idx(Command::Action1)]=true;
    check(k.Update(raw,{},true,0)[idx(Command::Action1)]&&!k.Update(raw,{},true,5000)[idx(Command::Action1)],"actions never auto-repeat");
    k={};k.Update(raw,{},false,0);p=k.Update(raw,{},true,10);
    check(!p[idx(Command::Action1)],"focus return cannot activate held action");
    k={};p=k.Update(raw,{false,true,false},true,0);
    check(!p[idx(Command::Action1)],"modified game controls are left alone");
    Controller pad;
    p=pad.Update(8,true,0);check(p[idx(Command::Next)],"controller cycles target");
    check(pad.Update(8,true,450)[idx(Command::Next)],"controller target repeat");
    pad={};pad.Update(0x40,true,0);p=pad.Update(0,true,1);
    check(p[idx(Command::Face)],"left stick click faces on release");
    pad={};pad.Update(0x40,true,0);p=pad.Update(0x42,true,1);
    check(p[idx(Command::Obstacles)]&&!p[idx(Command::Beacon)],"controller modifier selects obstacle toggle only");
    p=pad.Update(0,true,2);check(!p[idx(Command::Face)],"modifier chord cannot also face on release");
    pad={};pad.Update(0x40,false,0);pad.Update(0x40,true,1);p=pad.Update(0,true,2);
    check(!p[idx(Command::Face)],"held controller modifier cannot face after focus return");
    pad={};pad.Update(0x40,true,0);p=pad.Update(0,false,1);
    check(!p[idx(Command::Face)],"disconnect is not a face-button release");
    pad={};p=pad.Update(0xf000,true,0);
    check(std::none_of(p.begin(),p.end(),[](bool b){return b;}),"native face buttons are never synthesized or doubled");
    return failures?1:0;
}
