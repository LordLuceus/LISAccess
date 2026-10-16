// Ported from the shelved autowalk pure geometry/planner regression; no native dependencies.
#include "features/navigation/route_planner.h"
#include <thread>
#include "fixtures/route_capture.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <tuple>

using namespace lis::navigation::route::routing;
namespace {
void Check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
Config Small() {
    Config config;
    config.pitch = 1; config.maxCells = 8; config.maxMilliseconds = 10000;
    return config;
}
Goal Exact(double x, double y, double z = 0) { return {{x,y,z},0,0,0.01}; }
Status Begin(Planner& planner, Goal goal, Config config = Small()) {
    return planner.Begin({0,0,0}, {0,0,2}, 1, 2, goal, config);
}
template<class Probe> Status Run(Planner& planner, Probe probe) {
    for (int ticks = 0; ticks < 10000 && planner.GetStatus() == Status::Planning; ++ticks)
        planner.Step(probe, 3, 100);
    Check(planner.GetStatus() != Status::Planning, "bounded termination");
    return planner.GetStatus();
}
using Cell = std::pair<int,int>;
using Edge = std::pair<Cell,Cell>;
Cell Grid(Vec3 p, double pitch = 1) {
    return {static_cast<int>(std::lround(p.x/pitch)), static_cast<int>(std::lround(p.y/pitch))};
}
void Captured(const route_fixture::Capture& capture) {
    Planner planner;
    Goal goal{{capture.targetX,capture.targetY,0},21,84,0.01};
    Config config; config.maxMilliseconds = 10000;
    Check(planner.Begin({0,0,0},{0,0,44},21,44,goal,config) == Status::Planning,
          "captured start requires route");
    std::set<Edge> positive;
    for (std::size_t i=1; i<capture.witness.size(); ++i)
        positive.insert({capture.witness[i-1],capture.witness[i]});
    Check(Run(planner, [&](const EdgeQuery& query) {
        Check(query.radius == 21 && query.halfHeight == 44, "captured body dimensions");
        return EdgeResult{positive.count({Grid(query.from,42),Grid(query.to,42)}) ?
            EdgeState::Traversable : EdgeState::Unsupported, 0};
    }) == Status::Found, "captured start reaches candidate door annulus");
    const auto& path = planner.Path();
    Check(path.size() == capture.witness.size(), "no unverified shortcuts");
    for (std::size_t i=1; i<path.size(); ++i)
        Check(positive.count({Grid(path[i-1],42),Grid(path[i],42)}) != 0,
              "every captured path edge positively verified");
    Check(std::hypot(path.back().x-goal.center.x,path.back().y-goal.center.y) <= 84,
          "candidate proximity only, not native arrival");
}
} // namespace

