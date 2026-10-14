#include "output/rewind_audio.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
using namespace lis::audio::detail;
void check(bool condition, const char* name) {
    if (!condition) { std::cerr << "FAILED: " << name << '\n'; std::exit(1); }
}
int Sample(const std::vector<uint8_t>& pcm, size_t i) {
    const int raw = pcm[i * 2] | (int(pcm[i * 2 + 1]) << 8);
    return raw >= 32768 ? raw - 65536 : raw;
}
size_t Crossings(const std::vector<uint8_t>& pcm, size_t begin, size_t end) {
    size_t count = 0;
    for (size_t i = begin + 1; i < end; ++i)
        if (Sample(pcm, i - 1) < 0 && Sample(pcm, i) >= 0) ++count;
    return count;
}
int main() {
    GroupGate gate;
    check(!gate.Any() && !gate.Allows(0) && !gate.Allows(4), "both groups initially disabled");
    gate.rewind = true;
    check(gate.Any() && !gate.Allows(0) && gate.Allows(4) && gate.Allows(5), "rewind independent of navigation");
    gate.navigation = true;
    check(gate.Allows(0) && gate.Allows(3) && gate.Allows(5), "both groups enabled");
    gate.navigation = false;
    check(!gate.Allows(3) && gate.Allows(4) && gate.Allows(5), "navigation disabling preserves rewind");
    gate.navigation = true;
    gate.rewind = false;
    check(gate.Allows(0) && !gate.Allows(4), "rewind disabling preserves navigation");
    check(!gate.Allows(CueCount) && !gate.Allows(size_t(-1)), "invalid cue rejected");
    gate.Reset();
    check(!gate.Any() && !gate.Allows(0) && !gate.Allows(5), "stop all resets both groups");
    const auto approach = RewindPcm(false), crossed = RewindPcm(true);
    check(approach.size() == 3264 && crossed.size() == 5760, "bounded short mono PCM16 defaults");
    check(approach == RewindPcm(false) && crossed == RewindPcm(true), "deterministic generation");
    for (const auto* pcm : {&approach, &crossed}) {
        check(Sample(*pcm, 0) == 0 && Sample(*pcm, pcm->size() / 2 - 1) == 0, "smooth zero endpoints");
        int peak = 0;
        for (size_t i = 0; i < pcm->size() / 2; ++i) peak = std::max(peak, std::abs(Sample(*pcm, i)));
        check(peak > 10000 && peak <= 12000, "non-silent conservative peak without clipping");
    }
    check(std::all_of(approach.begin() + 432 * 2, approach.begin() + 1200 * 2,
                     [](uint8_t v) { return v == 0; }), "double tick has silent gap");
    check(Crossings(approach, 0, 432) > 20 && Crossings(approach, 1200, 1632) > 20, "both ticks present");
    check(Crossings(crossed, 0, 960) > Crossings(crossed, 1920, 2880), "crossing pitch descends");
    std::cout << "PASS: rewind audio group policy and generated PCM (no audio device)\n";
}
