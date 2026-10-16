#pragma once
#include "features/navigation/route_guide.h"
#include <cstdint>
namespace lis::navigation::guidance {
struct View {
    route::Result route;
    world::Vec3 player;
    float yaw=0,radius=0;
    std::string selected;
    uint64_t generation=0,sampledAt=0;
};
// Worker-side requests/results; native sampling never runs on the worker.
void Request(const std::string& selected);
void Cancel();
View Read();
// Game/window thread. Observe consumes the existing scanner sample; Tick
// refreshes player context and uses bounded collision queries while requested.
void Observe(const world::Snapshot&);
void Tick();
void Shutdown();
}
