// Ported from the shelved autowalk pure geometry/planner regression; no native dependencies.
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <deque>
#include <map>
#include <utility>
#include <vector>

namespace lis::navigation::route::routing {
struct Vec3 { double x{}, y{}, z{}; };
struct Goal {
    Vec3 center{}; // Target XY plus expected approach FLOOR elevation, not actor Z.
    double minRadius{}, maxRadius{84}, heightTolerance{2};
};
struct Config {
    double pitch{42}, flatTolerance{2};
    int maxCells{64}; // Inclusive coordinate bounds relative to start.
    std::size_t maxNodes{2048}, maxEdges{8192}, maxDeferred{2};
    double maxMilliseconds{1000}; // Cumulative active planning time, not wall time.
};
enum class Status { Planning, Found, NoRoute, Limit, Invalid };
enum class EdgeState { Traversable, Blocked, Unsupported, Deferred };
struct EdgeQuery { Vec3 from, to; double radius, halfHeight; };
struct EdgeResult { EdgeState state{EdgeState::Unsupported}; double floorZ{}; };
struct Statistics {
    std::size_t nodes{}, edges{}, deferred{}; double milliseconds{};
    // Probe classifications; traversable includes a clear edge rejected by the
    // node cap. Bounds are skipped neighbors, deferredExhausted counts edges
    // whose retries expired (deferred counts individual probe attempts).
    std::size_t traversable{},blocked{},unsupported{},bounds{},deferredExhausted{};
};

// Unit-cost Dijkstra (FIFO), four axis neighbors, no smoothing. The callback
// must positively verify the ENTIRE body sweep and destination floor; clearance
// alone is not Traversable. Missing floor is Blocked, unavailable query Unsupported.
// Floor/body calibration at Begin is caller-verified; start-only paths are vacuous.
class Planner {
public:
    void Reset() {
        nodes_.clear(); frontier_.clear(); index_.clear(); path_.clear();
        stats_ = {}; status_ = Status::Invalid; unknown_ = false;reason_="none";
    }
    Status Begin(Vec3 floor, Vec3 bodyCenter, double radius, double halfHeight,
                 Goal goal, Config config = {}) {
        Reset();
        reason_="invalid";
        config_ = config; goal_ = goal; start_ = floor;
        radius_ = radius; halfHeight_ = halfHeight;
        if (!Finite(floor) || !Finite(bodyCenter) || !Finite(goal.center) ||
            !Positive(radius) || !Positive(halfHeight) || radius > halfHeight ||
            !Positive(config.pitch) || !Nonnegative(config.flatTolerance) ||
            config.maxCells < 1 || config.maxCells > 1000000 ||
            !config.maxNodes || !config.maxEdges || config.maxDeferred > 1000 ||
            !Positive(config.maxMilliseconds) || !Nonnegative(goal.minRadius) ||
            !Nonnegative(goal.maxRadius) || goal.minRadius > goal.maxRadius ||
            !Nonnegative(goal.heightTolerance) ||
            std::abs(bodyCenter.x-floor.x) > 0.01 ||
            std::abs(bodyCenter.y-floor.y) > 0.01 ||
            std::abs(bodyCenter.z-floor.z-halfHeight) > config.flatTolerance ||
            !std::isfinite(config.pitch * config.maxCells) ||
            !std::isfinite(std::abs(floor.x) + config.pitch * config.maxCells) ||
            !std::isfinite(std::abs(floor.y) + config.pitch * config.maxCells))
            return status_;
        nodes_.push_back({0, 0, floor, 0, 0, 0});
        index_[{0, 0}] = 0; frontier_.push_back(0); stats_.nodes = 1;
        status_ = Status::Planning;reason_="planning";
        if (AtGoal(floor)) Finish(0);
        return status_;
    }
    template<class Probe>
    Status Step(Probe&& probe, std::size_t workBudget = 16, double tickMilliseconds = 2) {
        if (status_ != Status::Planning) return status_;
        if (!workBudget || !Positive(tickMilliseconds)) return status_;
        const auto begin = Clock::now();
        for (std::size_t work = 0; work < workBudget && status_ == Status::Planning; ++work) {
            const double elapsed = Milliseconds(begin);
            if (stats_.milliseconds + elapsed >= config_.maxMilliseconds) {
                status_ = Status::Limit;reason_="cpu_budget"; break;
            }
            if (elapsed >= tickMilliseconds) break;
            if (frontier_.empty()) {
                status_ = unknown_ ? Status::Limit : Status::NoRoute;
                reason_=unknown_?"exhausted_unknown":"frontier_empty";break;
            }
            const auto current = frontier_.front();
            // Copy: insertion may reallocate nodes_.
            const Node node = nodes_[current];
            if (node.direction == 4) { frontier_.pop_front(); continue; }
            constexpr int dx[4] = {1, 0, -1, 0};
            constexpr int dy[4] = {0, 1, 0, -1};
            const int x = node.x + dx[node.direction], y = node.y + dy[node.direction];
            if (std::abs(x) > config_.maxCells || std::abs(y) > config_.maxCells) {
                ++stats_.bounds;unknown_ = true; Advance(current); continue;
            }
            if (index_.count({x, y})) { Advance(current); continue; }
            if (stats_.edges >= config_.maxEdges) { status_ = Status::Limit;reason_="edge_budget"; break; }
            Vec3 destination{start_.x+x*config_.pitch, start_.y+y*config_.pitch, node.floor.z};
            const EdgeResult result = probe(EdgeQuery{node.floor, destination, radius_, halfHeight_});
            ++stats_.edges;
            if (result.state == EdgeState::Deferred) {
                ++stats_.deferred;
                if (++nodes_[current].retries <= config_.maxDeferred) continue;
                ++stats_.deferredExhausted;
                unknown_ = true; Advance(current); continue;
            }
            Advance(current);
            if (result.state == EdgeState::Blocked) {++stats_.blocked;continue;}
            if (result.state != EdgeState::Traversable || !std::isfinite(result.floorZ) ||
                std::abs(result.floorZ-start_.z) > config_.flatTolerance) {
                ++stats_.unsupported;unknown_ = true; continue;
            }
            ++stats_.traversable;
            if (nodes_.size() >= config_.maxNodes) { status_ = Status::Limit;reason_="node_budget"; break; }
            destination.z = result.floorZ;
            const auto next = nodes_.size();
            nodes_.push_back({x, y, destination, current, 0, 0});
            index_[{x, y}] = next; frontier_.push_back(next); ++stats_.nodes;
            if (AtGoal(destination)) Finish(next);
        }
        stats_.milliseconds += Milliseconds(begin);
        // A callback cannot be preempted. Its time counts toward the cap.
        if (stats_.milliseconds >= config_.maxMilliseconds) {
            status_ = Status::Limit;reason_="cpu_budget";
            path_.clear(); // A late successful probe must not bypass the budget.
        }
        return status_;
    }
    Status GetStatus() const { return status_; }
    const std::vector<Vec3>& Path() const { return path_; }
    const Statistics& Stats() const { return stats_; }
    // Static literals: none, invalid, planning, found, frontier_empty,
    // cpu_budget, node_budget, edge_budget, exhausted_unknown.
    const char* StopReason() const { return reason_; }
private:
    using Clock = std::chrono::steady_clock;
    struct Node { int x, y; Vec3 floor; std::size_t parent; int direction; std::size_t retries; };
    static double Milliseconds(Clock::time_point begin) {
        return std::chrono::duration<double, std::milli>(Clock::now()-begin).count();
    }
    static bool Positive(double value) { return std::isfinite(value) && value > 0; }
    static bool Nonnegative(double value) { return std::isfinite(value) && value >= 0; }
    static bool Finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    bool AtGoal(Vec3 p) const {
        const double distance = std::hypot(p.x-goal_.center.x, p.y-goal_.center.y);
        return distance >= goal_.minRadius && distance <= goal_.maxRadius &&
            std::abs(p.z-goal_.center.z) <= goal_.heightTolerance;
    }
    void Advance(std::size_t node) { ++nodes_[node].direction; nodes_[node].retries = 0; }
    void Finish(std::size_t node) {
        for (;;) {
            path_.push_back(nodes_[node].floor);
            if (!node) break;
            node = nodes_[node].parent;
        }
        std::reverse(path_.begin(), path_.end()); status_ = Status::Found;reason_="found";
    }
    Config config_{};
    Goal goal_{};
    Vec3 start_{};
    double radius_{}, halfHeight_{};
    Status status_{Status::Invalid};
    Statistics stats_{};
    const char* reason_="none";
    bool unknown_{};
    std::vector<Node> nodes_;
    std::deque<std::size_t> frontier_;
    std::map<std::pair<int, int>, std::size_t> index_;
    std::vector<Vec3> path_;
};
} // namespace lis::navigation::route::routing
