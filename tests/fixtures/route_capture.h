// Ported from the shelved autowalk pure geometry/planner regression; no native dependencies.
#pragma once
#include <utility>
#include <vector>

namespace route_fixture {
// Minimized geometry regression, derived from room-scanner/artifacts/
// room-probe-capture.json and room-probe-expanded-capture.json, respectively.
// Translate each captured start XY to (0,0), floor Z=1 to 0; grid pitch=42,
// captured body radius=21, half-height=44. Retain ONLY directed body-tested
// reachable edges along a BFS witness to the classroom door's 21..84 XY annulus.
// BFS sorted neighbors lexicographically; exclude an undirected pair if ANY
// captured direction/sample is not reachable (2 / 1 conflicting pairs removed).
// Retained floor heights were 1 within capture float tolerance (0.0001).
// This is deliberately NOT a room
// map: omitted edges are unknown, not blocked. No actor identities/assets.
// The target is translated XY with expected approach floor Z=0, NOT actor Z.
// Candidate proximity is not native focus/arrival evidence.
struct Capture { double targetX, targetY; std::vector<std::pair<int, int>> witness; };
inline const Capture original{549.100204, 556.666916, {
    {0,0}, {0,1}, {1,1}, {2,1}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5},
    {3,6}, {3,7}, {3,8}, {3,9}, {3,10}, {3,11}, {3,12}, {4,12},
    {5,12}, {6,12}, {7,12}, {8,12}, {9,12}, {10,12}, {11,12}, {12,12}
}};
inline const Capture expanded{420.309196, 647.037277, {
    {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8},
    {0,9}, {0,10}, {0,11}, {0,12}, {0,13}, {0,14}, {1,14}, {2,14},
    {3,14}, {4,14}, {5,14}, {6,14}, {7,14}, {8,14}, {9,14}
}};
} // namespace route_fixture
