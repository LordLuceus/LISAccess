#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace lis::navmath {
constexpr float Pi = 3.14159265358979323846f;
inline float Bearing(float dx, float dy, float cameraYaw) {
    return std::remainder(std::atan2(dy, dx) - cameraYaw, 2.f * Pi);
}
inline float Pan(float bearing) { return std::sin(bearing); }
inline bool Behind(float bearing) { return std::cos(bearing) < 0.f; }
inline unsigned Interval(float distance, float range) {
    const float fraction = range > 0 ? std::clamp(distance / range, 0.f, 1.f) : 1.f;
    return static_cast<unsigned>(250.f + 850.f * fraction);
}
inline const wchar_t* Direction(float bearing) {
    const int index = (static_cast<int>(std::round(bearing / (Pi / 4))) + 8) % 8;
    const wchar_t* names[] = {L"ahead", L"ahead right", L"right", L"behind right",
        L"behind", L"behind left", L"left", L"ahead left"};
    return names[index];
}
inline const wchar_t* Distance(float distance, float radius) {
    if (distance <= radius * 5) return L"very close";
    if (distance <= radius * 25) return L"nearby";
    return L"farther away";
}
}
