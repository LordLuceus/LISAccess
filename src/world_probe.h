#pragma once
#include <array>
#include <string>
#include <vector>

namespace lis::world {
struct Vec3 { float x = 0, y = 0, z = 0; };
enum class Kind { Object, Person, Destination };
struct Target { std::string stableId; std::wstring label; Vec3 position; Kind kind = Kind::Object; };
struct Landmark { std::string actorPath; std::wstring label; };
struct Probe { bool known = false, hit = false; float distance = 0, angle = 0; Vec3 normal; };
struct Snapshot {
    bool active = false;
    std::string reason;
    Vec3 player;
    float cameraYaw = 0, radius = 0, halfHeight = 0;
    std::vector<Target> targets;
    std::array<Probe, 3> probes{};
    bool dropKnown = false, dropAhead = false;
    std::string diagnostics;
};
// All entry points must run on the same game/window thread, after runtime build verification.
// First Sample binds that thread; Reset does not permit rebinding. No worker calls.
void SetLandmarks(const std::vector<Landmark>& landmarks);
bool Sample(Snapshot& snapshot); // true only for an active, calibrated sample
void Reset();
}
