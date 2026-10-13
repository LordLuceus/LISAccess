#include "features/navigation/world_sample.h"
#include <algorithm>
#include <cmath>
namespace lis::world {
bool SampleGeometry(const engine::ObjectRegistry& registry,uintptr_t base,uintptr_t trace,
                    uintptr_t pawn,Snapshot& s,unsigned& queries) {
    constexpr float Pi=3.14159265358979323846f;
    TraceParams floor{};
    Vec3 floorEnd = s.player; floorEnd.z -= s.halfHeight * 3;
    if (!Query(registry,base,trace,pawn, s.player, floorEnd, {}, floor) || !floor.actor || floor.normal.z < 0.65f ||
        floor.location.z > s.player.z || s.player.z - floor.location.z > s.halfHeight * 1.8f) {
        s.reason = "floor calibration unknown"; return false;
    }
    const float range = s.radius * 6;
    const Vec3 extent{s.radius * 0.8f, s.radius * 0.8f, s.halfHeight * 0.4f};
    queries = 1;
    for (size_t i = 0; i < s.probes.size(); ++i) {
        Probe& probe = s.probes[i]; probe.angle = (static_cast<float>(i) - 1) * 35 * Pi / 180;
        float angle = s.cameraYaw + probe.angle;
        Vec3 end{s.player.x + std::cos(angle)*range, s.player.y + std::sin(angle)*range, s.player.z};
        TraceParams hit{}; ++queries;
        if (!Query(registry,base,trace,pawn, s.player, end, extent, hit)) continue;
        // A floor contact terminates a sweep, so space behind it is UNKNOWN, not clear.
        if (hit.actor && hit.normal.z >= 0.65f) continue;
        probe.known = true; probe.hit = hit.actor != 0;
        probe.distance = probe.hit ? (std::min)(range, Length(Sub(hit.location, s.player))) : range;
        probe.normal = hit.normal;
    }
    Vec3 aheadStart{s.player.x + std::cos(s.cameraYaw)*s.radius*3, s.player.y + std::sin(s.cameraYaw)*s.radius*3, s.player.z};
    Vec3 down = aheadStart; down.z -= s.halfHeight * 4;
    TraceParams forwardFloor{}; ++queries;
    if (Query(registry,base,trace,pawn, aheadStart, down, {}, forwardFloor)) {
        if (!forwardFloor.actor) { s.dropKnown = true; s.dropAhead = true; }
        else if (forwardFloor.normal.z >= 0.65f && forwardFloor.location.z <= aheadStart.z) {
            s.dropKnown = true;
            s.dropAhead = floor.location.z - forwardFloor.location.z > s.halfHeight * 0.6f;
        }
    }
    return true;
}
}
