#include "features/dialogue/policy.h"
#include "features/dialogue/bindings.h"
#include <iostream>
using namespace lis::dialogue;

// Passive capture: CurrentDialogId E1_2A_ArtClass_CHJefferson_Phase01,
// page 0, ChoiceIndexChoosen -1, neither current nor total timeout.
// Native bindings: widget slot 1 -> choice 0, widget slot 2 -> choice 1.
// Empty strings at slots 0 and 3 must not compact the native slot mapping.
static Snapshot Jefferson() {
    Snapshot s;
    s.context=true;s.visible=true;
    s.identity="E1_2A_ArtClass_CHJefferson_Phase01|page=0|generation=0";
    s.choices[0]={L"",L"",false};
    s.choices[1]={L"$GT_HUD_Dial_E1_2A_ArtClass_CHJefferson_TalkJefferson_003",L"You're asking me?",true};
    s.choices[2]={L"$GT_HUD_Dial_E1_2A_ArtClass_CHJefferson_TalkJefferson_004",L"I did know...",true};
    s.choices[3]={L"",L"",false};
    return s;
}

int main() {
    int errors=0;
    auto check=[&](bool ok,const char* what){std::cout<<(ok?"PASS: ":"FAIL: ")<<what<<'\n';errors+=!ok;};
    const BindingEntry captured[]={{1,0,-1,0},{2,1,0,0}};
    std::array<int,4> indices{};
    check(DecodeBindings(captured,2,3,0,indices)&&indices==std::array<int,4>{-1,0,1,-1},"real captured sparse map selects X/B, not compacted Y/X");
    const BindingEntry hole[]={{99,99,0,0},{2,1,-1,0}};
    check(DecodeBindings(hole,2,2,1,indices)&&indices[2]==1&&indices[0]==-1,"free sparse entries are never offered");
    check(!DecodeBindings(hole,2,3,0,indices),"out of range allocated binding is rejected");
    const BindingEntry duplicate[]={{1,0,-1,0},{1,1,0,0}};
    check(!DecodeBindings(duplicate,2,3,0,indices),"duplicate slot cannot replace a bound answer");
    check(!DecodeBindings(captured,2,1,0,indices),"allocation/free-count mismatch fails closed");
    const auto fixture=Jefferson();
    const std::wstring spoken=L"2: You're asking me?\n3: I did know...";
    check(Describe(fixture)==spoken,"captured Jefferson answers retain native slots, prose and punctuation");
    auto pad=fixture;pad.gamepad=true;
    check(Describe(pad)==L"X: You're asking me?\nB: I did know...","controller readout contains only the native face buttons");
    check(CanActivate(fixture,pad,1,true,false)&&CanActivate(pad,fixture,1,true,false),"changing input device does not invalidate an unchanged choice");
    check(!fixture.timed,"capture with no current/total timeout is untimed");
    check(CanActivate(fixture,fixture,1,true,false)&&CanActivate(fixture,fixture,2,true,false),"readable native slots 1 and 2 can activate");
    check(!CanActivate(fixture,fixture,0,true,false)&&!CanActivate(fixture,fixture,3,true,false),"captured unoffered outer slots cannot activate");
    auto emptyOffered=fixture;emptyOffered.choices[0].offered=true;
    check(!CanActivate(emptyOffered,emptyOffered,0,true,false)&&Describe(emptyOffered)==spoken,"even an offered empty slot cannot activate");
    check(!CanActivate(fixture,fixture,-1,true,false)&&!CanActivate(fixture,fixture,4,true,false),"out of range native slots fail closed");
    check(!CanActivate(fixture,fixture,1,false,false),"background activation is blocked");
    check(!CanActivate(fixture,fixture,1,true,true),"menu overlay blocks activation");
    auto changed=fixture;changed.visible=false;
    check(Describe(changed).empty()&&!CanActivate(fixture,changed,1,true,false),"hidden dialogue cannot announce or activate");
    check(!CanActivate(changed,fixture,1,true,false),"hidden expected snapshot is not activation authority");
    changed=fixture;changed.context=false;
    check(Describe(changed).empty()&&!CanActivate(fixture,changed,1,true,false),"visible stale widget outside dialogue is rejected");
    check(!CanActivate(changed,fixture,1,true,false),"expected snapshot must also be dialogue context");
    changed=fixture;changed.identity.clear();
    check(!CanActivate(changed,changed,1,true,false)&&!CanActivate(changed,fixture,1,true,false),"empty identity never authorizes activation");
    changed=fixture;changed.identity="E1_2A_ArtClass_CHJefferson_Phase01|page=1|generation=0";
    check(!CanActivate(fixture,changed,1,true,false),"changed page identity rejects even identical answers");
    changed=fixture;changed.identity="E1_2A_ArtClass_CHJefferson_Phase01|page=0|generation=1";
    check(!CanActivate(fixture,changed,1,true,false),"rewind or reopen generation rejects a stale snapshot");
    changed=fixture;changed.choices[2].key+=L"_changed";
    check(!CanActivate(fixture,changed,1,true,false),"changed binding in another offered slot invalidates the snapshot");
    changed=fixture;changed.choices[2].text+=L" ";
    check(Describe(changed)==spoken&&!CanActivate(fixture,changed,1,true,false),"raw text changes fail closed even when cleaned speech is identical");
    changed=fixture;changed.choices[2].offered=false;
    check(!CanActivate(fixture,changed,1,true,false),"changed availability in another slot invalidates the snapshot");
    check(!CanActivate(changed,changed,2,true,false)&&Describe(changed)==L"2: You're asking me?","unoffered readable choice is neither spoken nor activated");
    changed=fixture;changed.choices[1].text=L" \t\n\u00a0";
    check(!CanActivate(changed,changed,1,true,false)&&Describe(changed)==L"3: I did know...","whitespace-only answer is unavailable without renumbering");
    changed=fixture;changed.choices[1].text=changed.choices[1].key;
    check(!CanActivate(changed,changed,1,true,false)&&Describe(changed)==L"3: I did know...","unresolved localization token is not a readable answer");
    changed=fixture;changed.choices={};changed.timed=true;
    check(Describe(changed).empty()&&!CanActivate(changed,changed,1,true,false),"empty timed dialogue emits no misleading announcement");
    check(Describe(Snapshot{}).empty(),"default snapshot is silent");
    changed=fixture;changed.timed=true;
    check(Describe(changed)==L"Timed choice\n"+spoken,"timed prefix preserves exact answer prose");
    check(!CanActivate(fixture,changed,1,true,false),"changed timed state fails closed");
    // Synthetic layout using only captured prose, not additional game answers.
    changed=fixture;
    for(auto& choice:changed.choices)choice=fixture.choices[1];
    check(Describe(changed)==L"1: You're asking me?\n2: You're asking me?\n3: You're asking me?\n4: You're asking me?","all four keyboard controls stay fixed and duplicate prose does not erase slots");
    changed.gamepad=true;
    check(Describe(changed)==L"Y: You're asking me?\nX: You're asking me?\nB: You're asking me?\nA: You're asking me?","all four controller controls stay fixed");
    check(CanActivate(changed,changed,0,true,false)&&CanActivate(changed,changed,3,true,false),"readable outer slots use zero-based native indices");
    return errors?1:0;
}
