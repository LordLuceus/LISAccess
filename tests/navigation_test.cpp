#include "features/navigation/math.h"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace lis::navmath;
void check(bool condition, const char* name) {
    if (!condition) { std::cerr << "FAILED: " << name << '\n'; std::exit(1); }
}
int main() {
    check(std::abs(Bearing(10, 0, 0)) < .001f, "ahead");
    check(Pan(Bearing(0, 10, 0)) > .999f, "positive Y right");
    check(Pan(Bearing(0, -10, 0)) < -.999f, "negative Y left");
    check(std::abs(Bearing(0, 10, Pi/2)) < .001f, "camera rather than pawn basis");
    check(Behind(Bearing(-10, 0, 0)), "rear cue distinguished from centred front");
    check(std::wstring(Direction(-Pi/4)) == L"ahead left", "direction wording");
    check(std::wstring(Direction(Pi)) == L"behind", "wraparound direction");
    check(Interval(0, 120) == 250 && Interval(120,120) == 1100, "proximity cadence");
    check(Interval(-10,120) == 250 && Interval(1000,120) == 1100, "cadence bounds");
    check(std::wstring(Distance(80,21)) == L"very close", "distance bands not guessed metres");
    std::cout << "PASS: 10 navigation geometry cases\n";
}
