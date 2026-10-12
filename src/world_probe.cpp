#include "world_probe.h"
#include "platform.h"
#include "engine_abi.h"
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
DWORD thread = 0;
bool faulted = false, ready = false;
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
bool ClearBool(uintptr_t object, const char* owner, const char* name) {
    uintptr_t f = Field(owner, name, "BoolProperty", 4);
    uint32_t value = 0, mask = Read<uint32_t>(f + 0x80);
    return f && mask && Copy(&value, object + Read<int>(f + 0x60), 4) && !(value & mask);
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
bool SampleImpl(Snapshot& s) {
    if (!Discover()) { s.reason = "discovering bounded world metadata"; return false; }
    for (uintptr_t m : menus) if (Instance(m) && (ClassOf(m)=="PauseMenu" || ClassOf(m)=="OutGameSensenMenu")) {
        unsigned char opening = 0;
        if (!Value(m, "SensenMenuBase", "OpeningState", "ByteProperty", opening) || opening == 2) {
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
    s.active = true;
    s.reason = "calibrated local geometry";
    s.diagnostics = "queries=" + std::to_string(queries) + "; floor=positive; units=engine; partial body sweeps, not a clear route; drop=no support within bounded ray; destinations=authored approach triggers; interactions=disabled (eligibility and NPC cues unverified)";
    return true;
}
bool SafeSample(Snapshot& s) {
    __try { return SampleImpl(s); }
    __except (EXCEPTION_EXECUTE_HANDLER) { faulted = true; return false; }
}
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
