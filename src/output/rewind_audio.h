#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lis::audio::detail {
constexpr size_t NavigationCount = 4;
constexpr size_t CueCount = 6;
constexpr uint32_t RewindSampleRate = 24000;
// Kept independent of the audio device so group policy can be tested silently.
struct GroupGate {
    bool navigation = false;
    bool rewind = false;
    bool Allows(size_t index) const {
        return index < CueCount && (index < NavigationCount ? navigation : rewind);
    }
    bool Any() const { return navigation || rewind; }
    void Reset() { navigation = rewind = false; }
};

// Little-endian mono PCM16: two 18 ms ticks separated by 32 ms, or a
// 120 ms descending chirp. Smooth envelopes prevent abrupt sample edges.
inline std::vector<uint8_t> RewindPcm(bool crossed) {
    constexpr double pi = 3.14159265358979323846;
    const size_t samples = crossed ? 2880 : 1632;
    std::vector<uint8_t> pcm(samples * 2, 0);
    for (size_t i = 0; i < samples; ++i) {
        double value = 0;
        if (crossed) {
            const double t = double(i) / RewindSampleRate;
            const double envelope = std::sin(pi * double(i) / double(samples - 1));
            value = envelope * std::sin(2 * pi * (1000 * t - 2500 * t * t));
        } else {
            const size_t local = i < 432 ? i : (i >= 1200 ? i - 1200 : 432);
            if (local < 432) {
                const double envelope = std::sin(pi * double(local) / 431);
                value = envelope * std::sin(2 * pi * 1600 * double(local) / RewindSampleRate);
            }
        }
        const auto sample = static_cast<uint16_t>(static_cast<int16_t>(value * 12000));
        pcm[i * 2] = static_cast<uint8_t>(sample);
        pcm[i * 2 + 1] = static_cast<uint8_t>(sample >> 8);
    }
    return pcm;
}
}
