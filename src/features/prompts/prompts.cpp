#include "features/prompts/prompts.h"
#include "features/prompts/policy.h"
#include "features/prompts/extraction.h"
#include "engine/gfx.h"
#include "engine/reflection.h"
#include "platform/log.h"
#include "platform/text.h"
#include "output/speech.h"
#include <Windows.h>
#include <mutex>

namespace lis::prompts {
namespace {
engine::Reflection reflection;
uintptr_t movie=0;
DWORD thread=0;
uint64_t nextDiscovery=0;
bool initialized=false,faulted=false,faultReported=false,repeat=false;
std::mutex mutex;
History history;
constexpr bool TracePrompts=false;
std::map<std::string,std::string> diagnostics;
constexpr const char* channels[]={"AnnouncementQuotes","AnnouncementTooltipsHigh",
    "AnnouncementTooltips","AnnouncementPrompt","AnnouncementNotification",
    "AnnouncementAnimatedImages"};

void Discover(uint64_t now) {
    nextDiscovery=now+1000;
    if(!engine::gfx::ObjectQueriesReady())engine::gfx::Discover();
    engine::Array a{};
    if(!reflection.registry.Registry(a))return;
    std::vector<uintptr_t> objects(a.count);
    if(!engine::Copy(objects.data(),a.data,objects.size()*4))return;
    movie=0;
    for(auto p:objects) {
        const auto cls=reflection.registry.ClassOf(p);
        if(cls=="GFxUIAnnouncement"&&engine::gfx::ValidMovie(p))movie=p;
        if(cls.find("Property")==std::string::npos)continue;
        const auto owner=reflection.registry.NameOf(engine::Read<uintptr_t>(p+0x28));
        if(owner=="GFxWidget"||owner=="GFxUIWidgets"||owner=="GFxUIWidgetPlayer")
            reflection.Observe(p,cls,reflection.registry.NameOf(p));
    }
}
void State(const std::string& channel,const std::string& state) {
    if constexpr(!TracePrompts)return;
    if(diagnostics[channel]!=state){diagnostics[channel]=state;Log("event=prompt_state channel="+channel+" "+state);}
}
Item Read(const char* channel,uintptr_t widgets) {
    Item item{channel,{},{}};
    int offset=0;
    if(!reflection.StructOffset("GFxUIWidgets",channel,offset)){State(channel,"status=missing_widget_metadata");return item;}
    const auto widget=widgets+offset;
    unsigned char opening=0;
    uintptr_t root=0;
    if(!reflection.Value(widget,"GFxWidget","OpeningState","ByteProperty",opening)||
       !reflection.Value(widget,"GFxWidget","Root","ObjectProperty",root)){State(channel,"status=missing_base_fields");return item;}
    const bool unhidden=reflection.ClearBool(widget,"GFxWidget","bWidgetDisabled")&&
        reflection.ClearBool(widget,"GFxWidget","bWidgetHiddenByMenu")&&
        reflection.ClearBool(widget,"GFxWidget","bWidgetHiddenByCinematic")&&
        reflection.ClearBool(widget,"GFxWidget","bWidgetHiddenByFSM");
    std::string state="opening="+std::to_string(opening)+" unhidden="+std::to_string(unhidden)+" root="+std::to_string(root!=0);
    if(!unhidden||!root){State(channel,state);return item;}
    const auto path=ClipPath(engine::gfx::ObjectString(root,"_target"));
    if(path.empty()){State(channel,state+" path=unavailable");return item;}
    uintptr_t owner=0;
    const bool owned=reflection.Value(widget,"GFxWidget","Owner","ObjectProperty",owner)&&engine::gfx::ValidMovie(owner);
    // Keep the working legacy receiver as a fallback if ownership cannot be
    // obtained. A new ownership path must not remove already-working output.
    if(!owned)owner=movie;
    auto string=[owner](const std::string& p){return engine::gfx::DisplayString(owner,p);};
    auto number=[owner](const std::string& p){return engine::gfx::Number(owner,p);};
    const auto caption=ReadComposition(path,string,number);
    item.text=caption.text; // Leaf-level adaptation has already preserved other captions.
    if constexpr(TracePrompts)State(channel,state+" owner_resolved="+std::to_string(owned)+" owner_matches_discovery="+std::to_string(owner==movie)+" script_opening="+std::to_string(number(path+".opening_state"))+" text="+std::to_string(!item.text.empty())+" key="+Utf8(caption.key));
    return item;
}
void Snapshot() {
    const auto now=GetTickCount64();
    if(!engine::gfx::ValidMovie(movie)) {
        if(now>=nextDiscovery)Discover(now);
    }
    std::vector<Item> frame;
    uintptr_t widgets=0;
    if(engine::gfx::ValidMovie(movie)&&engine::gfx::ObjectQueriesReady()&&
       reflection.Value(movie,"GFxUIWidgetPlayer","UIWidgets","ObjectProperty",widgets)&&
       reflection.registry.Instance(widgets)&&reflection.registry.ClassOf(widgets)=="GFxUIWidgets") {
        for(auto channel:channels){auto item=Read(channel,widgets);if(!item.text.empty())frame.push_back(std::move(item));}
    } else State("reader","status=waiting_for_announcement_movie");
    std::lock_guard<std::mutex> lock(mutex);history.Observe(frame);
}
bool SafeSnapshot(){__try{Snapshot();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
}
void Initialize(uintptr_t base){reflection.registry=engine::ObjectRegistry(base,engine::RegistryPolicy::World);initialized=true;}
void Tick() {
    if(!initialized||faulted)return;
    if(!thread)thread=GetCurrentThreadId();
    if(thread!=GetCurrentThreadId())return;
    bool ok=false;
    try{ok=SafeSnapshot();}catch(...){ok=false;}
    if(!ok){std::lock_guard<std::mutex> lock(mutex);faulted=true;Log("event=prompt_fault status=disabled");}
}
void RequestRepeat(){std::lock_guard<std::mutex> lock(mutex);repeat=true;}
void Pump(bool foreground,bool readableMenu) {
    std::wstring text;bool interrupt=false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(!foreground){history.Background();repeat=false;return;}
        if(faulted&&!faultReported){faultReported=true;text=L"Prompt reading failed and has been disabled. See the mod log.";interrupt=true;}
        else if(!readableMenu) {
            interrupt=repeat;
            text=repeat?history.Repeat():history.Take();
        }
        repeat=false;
    }
    // Automatic prompts queue after this iteration's menu/scanner output.
    // Explicit repeat interrupts; it never selects or dismisses a game prompt.
    if(!text.empty())Speak(text,interrupt);
}
}
