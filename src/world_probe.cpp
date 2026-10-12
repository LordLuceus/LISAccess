#include "world_probe.h"
#include "platform.h"
#include "engine_abi.h"
#include "interaction_fields.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <unordered_map>

namespace lis::world {
namespace {
constexpr uintptr_t Names = 0x11e43b4, Objects = 0x11d0598, Dispatch = 0x2a1b00;
constexpr float Pi = 3.14159265358979323846f;
struct Array { uintptr_t data; int count, capacity; };
static_assert(sizeof(uintptr_t) == 4, "Verified x86 build only");
uintptr_t base = 0, trace = 0;
uintptr_t localize = 0, liveController = 0, liveWorld = 0, liveFocus = 0;
uintptr_t faceFunction = 0;
std::unordered_map<std::wstring,std::wstring> translations;
DWORD thread = 0;
bool faulted = false, ready = false;
bool interactionFaulted = false;
bool faceFaulted = false;
int cursor = 0;
std::unordered_map<std::string, uintptr_t> fields;
std::vector<uintptr_t> controllers, menus, destinations;
std::vector<Landmark> landmarks;
std::string lastState;
bool Copy(void* dst, uintptr_t src, size_t n) {
    if (src < 0x10000 || n > 1024 * 1024) return false;
    __try { std::memcpy(dst, reinterpret_cast<void*>(src), n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> T Read(uintptr_t p) { T v{}; Copy(&v, p, sizeof(v)); return v; }
bool Registry(Array& a) {
    return Copy(&a, base + Objects, sizeof(a)) && a.data >= 0x10000 &&
        a.count > 100 && a.count <= 250000 && a.capacity >= a.count;
}
bool Registered(uintptr_t p) {
    Array a{}; int i = -1;
    return p && Registry(a) && Copy(&i, p + 0x20, 4) && i >= 0 && i < a.count && Read<uintptr_t>(a.data + i * 4) == p;
}
std::string Name(int i) {
    Array a = Read<Array>(base + Names);
    if (i < 0 || i >= a.count || a.count > 1000000 || a.capacity < a.count) return {};
    char text[192]{};
    if (!Copy(text, Read<uintptr_t>(a.data + i * 4) + 8, sizeof(text) - 1)) return {};
    return text;
}
std::string NameOf(uintptr_t p) { return Name(Read<int>(p + 0x2c)); }
std::string ClassOf(uintptr_t p) { return NameOf(Read<uintptr_t>(p + 0x34)); }
bool Instance(uintptr_t p) {
    uint32_t flags = 0;
    return Registered(p) && Copy(&flags, p + 8, 4) && !(flags & (0x200 | 0x400)) && NameOf(p).find("Default__") != 0;
}
bool IsA(uintptr_t p, const char* type) {
    uintptr_t c = Read<uintptr_t>(p + 0x34);
    for (int n = 0; c && n < 64; ++n, c = Read<uintptr_t>(c + 0x48))
        if (Registered(c) && NameOf(c) == type) return true;
    return false;
}
std::string Path(uintptr_t p) {
    std::string result;
    for (int n = 0; p && n < 16; ++n, p = Read<uintptr_t>(p + 0x28)) {
        if (!Registered(p)) return {};
        std::string part = NameOf(p);
        int number = Read<int>(p + 0x30);
        if (number > 0) part += "_" + std::to_string(number - 1);
        result = part + (result.empty() ? "" : "." + result);
    }
    return result;
}
bool EqualPath(const std::string& a, const std::string& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](unsigned char x,unsigned char y){return std::tolower(x)==std::tolower(y);});
}
uintptr_t Field(const char* owner, const char* name, const char* type, int size) {
    auto it = fields.find(std::string(owner) + "." + name);
    if (it == fields.end()) return 0;
    uintptr_t p = it->second;
    if (!Registered(p) || ClassOf(p) != type || NameOf(p)!=name || NameOf(Read<uintptr_t>(p+0x28))!=owner || Read<int>(p + 0x40) != 1 || Read<int>(p + 0x44) != size) return 0;
    int offset = Read<int>(p + 0x60);
    return offset >= 0 && offset < 0x10000 ? p : 0;
}
bool Offset(const char* owner, const char* name, const char* type, int size, int& offset) {
    uintptr_t f = Field(owner, name, type, size);
    if (!f) return false;
    offset = Read<int>(f + 0x60); return true;
}
template<class T> bool Value(uintptr_t object, const char* owner, const char* name, const char* type, T& value) {
    int offset = 0;
    return Offset(owner, name, type, sizeof(T), offset) && Copy(&value, object + offset, sizeof(T));
}
struct PropertyReader {
    template<class T> bool operator()(uintptr_t object,const char* owner,const char* name,const char* type,T& value) const {
        return Value(object,owner,name,type,value);
    }
};
std::wstring WideName(uint32_t index) {
    const std::string name=Name(static_cast<int>(index));
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),static_cast<int>(name.size()),nullptr,0);
    if(count<=0)return {};
    std::wstring result(static_cast<size_t>(count),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name.data(),static_cast<int>(name.size()),result.data(),count);
    return result;
}
bool ClearBool(uintptr_t object, const char* owner, const char* name) {
    uintptr_t f = Field(owner, name, "BoolProperty", 4);
    uint32_t value = 0, mask = Read<uint32_t>(f + 0x80);
    return f && mask && Copy(&value, object + Read<int>(f + 0x60), 4) && !(value & mask);
}
bool BoolIsSet(uintptr_t object,const char* owner,const char* name) {
    uintptr_t f=Field(owner,name,"BoolProperty",4);
    uint32_t value=0,mask=Read<uint32_t>(f+0x80);
    return f&&mask&&Copy(&value,object+Read<int>(f+0x60),4)&&(value&mask);
}
void Add(std::vector<uintptr_t>& v, uintptr_t p) {
    if (v.size() < 256 && std::find(v.begin(), v.end(), p) == v.end()) v.push_back(p);
}
bool Discover() {
    Array a{};
    if (!Registry(a) || Name(0) != "None" || Name(2) != "IntProperty") return false;
    // Hard per-tick discovery bound. Initial sampling waits for a complete registry pass.
    int end = (std::min)(a.count, cursor + 8192);
    for (; cursor < end; ++cursor) {
        uintptr_t p = Read<uintptr_t>(a.data + cursor * 4);
        if (!Registered(p)) continue;
        std::string cls = ClassOf(p), name = NameOf(p);
        if (cls.find("Property") != std::string::npos && fields.size() < 50000)
            fields[NameOf(Read<uintptr_t>(p + 0x28)) + "." + name] = p;
        if (cls == "Function" && name == "Trace" && NameOf(Read<uintptr_t>(p + 0x28)) == "Actor") trace = p;
        if (cls == "Function" && name == "Localize" && NameOf(Read<uintptr_t>(p + 0x28)) == "Object") localize = p;
        if (cls == "Function" && name == "SetNextCameraRotationForced" && NameOf(Read<uintptr_t>(p + 0x28)) == "CBA_3RD_Basic") faceFunction = p;
        if (!Instance(p)) continue;
        if (cls == "WhatIfController") Add(controllers, p);
        if (cls == "PauseMenu" || cls == "OutGameSensenMenu") Add(menus, p);
        std::string shortName=name;
        const int number=Read<int>(p+0x30);
        if(number>0) shortName+="_"+std::to_string(number-1);
        for(const auto& landmark:landmarks) {
            const auto last=landmark.actorPath.find_last_of('.');
            const auto targetName=last==std::string::npos ? landmark.actorPath : landmark.actorPath.substr(last+1);
            if(EqualPath(shortName,targetName) && IsA(p,"Actor") && EqualPath(Path(p),landmark.actorPath)) Add(destinations,p);
        }
    }
    if (cursor == a.count) { cursor = 0; ready = true; }
    return ready;
}
bool Finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::fabs(v.x) < 1e7f && std::fabs(v.y) < 1e7f && std::fabs(v.z) < 1e7f; }
float Length(Vec3 v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
struct TraceParams {
    Vec3 location, normal, end, start;
    uint32_t actors = 1;
    Vec3 extent;
    unsigned char hitInfo[28]{};
    int flags = 8;
    uintptr_t actor = 0;
};
static_assert(sizeof(TraceParams) == 100);
bool TraceVerified() {
    return Registered(trace) && ClassOf(trace) == "Function" && NameOf(trace) == "Trace" &&
        NameOf(Read<uintptr_t>(trace + 0x28)) == "Actor" && Read<uint16_t>(trace + 0xa2) == 100 &&
        Read<uint16_t>(trace + 0xa4) == 96 && (Read<uint32_t>(trace + 0x90) & 0x400) && Read<uint16_t>(trace + 0x94) == 277;
}
// ONLY Actor.Trace(277): stock UE3 indexed-native SDK dispatch adapter.
// This transient write is dispatch metadata, not world/save state. Restore even on SEH.
// No other native index or function flag is ever changed.
void DispatchTrace(uintptr_t pawn, TraceParams* p) {
    volatile uint16_t* index = reinterpret_cast<volatile uint16_t*>(trace + 0x94);
    __try {
        *index = 0;
        // AActor's override (vtable72, RVA2a1b00) checks world readiness and calls
        // UObject::ProcessEvent. Both end in ret 0x0c, including the legacy result slot.
        reinterpret_cast<engine::ProcessEvent>(base + Dispatch)(reinterpret_cast<void*>(pawn), reinterpret_cast<void*>(trace), p, nullptr);
    } __finally { *index = 277; }
}
bool Query(uintptr_t pawn, Vec3 start, Vec3 end, Vec3 extent, TraceParams& p) {
    if (!TraceVerified() || !Instance(pawn) ||
        Read<uintptr_t>(Read<uintptr_t>(pawn) + 72 * 4) != base + Dispatch ||
        !Finite(start) || !Finite(end) || !Finite(extent)) return false;
    p = {}; p.start = start; p.end = end; p.extent = extent; p.actors = 1; p.flags = 8;
    DispatchTrace(pawn, &p);
    if (!p.actor) return true; // meaningful only after this sample's positive floor calibration
    float n = Length(p.normal);
    return Registered(p.actor) && p.actor != pawn &&
        Read<uintptr_t>(p.actor + 0xd8) == Read<uintptr_t>(pawn + 0xd8) &&
        Finite(p.location) && Finite(p.normal) && n >= 0.8f && n <= 1.2f &&
        Length(Sub(p.location, start)) <= Length(Sub(end, start)) + Length(extent) + 4;
}
struct String {uintptr_t data=0;int count=0,capacity=0;};
std::wstring Text(uintptr_t object,const char* owner,const char* key) {
    String value{};
    if(!Value(object,owner,key,"StrProperty",value) || !value.data || value.count<1 || value.count>8192 || value.capacity<value.count) return {};
    std::wstring out(static_cast<size_t>(value.count-1),L'\0');
    return Copy(out.data(),value.data,out.size()*sizeof(wchar_t)) ? out : std::wstring{};
}
std::wstring Translate(const std::wstring& key) {
    if(key.rfind(L"$GT_",0)!=0)return key;
    auto cached=translations.find(key);if(cached!=translations.end())return cached->second;
    const auto a=key.find(L'_',4),b=a==std::wstring::npos?a:key.find(L'_',a+1);
    if(b==std::wstring::npos || !Registered(localize) || Read<uint16_t>(localize+0xa2)!=48 || Read<uint16_t>(localize+0xa4)!=36 || Read<uint16_t>(localize+0x94)!=0) return L"Unnamed object";
    std::wstring package=key.substr(4,a-4),section=key.substr(a+1,b-a-1),item=key.substr(b+1);
    auto borrow=[](std::wstring& text){return String{reinterpret_cast<uintptr_t>(text.data()),static_cast<int>(text.size()+1),static_cast<int>(text.size()+1)};};
    struct Params {String section,key,package,result;} p{borrow(section),borrow(item),borrow(package),{}};
    static_assert(sizeof(Params)==48);
    reinterpret_cast<engine::ProcessEvent>(base+Dispatch)(reinterpret_cast<void*>(liveController),reinterpret_cast<void*>(localize),&p,nullptr);
    std::wstring result;
    if(p.result.data && p.result.count>0 && p.result.count<=8192 && p.result.capacity>=p.result.count) {
        result.resize(p.result.count-1);
        if(!Copy(result.data(),p.result.data,result.size()*sizeof(wchar_t)))result.clear();
    }
    if(p.result.data)reinterpret_cast<void(__cdecl*)(void*)>(base+0x259e0)(reinterpret_cast<void*>(p.result.data));
    if(result.empty() || result.rfind(L"$GT_",0)==0)return L"Unnamed object";
    if(translations.size()<2048)translations.emplace(key,result);
    return result;
}
bool ValidArray(const Array& a,int maximum) {return a.count>=0&&a.count<=maximum&&a.capacity>=a.count&&(!a.count||a.data>=0x10000);}
uintptr_t InteractionWidget(uintptr_t actor) {
    Array components{};
    if(!Value(actor,"Actor","AllComponents","ArrayProperty",components)||!ValidArray(components,128))return 0;
    uintptr_t result=0;
    for(int i=0;i<components.count;++i) {
        uintptr_t component=Read<uintptr_t>(components.data+i*4);Array spawned{};
        if(!Instance(component)||!IsA(component,"SensenARComponent")||Read<uintptr_t>(component+0x28)!=actor ||
            !Value(component,"SensenARComponent","SpawnedActors","ArrayProperty",spawned)||!ValidArray(spawned,32))continue;
        for(int j=0;j<spawned.count;++j) {
            uintptr_t ar=Read<uintptr_t>(spawned.data+j*4),widget=0;
            if(!Instance(ar)||!IsA(ar,"SensenARActor")||Read<uintptr_t>(ar+0xd8)!=liveWorld ||
                !Value(ar,"SensenARActor","ARObject","ObjectProperty",widget)||!Instance(widget)||!IsA(widget,"WhatIfInteractionWidget")||Read<uintptr_t>(widget+0x28)!=ar)continue;
            if(result && result!=widget)return 0;
            result=widget;
        }
    }
    return result;
}
bool Binding(unsigned slot,engine::Name& out) {
    static const char* names[]={"Choice_Y","Choice_X","Choice_B","Choice_A"};
    // Four entries only: the adjacent fifth FName is unrelated (DeleteSaveGame).
    return slot<4&&Copy(&out,base+0x1236d20+slot*sizeof(out),sizeof(out))&&out.number==0&&Name(out.index)==names[slot];
}
int EligibleState(uintptr_t actor) {
    Array actions{};int stride=0;
    if(!Value(actor,"WhatIfInteractionActor","Actions","ArrayProperty",actions)||!ValidArray(actions,64))return false;
    uintptr_t property=Field("WhatIfInteractionActor","Actions","ArrayProperty",12),inner=Read<uintptr_t>(property+0x80);
    if(!Registered(inner)||ClassOf(inner)!="StructProperty"||!Copy(&stride,inner+0x44,4)||stride!=0x138)return false;
    int best=0;
    for(int i=0;i<actions.count;++i) {
        const int state=reinterpret_cast<engine::ActionEligibility>(base+0x9f6a40)(reinterpret_cast<void*>(actor),reinterpret_cast<void*>(actions.data+i*stride));
        if(state==2)return 2;
        if(state==1)best=1;
    }
    return best;
}
void Interactions(Snapshot& s) {
    uintptr_t input=0,component=0,current=0;Array actors{};float radius=0;
    if(Value(liveController,"WhatIfController","ExInput","ObjectProperty",input)&&Instance(input)) {
        s.gamepad=BoolIsSet(input,"WhatIfInput","bPlayingWithGamepad");
        Value(input,"WhatIfInput","ActiveControllerId","IntProperty",s.controllerIndex);
    }
    s.interactionStatus="component_unavailable";
    if(!interaction::ReadComponent(PropertyReader{},liveController,component)||!Instance(component)||!IsA(component,"DNEWhatIfInteractionComponent"))return;
    s.interactionStatus="state_unavailable";
    if(!Value(component,"DNEWhatIfInteractionComponent","RegisteredActors","ArrayProperty",actors)||!ValidArray(actors,2048) ||
        !Value(component,"DNEWhatIfInteractionComponent","CurrentInteraction","ObjectProperty",current) ||
        !Value(component,"DNEWhatIfInteractionComponent","ActorInteractionFocused","ObjectProperty",liveFocus) ||
        !Value(component,"DNEWhatIfInteractionComponent","FarZoneRadius","FloatProperty",radius)||!std::isfinite(radius)||radius<=0||radius>10000)return;
    s.interactionBusy=current!=0;
    if(s.interactionBusy){s.interactionStatus="busy";liveFocus=0;return;}
    s.interactionStatus="schema_unavailable";
    const uintptr_t actionsProperty=Field("WhatIfInteractionActor","Actions","ArrayProperty",12);
    const uintptr_t inner=Read<uintptr_t>(actionsProperty+0x80);
    if(!Field("WhatIfInteractionActor","ObjectName","NameProperty",8)||!actionsProperty ||
        !Registered(inner)||ClassOf(inner)!="StructProperty"||Read<int>(inner+0x44)!=0x138)return;
    s.interactionStatus="ready";
    std::vector<std::pair<float,uintptr_t>> nearby;
    for(int i=0;i<actors.count;++i) {
        uintptr_t actor=Read<uintptr_t>(actors.data+i*4);Vec3 position{};
        if(!Instance(actor)||!IsA(actor,"WhatIfInteractionActor")||Read<uintptr_t>(actor+0xd8)!=liveWorld ||
            !ClearBool(actor,"Actor","bDeleteMe")||!BoolIsSet(actor,"WhatIfInteractionActor","bIsInteractionActive") ||
            !Copy(&position,actor+0x54,sizeof(position))||!Finite(position))continue;
        const float distance=Length(Sub(position,s.player));
        if(distance<=radius||actor==liveFocus)nearby.emplace_back(distance,actor);
    }
    std::sort(nearby.begin(),nearby.end());
    // Never lose the real focus behind nearer, condition-ineligible objects.
    auto focused=std::find_if(nearby.begin(),nearby.end(),[](const auto& item){return item.second==liveFocus;});
    if(focused!=nearby.end())std::rotate(nearby.begin(),focused,focused+1);
    unsigned eligibleCount=0;
    for(const auto& candidate:nearby) {
        if(eligibleCount>=32)break;
        const uintptr_t actor=candidate.second;
        const int eligibility=EligibleState(actor);
        if(!eligibility)continue;
        const std::wstring raw=interaction::ReadObjectLabel(PropertyReader{},WideName,actor);
        if(raw.empty())continue;
        Target target;target.stableId=Path(actor);target.label=Translate(raw);target.angleRestricted=eligibility==1;Copy(&target.position,actor+0x54,sizeof(target.position));
        if(target.stableId.empty())continue;
        ++eligibleCount;
        uintptr_t object=0;
        // This game also represents NPCs as SkeletalMeshActorMAT, not Pawns.
        // Its authored Chara localization namespace identifies those names in
        // every language, without guessing from the displayed English text.
        if(raw.rfind(L"$GT_Sensen_Chara_",0)==0 ||
            (Value(actor,"WhatIfInteractionActor","ObjectActor","ObjectProperty",object)&&Instance(object)&&IsA(object,"Pawn")))target.kind=Kind::Person;
        if(std::none_of(s.targets.begin(),s.targets.end(),[&](const Target& other){return other.stableId==target.stableId;}))s.targets.push_back(target);
        if(actor!=liveFocus)continue;
        uintptr_t widget=InteractionWidget(actor);
        if(!widget||!BoolIsSet(actor,"WhatIfInteractionActor","bIsInteractionUIOpened"))continue;
        s.focusedId=target.stableId;s.focusedLabel=target.label;
        for(unsigned slot=0;slot<4;++slot) {
            engine::Name binding{};
            const auto number=std::to_string(slot+1);
            if(!Binding(slot,binding)||!BoolIsSet(widget,"WhatIfInteractionWidget",("button"+number+"_visible").c_str()))continue;
            const auto label=Text(widget,"WhatIfInteractionWidget",("action_name"+number).c_str());
            if(!label.empty())s.actions[slot].label=Translate(label);
        }
    }
}
bool SafeInteractions(Snapshot& s) {
    __try {Interactions(s);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){interactionFaulted=true;return false;}
}
bool SampleImpl(Snapshot& s) {
    liveController=liveWorld=liveFocus=0;
    if (!Discover()) { s.reason = "discovering bounded world metadata"; return false; }
    for (uintptr_t m : menus) if (Instance(m) && (ClassOf(m)=="PauseMenu" || ClassOf(m)=="OutGameSensenMenu")) {
        unsigned char opening = 0;
        if (!Value(m, "SensenMenuBase", "OpeningState", "ByteProperty", opening) || opening != 0) {
            s.reason = "pause or out-game menu"; return false;
        }
    }
    uintptr_t controller = 0, pawn = 0, world = 0;
    for (uintptr_t c : controllers) {
        uintptr_t p = 0, ex = 0, back = 0, w = 0;
        if (!Instance(c) || !IsA(c,"WhatIfController") || !Value(c, "Controller", "Pawn", "ObjectProperty", p) || !Instance(p) || !IsA(p,"Pawn") ||
            !Value(c, "WhatIfController", "ExPawn", "ObjectProperty", ex) || ex != p ||
            !Value(p, "Pawn", "Controller", "ObjectProperty", back) || back != c ||
            !Copy(&w, p + 0xd8, 4) || !Registered(w) || Read<uintptr_t>(c + 0xd8) != w) continue;
        if (controller) { s.reason = "ambiguous local controller"; return false; }
        controller = c; pawn = p; world = w;
    }
    if (!controller) { s.reason = "no verified live player"; return false; }
    liveController=controller;liveWorld=world;
    uintptr_t hud=0,debugMenu=0;
    if(!Value(controller,"PlayerController","myHUD","ObjectProperty",hud)||!Instance(hud)||!IsA(hud,"ExampleHUD") ||
        !Value(hud,"ExampleHUD","myDebugMenu","ObjectProperty",debugMenu)) {
        s.reason="HUD input context unavailable";return false;
    }
    // D-pad commands also belong to the native debug menu. Never run scanner
    // controls alongside it, even if ordinary traversal flags remain enabled.
    if(debugMenu&&(!Instance(debugMenu)||!IsA(debugMenu,"DebugMenu") ||
        !ClearBool(debugMenu,"DebugMenu","bActive")||!ClearBool(debugMenu,"DebugMenu","bShow"))) {
        s.reason="native debug menu active";return false;
    }
    unsigned char gameplay = 0, move = 1, look = 1;
    uintptr_t pauser = 0;
    if (!Value(controller, "WhatIfController", "CurrentGameplayType", "ByteProperty", gameplay) || gameplay != 1 ||
        !Value(world, "WorldInfo", "Pauser", "ObjectProperty", pauser) || pauser ||
        !ClearBool(controller, "WhatIfController", "bGameplayPause") ||
        !ClearBool(controller, "WhatIfController", "PlayerControlDisabled") ||
        !ClearBool(controller, "PlayerController", "bCinematicMode") ||
        !Value(controller, "PlayerController", "bIgnoreMoveInput", "ByteProperty", move) || move ||
        !Value(controller, "PlayerController", "bIgnoreLookInput", "ByteProperty", look) || look) {
        s.reason = "traversal controls unavailable"; return false;
    }
    uintptr_t cylinder = 0, camera = 0;
    int cache = 0, pov = 0, rotation = 0;
    // Struct sizes are validated by metadata rather than assuming stock SDK packing.
    auto structOffset = [](const char* owner, const char* name, int& out) {
        auto it = fields.find(std::string(owner) + "." + name);
        if (it == fields.end()) return false;
        int size = Read<int>(it->second + 0x44);
        return size >= 12 && size < 4096 && Offset(owner, name, "StructProperty", size, out);
    };
    int yaw = 0;
    if (!Copy(&s.player, pawn + 0x54, sizeof(Vec3)) || !Finite(s.player) ||
        !Value(pawn, "Actor", "CollisionComponent", "ComponentProperty", cylinder) || !Registered(cylinder) || !IsA(cylinder, "CylinderComponent") ||
        !Value(cylinder, "CylinderComponent", "CollisionRadius", "FloatProperty", s.radius) ||
        !Value(cylinder, "CylinderComponent", "CollisionHeight", "FloatProperty", s.halfHeight) ||
        !std::isfinite(s.radius) || !std::isfinite(s.halfHeight) || s.radius < 1 || s.radius > 256 || s.halfHeight < s.radius || s.halfHeight > 512 ||
        !Value(controller, "WhatIfController", "ExCamera", "ObjectProperty", camera) || !Registered(camera) || !IsA(camera, "Camera") ||
        !structOffset("Camera", "CameraCache", cache) || !structOffset("TCameraCache", "POV", pov) ||
        !Offset("TPOV", "Rotation", "StructProperty", 12, rotation) ||
        !Copy(&yaw, camera + cache + pov + rotation + 4, 4)) {
        s.reason = "invalid player cylinder or camera metadata"; return false;
    }
    s.cameraYaw = static_cast<float>(yaw % 65536) * (2 * Pi / 65536);
    TraceParams floor{};
    Vec3 floorEnd = s.player; floorEnd.z -= s.halfHeight * 3;
    if (!Query(pawn, s.player, floorEnd, {}, floor) || !floor.actor || floor.normal.z < 0.65f ||
        floor.location.z > s.player.z || s.player.z - floor.location.z > s.halfHeight * 1.8f) {
        s.reason = "floor calibration unknown"; return false;
    }
    const float range = s.radius * 6;
    const Vec3 extent{s.radius * 0.8f, s.radius * 0.8f, s.halfHeight * 0.4f};
    unsigned queries = 1;
    for (size_t i = 0; i < s.probes.size(); ++i) {
        Probe& probe = s.probes[i]; probe.angle = (static_cast<float>(i) - 1) * 35 * Pi / 180;
        float angle = s.cameraYaw + probe.angle;
        Vec3 end{s.player.x + std::cos(angle)*range, s.player.y + std::sin(angle)*range, s.player.z};
        TraceParams hit{}; ++queries;
        if (!Query(pawn, s.player, end, extent, hit)) continue;
        // A floor contact terminates a sweep, so space behind it is UNKNOWN, not clear.
        if (hit.actor && hit.normal.z >= 0.65f) continue;
        probe.known = true; probe.hit = hit.actor != 0;
        probe.distance = probe.hit ? (std::min)(range, Length(Sub(hit.location, s.player))) : range;
        probe.normal = hit.normal;
    }
    Vec3 aheadStart{s.player.x + std::cos(s.cameraYaw)*s.radius*3, s.player.y + std::sin(s.cameraYaw)*s.radius*3, s.player.z};
    Vec3 down = aheadStart; down.z -= s.halfHeight * 4;
    TraceParams forwardFloor{}; ++queries;
    if (Query(pawn, aheadStart, down, {}, forwardFloor)) {
        if (!forwardFloor.actor) { s.dropKnown = true; s.dropAhead = true; }
        else if (forwardFloor.normal.z >= 0.65f && forwardFloor.location.z <= aheadStart.z) {
            s.dropKnown = true;
            s.dropAhead = floor.location.z - forwardFloor.location.z > s.halfHeight * 0.6f;
        }
    }
    for (uintptr_t actor : destinations) {
        if (!Instance(actor) || !IsA(actor,"Actor") || Read<uintptr_t>(actor + 0xd8) != world) continue;
        std::string path = Path(actor);
        for (const Landmark& landmark : landmarks) if (EqualPath(landmark.actorPath,path) && !landmark.label.empty()) {
            Vec3 position{};
            if (Copy(&position, actor + 0x54, sizeof(position)) && Finite(position))
                s.targets.push_back({path, landmark.label, position, Kind::Destination});
        }
    }
    const size_t authored=s.targets.size();
    if(!interactionFaulted&&!SafeInteractions(s)) {
        s.targets.resize(authored);s.focusedId.clear();s.focusedLabel.clear();s.actions={};
        Log("event=interaction_fault status=disabled navigation=preserved");
    }
    s.interactionsFailed=interactionFaulted;
    if(interactionFaulted)s.interactionStatus="fault_disabled";
    static std::string lastInteractionState;
    const std::string state=s.interactionStatus+" targets="+std::to_string(s.targets.size()-authored)+" focused="+std::to_string(!s.focusedId.empty());
    if(state!=lastInteractionState){Log("event=interaction_scan status="+state);lastInteractionState=state;}
    s.active = true;
    s.reason = "calibrated local geometry";
    s.diagnostics = "queries=" + std::to_string(queries) + "; floor=positive; units=engine; partial body sweeps, not a clear route; drop=no support within bounded ray; interactions=native registered near-zone eligible actors";
    return true;
}
bool SafeSample(Snapshot& s) {
    __try { return SampleImpl(s); }
    __except (EXCEPTION_EXECUTE_HANDLER) { faulted = true; return false; }
}
bool SendAction(uintptr_t actor,engine::Name binding) {
    __try {
        const uintptr_t listener=actor+0x26c;
        const uintptr_t method=Read<uintptr_t>(Read<uintptr_t>(listener)+8);
        if(method!=base+0x9918b0)return false;
        // That concrete consumer does not read its third stack word. It resolves
        // the real BindingActionMap and calls the native guarded request path.
        reinterpret_cast<engine::InteractionPress>(method)(reinterpret_cast<void*>(listener),binding,nullptr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){interactionFaulted=true;return false;}
}
struct Rotator {int pitch=0,yaw=0,roll=0;};
int SignedRotation(int angle) {return static_cast<int16_t>(static_cast<uint32_t>(angle)&65535u);}
bool RequestCameraRotation(uintptr_t behaviour,Rotator rotation) {
    __try {
        if(!Registered(faceFunction)||Read<uint16_t>(faceFunction+0xa2)!=16||Read<uint16_t>(faceFunction+0x94)!=0 ||
            Read<uintptr_t>(Read<uintptr_t>(behaviour)+72*4)!=base+0x7b4b0)return false;
        struct Params {uint32_t activate;Rotator rotation;} p{1,rotation};
        static_assert(sizeof(Params)==16);
        reinterpret_cast<engine::ProcessEvent>(base+0x7b4b0)(reinterpret_cast<void*>(behaviour),reinterpret_cast<void*>(faceFunction),&p,nullptr);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){faceFaulted=true;return false;}
}
}
std::wstring FaceTarget(const std::string& expectedTarget,bool& submitted) {
    // EXPERIMENTAL (2026-10-12): explicit normal-camera request only. Native
    // collision/offsets may prevent focus; submission is not proof of arrival.
    submitted=false;
    if(thread!=GetCurrentThreadId()||!IsGameForeground()||faulted||faceFaulted)return L"Facing unavailable here.";
    Snapshot fresh;
    if(!SafeSample(fresh)||fresh.interactionBusy)return L"Facing unavailable while the camera controls are locked.";
    auto target=std::find_if(fresh.targets.begin(),fresh.targets.end(),[&](const Target& t){return t.stableId==expectedTarget;});
    if(target==fresh.targets.end())return L"That target is no longer available.";
    if(target->stableId==fresh.focusedId)return L"Already focused on "+target->label+L".";
    uintptr_t camera=0,behaviour=0;Array behaviours{};
    if(!Value(liveController,"WhatIfController","ExCamera","ObjectProperty",camera)||!Instance(camera) ||
        !Value(camera,"WhatIfCamera","GameplayCameraBehaviours","ArrayProperty",behaviours)||!ValidArray(behaviours,32))return L"Camera mode unavailable.";
    for(int i=0;i<behaviours.count;++i) {
        uintptr_t candidate=Read<uintptr_t>(behaviours.data+i*4);
        if(!Instance(candidate)||!BoolIsSet(candidate,"CameraBehaviour","bActive"))continue;
        // Do not take over fixed, dialogue, derived or blended camera behaviours.
        if(behaviour||ClassOf(candidate)!="CBA_3RD_Basic")return L"Facing is unavailable in this camera mode.";
        behaviour=candidate;
    }
    if(!behaviour||BoolIsSet(behaviour,"CBA_3RD_Basic","bWantNextCameraRotationForced"))return L"Wait for the camera to finish moving.";
    uintptr_t settings=0;Rotator down{},up{},gameRotation{},viewRotation{};Vec3 view{};
    auto locationOf=[](const char* owner,const char* name,int& offset) {
        auto it=fields.find(std::string(owner)+"."+name);
        if(it==fields.end())return false;
        int size=Read<int>(it->second+0x44);
        return size>=12&&size<4096&&Offset(owner,name,"StructProperty",size,offset);
    };
    int low=0,high=0,boundary=0,cache=0,pov=0,position=0,rotation=0;
    if(!Value(behaviour,"CBA_3RD_Basic","CurPOVSettings","ObjectProperty",settings)||!Instance(settings) ||
        !locationOf("CBPOVSettings","SettingsLookDown",low)||!locationOf("CBPOVSettings","SettingsLookUp",high) ||
        !Offset("SCameraPOVSettings","PitchBoundary","StructProperty",12,boundary) ||
        !Copy(&down,settings+low+boundary,12)||!Copy(&up,settings+high+boundary,12) ||
        !locationOf("Camera","CameraCache",cache)||!locationOf("TCameraCache","POV",pov) ||
        !Offset("TPOV","Location","StructProperty",12,position)||!Offset("TPOV","Rotation","StructProperty",12,rotation) ||
        !Copy(&view,camera+cache+pov+position,12)||!Finite(view)||!Copy(&viewRotation,camera+cache+pov+rotation,12) ||
        !Value(camera,"WhatIfCamera","GPCamRotation","StructProperty",gameRotation))return L"Camera limits could not be read.";
    int minimum=SignedRotation(down.pitch),maximum=SignedRotation(up.pitch);
    if(BoolIsSet(behaviour,"CBA_3RD_Basic","bOverrideNextPitch")) {
        if(!Value(behaviour,"CBA_3RD_Basic","NextPitchMin","IntProperty",minimum)||!Value(behaviour,"CBA_3RD_Basic","NextPitchMax","IntProperty",maximum))return L"Camera limits could not be read.";
        minimum=SignedRotation(minimum);maximum=SignedRotation(maximum);
    }
    if(minimum>maximum||minimum< -16384||maximum>16384)return L"Unsupported camera limits.";
    Vec3 delta=Sub(target->position,view);
    if(Length(delta)<1)return L"The target is too close to aim at.";
    const float units=65536/(2*Pi);
    const int yaw=static_cast<int>(std::lround(std::atan2(delta.y,delta.x)*units));
    const int pitch=static_cast<int>(std::lround(std::atan2(delta.z,std::hypot(delta.x,delta.y))*units));
    Rotator wanted=gameRotation;
    wanted.yaw=SignedRotation(SignedRotation(gameRotation.yaw)+SignedRotation(yaw-SignedRotation(viewRotation.yaw)));
    if(target->kind!=Kind::Destination)
        wanted.pitch=std::clamp(SignedRotation(gameRotation.pitch)+SignedRotation(pitch-SignedRotation(viewRotation.pitch)),minimum,maximum);
    if(!RequestCameraRotation(behaviour,wanted))return L"The camera request failed.";
    submitted=true;
    return L"Turning toward "+target->label+L".";
}
std::wstring ActivateFocused(const std::string& expectedFocus,unsigned slot) {
    if(thread!=GetCurrentThreadId()||!IsGameForeground()||faulted||interactionFaulted||slot>=4)return L"Interaction unavailable.";
    Snapshot fresh;
    if(!SafeSample(fresh)||fresh.focusedId.empty()||fresh.focusedId!=expectedFocus)return L"The focused object changed. Face the target again.";
    if(fresh.actions[slot].label.empty())return L"That action is not available.";
    engine::Name binding{};
    if(!Binding(slot,binding)||!Instance(liveFocus)||!SendAction(liveFocus,binding))
        return interactionFaulted ? L"Interaction failed and has been disabled. Navigation is still available." : L"This object's input interface is not supported.";
    return {};
}
void SetLandmarks(const std::vector<Landmark>& value) {
    if (thread && thread != GetCurrentThreadId()) return;
    landmarks.assign(value.begin(), value.begin() + (std::min)(value.size(), size_t(16)));
}
void Reset() {
    if (thread && thread != GetCurrentThreadId()) return;
    fields.clear(); controllers.clear(); menus.clear(); destinations.clear();
    cursor = 0; ready = false; trace = 0; lastState.clear();
    // A native fault remains latched for this DLL lifetime; menu sampling is independent.
}
bool Sample(Snapshot& s) {
    s = {};
    if (!thread) thread = GetCurrentThreadId();
    if (thread != GetCurrentThreadId()) { s.reason = "wrong sampling thread"; return false; }
    if (!base) base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    LARGE_INTEGER before{}, after{}, frequency{};
    QueryPerformanceCounter(&before);
    bool ok = false;
    try { if (!faulted) ok = SafeSample(s); }
    catch (...) { faulted = true; }
    if (faulted) { s = {}; s.reason = "world sampler fault; disabled independently of menus"; }
    if (s.reason != lastState) {
        QueryPerformanceCounter(&after); QueryPerformanceFrequency(&frequency);
        double ms = frequency.QuadPart ? 1000.0 * (after.QuadPart - before.QuadPart) / frequency.QuadPart : 0;
        Log("event=world_calibration active=" + std::to_string(ok) + " reason=" + s.reason + " sample_ms=" + std::to_string(ms));
        lastState = s.reason;
    }
    return ok;
}
}