int main() {
    Planner planner;
    auto blocked = [](const EdgeQuery&) { return EdgeResult{EdgeState::Blocked,0}; };
    auto clear = [](const EdgeQuery&) { return EdgeResult{EdgeState::Traversable,0}; };
    Check(Begin(planner,Exact(0,0)) == Status::Found && planner.Path().size() == 1,
          "goal at positively calibrated start");
    int calls = 0;
    planner.Step([&](const EdgeQuery&) { ++calls; return EdgeResult{}; });
    Check(calls == 0, "terminal Step is inert");
    planner.Reset();
    Check(planner.GetStatus() == Status::Invalid && planner.Path().empty() &&
          planner.Stats().nodes == 0, "reset cancels and clears evidence");
    Begin(planner,Exact(2,0));
    Check(Run(planner,blocked) == Status::NoRoute && planner.Stats().edges == 4,
          "boxed in or absent floor proves local no route");
    Begin(planner,Exact(2,0));
    Check(Run(planner,[](const EdgeQuery&) { return EdgeResult{}; }) == Status::Limit,
          "unavailable evidence is not no route");
    Begin(planner,Exact(2,0));
    Check(Run(planner,[](const EdgeQuery&) { return EdgeResult{EdgeState::Deferred,0}; })
          == Status::Limit && planner.Stats().edges == 12, "bounded deferred retries");
    Begin(planner,Exact(2,0));
    planner.Step(clear,1,100);
    Check(planner.Stats().edges == 1 && planner.GetStatus() == Status::Planning,
          "per tick work budget");
    planner.Reset();
    planner.Step(clear);
    Check(planner.Stats().edges == 0 && planner.Path().empty(), "cancel pending search");

    // This route initially moves away from the goal and cannot cut corners.
    const std::vector<Cell> detour{{0,0},{-1,0},{-1,1},{-1,2},{0,2},{1,2},{2,2},{2,1},{2,0}};
    std::set<Edge> allowed;
    for (std::size_t i=1; i<detour.size(); ++i) allowed.insert({detour[i-1],detour[i]});
    Begin(planner,Exact(2,0));
    Check(Run(planner,[&](const EdgeQuery& q) {
        return EdgeResult{allowed.count({Grid(q.from),Grid(q.to)}) ?
            EdgeState::Traversable : EdgeState::Blocked,0};
    }) == Status::Found, "detour requires moving away first");
    Check(planner.Path().size() == detour.size() && planner.Path()[1].x == -1,
          "verified unsmoothed four neighbor path");
    Begin(planner,Exact(2,0));
    Check(Run(planner,[](const EdgeQuery& q) {
        return EdgeResult{q.to.x <= 0 && std::abs(q.to.y) <= 1 && q.to.x >= -1 ?
            EdgeState::Traversable : EdgeState::Blocked,0};
    }) == Status::NoRoute, "disconnected supported island");

    auto config = Small(); config.maxCells = 1;
    Begin(planner,Exact(3,0),config);
    Check(Run(planner,clear) == Status::Limit, "search boundary is not no route");
    config = Small(); config.maxNodes = 1;
    Begin(planner,Exact(3,0),config);
    Check(Run(planner,clear) == Status::Limit && planner.Path().empty(), "node cap");
    config = Small(); config.maxEdges = 1;
    Begin(planner,Exact(3,0),config);
    Check(Run(planner,clear) == Status::Limit && planner.Stats().edges == 1, "edge cap");
    config = Small(); config.maxMilliseconds = std::numeric_limits<double>::min();
    Begin(planner,Exact(3,0),config);
    Check(Run(planner,clear) == Status::Limit, "active time cap");
    config = Small(); config.maxMilliseconds = 1000;
    Begin(planner,Exact(1,0),config);
    bool slowProbeCalled=false;
    Check(planner.Step([&](const EdgeQuery&) {
        slowProbeCalled=true;std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        return EdgeResult{EdgeState::Traversable,0};
    },1,1000)==Status::Limit&&slowProbeCalled&&planner.Path().empty(),
          "successful but over-budget native callback cannot authorize a late path");
    Begin(planner,Exact(2,0));
    Check(Run(planner,[](const EdgeQuery&) { return EdgeResult{EdgeState::Traversable,10}; })
          == Status::Limit, "nonflat floor unsupported");
    Begin(planner,Exact(2,0));
    Check(Run(planner,[](const EdgeQuery&) {
        return EdgeResult{EdgeState::Traversable,std::numeric_limits<double>::quiet_NaN()};
    }) == Status::Limit, "NaN probe evidence never admitted");
    Begin(planner,{{0,0,0},1,2,0});
    Check(planner.GetStatus() == Status::Planning && Run(planner,clear) == Status::Found,
          "minimum approach radius excludes start");
    Begin(planner,Exact(0,0,5));
    Check(Run(planner,blocked) == Status::NoRoute, "height tolerance enforced");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    Check(Begin(planner,Exact(nan,0)) == Status::Invalid, "NaN goal");
    config = Small(); config.pitch = nan;
    Check(Begin(planner,Exact(2,0),config) == Status::Invalid, "NaN pitch");
    config = Small(); config.maxNodes = 0;
    Check(Begin(planner,Exact(2,0),config) == Status::Invalid, "zero cap");
    Check(Begin(planner,{{0,0,0},2,1,0}) == Status::Invalid, "inverted goal annulus");
    Check(planner.Begin({nan,0,0},{0,0,2},1,2,Exact(2,0)) == Status::Invalid, "NaN start");
    Check(planner.Begin({0,0,0},{0,0,2},-1,2,Exact(2,0)) == Status::Invalid, "bad body size");
    Check(planner.Begin({0,0,0},{0,0,20},1,2,Exact(2,0)) == Status::Invalid,
          "floor/body calibration disagreement");
    Captured(route_fixture::original);
    Captured(route_fixture::expanded);
    std::cout << "autowalk planner tests passed\n";
}
