#include "features/rewind/policy.h"
#include "features/prompts/policy.h"
#include <iostream>
#include <limits>
using namespace lis::rewind;
// Synthetic boundary samples for the verified native ReferenceTime/end and
// LastPlayerAction.time/end formulas; not a claimed captured conversation.
Frame At(unsigned state,float reference,float marker=60) {
    Frame f;f.context=f.valid=f.visible=f.markerKnown=f.markerPresent=true;
    f.manager=1;f.world=2;f.controller=3;f.state=state;f.reference=reference;f.end=100;f.marker=marker;return f;
}
int main() {
    int errors=0;auto check=[&](bool ok,const char* label){std::cout<<(ok?"PASS: ":"FAIL: ")<<label<<'\n';errors+=!ok;};
    Tracker t;
    check(t.Observe(At(2,100),100).speech.empty()&&!t.Running(),"scripted rewind-paused choice is not active rewind");
    t.Observe(At(0,100),150);
    check(t.Observe(At(1,90),200).cue==Cue::None,"rewind well beyond warning range is quiet");
    check(t.Observe(At(1,64),250).cue==Cue::Approach,"approach warns inside five percent of the gauge");
    check(t.Observe(At(1,63),300).cue==Cue::None,"stationary/nearby samples do not repeat approach");
    auto moved=At(1,59,30);
    check(t.Observe(moved,350).cue==Cue::Crossed,"crossing uses starting marker even after native marker moves earlier");
    check(t.Observe(At(1,58,30),400).cue==Cue::None,"crossing is one-shot");
    const auto done=t.Observe(At(0,58.1f,30),450);
    check(done.speech==L"Rewind gauge 58 percent. Starting last-action mark reached or passed."&&!t.Running(),"release reports gauge and remembered crossing, not later forward ticking");
    check(t.Observe(At(0,59,30),500).speech.empty(),"completion is announced once");
    t.Reset();t.Observe(At(0,100),100);
    check(t.Observe(At(4,50,30),150).cue==Cue::Crossed,"fast jump over approach band still reports crossing from recent idle sample");
    check(t.Observe(At(2,50,30),200).speech.find(L"50 percent")!=std::wstring::npos,"quick rewind ending in paused state produces one readout");
    t.Reset();t.Observe(At(1,70),100);auto stop=t.Observe(At(0,59,30),150);
    check(stop.cue==Cue::Crossed&&!stop.speech.empty(),"crossing between active and stop samples is not discarded");
    t.Reset();t.Observe(At(1,80),100);
    check(t.Observe(At(0,80),150).speech.find(L"Still after")!=std::wstring::npos,"stopping before reaching marker explicitly says it remains ahead");
    t.Reset();t.Observe(At(1,70),100);
    check(t.Observe(At(1,60),150).cue==Cue::Crossed,"exactly reaching marker gives boundary cue");
    auto none=At(1,50);none.markerPresent=false;
    t.Reset();t.Observe(none,100);check(t.Describe().find(L"No last-action mark")!=std::wstring::npos,"absent native marker is not a fabricated zero-time marker");
    none.markerKnown=false;t.Reset();t.Observe(none,100);
    check(t.Describe().find(L"mark unavailable")!=std::wstring::npos,"unreadable marker is distinct from absent marker");
    auto bad=At(1,80);bad.end=0;t.Reset();
    check(!t.Observe(bad,100).speech.empty()&&t.Observe(bad,150).speech.empty(),"invalid denominator reports unavailable once, never zero percent");
    bad=At(1,80);bad.reference=std::numeric_limits<float>::quiet_NaN();t.Reset();
    check(!t.Observe(bad,100).speech.empty()&&!t.Running(),"nonfinite native data cannot make a cue or readout");
    auto hidden=At(1,50);hidden.visible=false;t.Reset();
    check(t.Observe(hidden,100).speech.empty()&&!t.Running(),"hidden/opening gauge does not narrate or complain");
    t.Reset();t.Observe(At(1,80),100);Frame blocked;
    check(t.Observe(blocked,150).speech.empty()&&!t.Running(),"menus/focus loss cancel silently without fake completion");
    t.Observe(At(1,80),200);auto changed=At(0,50);changed.world=9;
    check(t.Observe(changed,250).speech.empty()&&!t.Running(),"world change cannot report an old crossing or completion");
    t.Reset();t.Observe(At(1,80),100);
    check(t.Observe(At(0,50),1000).speech.empty(),"stale sample gap cancels rather than inventing missed events");
    t.Reset();t.Observe(At(1,80),100);
    check(t.Observe(At(3,85),150).speech.empty()&&t.Running(),"fast-forward transition defers completion");
    check(t.Observe(At(0,90),200).speech.find(L"90 percent")!=std::wstring::npos,"post-fast-forward summary uses recovered position");
    lis::prompts::History h;h.Observe({{"tutorial",L"key",L"Tutorial."}});h.Remember(done.speech);
    check(h.Repeat()==done.speech,"existing prompt repeat retains the completed rewind readout");
    return errors?1:0;
}
