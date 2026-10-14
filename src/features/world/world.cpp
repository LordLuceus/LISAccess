#include "features/world/session.h"
#include "features/world/target_list.h"
#include "features/interactions/scanner.h"
#include "platform/log.h"
#include "platform/foreground.h"
#include "engine/abi.h"
#include "engine/build_profile.h"
#include "features/navigation/world_sample.h"
#include <algorithm>
#include <cmath>
#include <cctype>
namespace lis::world {
using engine::Copy;
using engine::Read;
using engine::Array;
Session& GameSession() { static Session session; return session; }
static bool SampleImpl(Session& session,Snapshot& s) {
    uintptr_t pawn=0;
    unsigned queries=0;
    if(!GatePlayer(session,s,pawn)||!SampleGeometry(session.reflection.registry,session.base,session.trace,pawn,s,queries))return false;
    for (uintptr_t actor : session.destinations) {
        if (!session.reflection.registry.Instance(actor) || !session.reflection.registry.IsA(actor,"Actor") || Read<uintptr_t>(actor + 0xd8) != session.liveWorld) continue;
        std::string path = session.reflection.registry.Path(actor);
        for (const Landmark& landmark : session.landmarks) if (EqualPath(landmark.actorPath,path) && !landmark.label.empty()) {
            Vec3 position{};
            if (Copy(&position, actor + 0x54, sizeof(position)) && Finite(position))
                s.targets.push_back({path, landmark.label, position, Kind::Destination});
        }
    }
    const size_t authored=s.targets.size();
    if(!session.interactionFaulted&&!SafeInteractions(session,s)) {
        s.targets.resize(authored);s.focusedId.clear();s.focusedLabel.clear();s.actions={};
        Log("event=interaction_fault status=disabled navigation=preserved");
    }
    s.interactionsFailed=session.interactionFaulted;
    if(session.interactionFaulted)s.interactionStatus="fault_disabled";
    PrepareTargets(s);
    static std::string lastInteractionState;
    const std::string state=s.interactionStatus+" targets="+std::to_string(s.targets.size()-authored)+" focused="+std::to_string(!s.focusedId.empty());
    if(state!=lastInteractionState){Log("event=interaction_scan status="+state);lastInteractionState=state;}
    s.active = true;
    s.reason = "calibrated local geometry";
    s.diagnostics = "queries=" + std::to_string(queries) + "; floor=positive; units=engine; partial body sweeps, not a clear route; drop=no support within bounded ray; interactions=all loaded registered eligible actors; room_filter=none";
    return true;
}
bool SafeSample(Session& session,Snapshot& s) {
    __try { return SampleImpl(session,s); }
    __except (EXCEPTION_EXECUTE_HANDLER) { session.faulted = true; return false; }
}
void SetLandmarks(const std::vector<Landmark>& value) {
    auto& session=GameSession();
    if (session.thread && session.thread != GetCurrentThreadId()) return;
    session.landmarks.assign(value.begin(), value.begin() + (std::min)(value.size(), size_t(16)));
}
void Reset() {
    auto& session=GameSession();
    if (session.thread && session.thread != GetCurrentThreadId()) return;
    session.reflection.Reset(); session.controllers.clear(); session.menus.clear(); session.destinations.clear();
    session.cursor = 0; session.ready = false; session.trace = 0; session.lastState.clear();
    // A native fault remains latched for this DLL lifetime; menu sampling is independent.
}
bool Sample(Snapshot& s) {
    auto& session=GameSession();
    s = {};
    if (!session.thread) session.thread = GetCurrentThreadId();
    if (session.thread != GetCurrentThreadId()) { s.reason = "wrong sampling thread"; return false; }
    if (!session.base) {
        session.base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        session.reflection.registry=engine::ObjectRegistry(session.base,engine::RegistryPolicy::World);
    }
    LARGE_INTEGER before{}, after{}, frequency{};
    QueryPerformanceCounter(&before);
    bool ok = false;
    try { if (!session.faulted) ok = SafeSample(session,s); }
    catch (...) { session.faulted = true; }
    if (session.faulted) { s = {}; s.reason = "world sampler fault; disabled independently of menus"; }
    if (s.reason != session.lastState) {
        QueryPerformanceCounter(&after); QueryPerformanceFrequency(&frequency);
        double ms = frequency.QuadPart ? 1000.0 * (after.QuadPart - before.QuadPart) / frequency.QuadPart : 0;
        Log("event=world_calibration active=" + std::to_string(ok) + " reason=" + s.reason + " sample_ms=" + std::to_string(ms));
        session.lastState = s.reason;
    }
    return ok;
}
}
