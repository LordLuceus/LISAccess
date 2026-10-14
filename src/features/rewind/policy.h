#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace lis::rewind {
struct Frame {
    bool context=false,valid=false,visible=false,markerKnown=false,markerPresent=false;
    uintptr_t manager=0,world=0,controller=0;
    unsigned state=255;
    float reference=0,end=0,marker=0;
    const char* reason="uninitialized";
};
inline bool Rewinding(const Frame& f){return f.state==1||f.state==4;}
inline bool SameWorld(const Frame& a,const Frame& b) {
    return a.manager==b.manager&&a.world==b.world&&a.controller==b.controller;
}
inline bool PositionValid(const Frame& f) {
    return f.valid&&std::isfinite(f.reference)&&f.reference>=0&&std::isfinite(f.end)&&f.end>0;
}
enum class Cue { None, Approach, Crossed };
struct Feedback {Cue cue=Cue::None;std::wstring speech;};
// The native marker can move to an earlier action after undo. Retain only its
// timestamp (never a native pointer) for this rewind, so it cannot escape us.
class Tracker {
    Frame previous{},last{},anchor{};
    uint64_t previousAt=0;
    bool running=false,warned=false,crossed=false,invalidReported=false,forwarded=false;
    float scale=0;
    Cue Check(float from,float to) {
        if(!anchor.markerKnown||!anchor.markerPresent)return Cue::None;
        if(!crossed&&from>anchor.marker&&to<=anchor.marker) {
            crossed=true;warned=true;return Cue::Crossed;
        }
        if(!warned&&!crossed&&to>anchor.marker&&to<=from&&
           (to-anchor.marker)/scale<=0.05f) {warned=true;return Cue::Approach;}
        return Cue::None;
    }
public:
    bool Running() const{return running;}
    void Reset(){*this=Tracker{};}
    std::wstring Describe() const {
        if(!running||!PositionValid(last))return L"Rewind position unavailable.";
        const int percent=static_cast<int>(std::lround(std::clamp(last.reference/scale,0.f,1.f)*100));
        std::wstring text=L"Rewind gauge "+std::to_wstring(percent)+L" percent. ";
        if(!anchor.markerKnown)return text+L"Last-action mark unavailable.";
        if(!anchor.markerPresent)return text+L"No last-action mark.";
        if(crossed)return text+L"Starting last-action mark reached or passed.";
        const auto gap=(last.reference-anchor.marker)/scale;
        if(std::abs(gap)<=0.00001f)return text+L"At the starting last-action mark.";
        return text+(gap<0?L"Before the starting last-action mark.":L"Still after the starting last-action mark.");
    }
    Feedback Observe(const Frame& f,uint64_t now) {
        Feedback out;
        if(!f.context){Reset();return out;}
        if(previousAt&&(now<previousAt||now-previousAt>500||!SameWorld(previous,f)))Reset();
        if(Rewinding(f)&&!f.visible){running=false;previousAt=0;return out;}
        if(!f.valid||!std::isfinite(f.reference)||f.reference<0||f.state>4||(Rewinding(f)&&!PositionValid(f))) {
            if(Rewinding(f)&&!invalidReported)out.speech=L"Rewind position unavailable.";
            running=false;previousAt=0;invalidReported=true;return out;
        }
        invalidReported=false;
        if(Rewinding(f)) {
            if(!running) {
                anchor=f;
                // A recent idle sample also catches a fast jump over the mark
                // before our first active sample. Never infer an unseen crossing.
                const bool idle=previousAt&&now-previousAt<=150&&SameWorld(previous,f)&&
                    (previous.state==0||previous.state==2)&&previous.reference>=f.reference;
                if(idle)anchor=previous;
                scale=f.end;last=f;running=true;warned=crossed=forwarded=false;
                if(anchor.markerPresent&&(!std::isfinite(anchor.marker)||anchor.marker<0||anchor.marker>scale))
                    anchor.markerKnown=anchor.markerPresent=false;
                out.cue=Check(idle?previous.reference:f.reference,f.reference);
            } else {
                scale=f.end;out.cue=Check(last.reference,f.reference);last=f;
            }
        } else if(running&&(f.state==0||f.state==2)) {
            // Use the last rewind position, or a lower stop sample. Forward
            // ticking after release must not claim the undone action was kept.
            const float stop=forwarded?f.reference:(std::min)(last.reference,f.reference);
            out.cue=Check(last.reference,stop);last.reference=stop;last.end=scale;
            out.speech=Describe();running=false;
        }
        if(running&&f.state==3)forwarded=true;
        // State3 is native fast-forward, not a rewind completion. Wait for a
        // stable playing/paused state; don't turn scripted state2 into a start.
        previous=f;previousAt=now;
        return out;
    }
};
}
