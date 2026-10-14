#ifndef NOMINMAX
#define NOMINMAX
#endif
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include "output/audio_cues.h"
#include "output/rewind_audio.h"
#include "platform/log.h"
#include <xaudio2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace lis::audio {
namespace {
struct Sound {
    std::vector<BYTE> data;
    IXAudio2SourceVoice* voice = nullptr;
};
IXAudio2* engine = nullptr;
IXAudio2MasteringVoice* master = nullptr;
std::array<Sound, detail::CueCount> sounds;
detail::GroupGate gate;
bool comInitialized = false;
constexpr const wchar_t* names[] = {L"obstacle.wav", L"person.wav", L"object.wav", L"destination.wav",
                                  L"rewind_approach.wav", L"rewind_crossed.wav"};
constexpr const char* labels[] = {"obstacle", "person", "object", "destination", "rewind_approach", "rewind_crossed"};
uint16_t U16(const BYTE* p) { return uint16_t(p[0] | (uint16_t(p[1]) << 8)); }
uint32_t U32(const BYTE* p) { return uint32_t(U16(p)) | (uint32_t(U16(p + 2)) << 16); }
// Intentionally supports only replaceable mono signed PCM16 RIFF/WAVE, 8-48 kHz,
// <=2 seconds, <=256 KiB. Extensible/float/compressed/multichannel are rejected.
bool Load(const std::filesystem::path& path, Sound& sound) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto length = file.tellg();
    if (length < 44 || length > 262144) return false;
    std::vector<BYTE> bytes(static_cast<size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return false;
    if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data() + 8, "WAVE", 4)) return false;
    const uint64_t end = uint64_t(U32(bytes.data() + 4)) + 8;
    if (end != bytes.size()) return false;
    WAVEFORMATEX format{};
    size_t dataOffset = 0, dataSize = 0;
    bool haveFormat = false;
    size_t pos = 12;
    while (pos + 8 <= end) {
        const BYTE* chunk = bytes.data() + pos;
        const uint32_t size = U32(chunk + 4);
        pos += 8;
        if (uint64_t(pos) + size + (size & 1) > end) return false;
        if (!std::memcmp(chunk, "fmt ", 4)) {
            if (haveFormat || size < 16) return false;
            const BYTE* f = bytes.data() + pos;
            format.wFormatTag = U16(f);
            format.nChannels = U16(f + 2);
            format.nSamplesPerSec = U32(f + 4);
            format.nAvgBytesPerSec = U32(f + 8);
            format.nBlockAlign = U16(f + 12);
            format.wBitsPerSample = U16(f + 14);
            haveFormat = true;
        } else if (!std::memcmp(chunk, "data", 4)) {
            if (dataOffset) return false;
            dataOffset = pos;
            dataSize = size;
        }
        pos += size + (size & 1);
    }
    if (pos != end || !haveFormat || !dataOffset || !dataSize || (dataSize % 2) ||
        format.wFormatTag != WAVE_FORMAT_PCM || format.nChannels != 1 ||
        format.wBitsPerSample != 16 || format.nBlockAlign != 2 ||
        format.nSamplesPerSec < 8000 || format.nSamplesPerSec > 48000 ||
        format.nAvgBytesPerSec != format.nSamplesPerSec * 2 ||
        dataSize > format.nSamplesPerSec * 4) return false;
    sound.data.assign(bytes.begin() + dataOffset, bytes.begin() + dataOffset + dataSize);
    return SUCCEEDED(engine->CreateSourceVoice(&sound.voice, &format, 0, 2.0f));
}
bool DefaultRewind(size_t index) {
    auto& sound = sounds[index];
    sound.data = detail::RewindPcm(index == static_cast<size_t>(Cue::RewindCrossed));
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = detail::RewindSampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = format.nSamplesPerSec * 2;
    return SUCCEEDED(engine->CreateSourceVoice(&sound.voice, &format, 0, 2.0f));
}
void Flush(size_t begin, size_t end) {
    for (size_t i = begin; i < end; ++i) if (sounds[i].voice) {
        sounds[i].voice->Stop(0);
        sounds[i].voice->FlushSourceBuffers();
    }
}
void SetGroup(bool rewind, bool value) {
    bool& enabled = rewind ? gate.rewind : gate.navigation;
    if (enabled == value) return;
    if (value) {
        enabled = engine && SUCCEEDED(engine->StartEngine());
        return;
    }
    enabled = false;
    if (engine) engine->StopEngine();
    Flush(rewind ? detail::NavigationCount : 0, rewind ? sounds.size() : detail::NavigationCount);
    if (engine && gate.Any() && FAILED(engine->StartEngine())) StopAll();
}
}

void StopAll() {
    gate.Reset();
    // StopEngine synchronizes with the processing pass; flush cannot leak an old
    // buffer on re-enable. No queued commands or deferred bursts are retained.
    if (engine) engine->StopEngine();
    Flush(0, sounds.size());
}
bool SetMasterGain(float gain) {
    if (!master || !std::isfinite(gain)) return false;
    return SUCCEEDED(master->SetVolume(std::clamp(gain, 0.0f, 1.0f)));
}
void SetEnabled(bool value) { SetGroup(false, value); }
void SetRewindEnabled(bool value) { SetGroup(true, value); }
void Shutdown() {
    StopAll();
    for (auto& sound : sounds) {
        if (sound.voice) sound.voice->DestroyVoice();
        sound.voice = nullptr;
        sound.data.clear();
    }
    if (master) master->DestroyVoice();
    master = nullptr;
    if (engine) engine->Release();
    engine = nullptr;
    if (comInitialized) CoUninitialize();
    comInitialized = false;
}
bool Initialize(HMODULE module) {
    Shutdown();
    if (!module) return false;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comInitialized = SUCCEEDED(com);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) return false;
    if (FAILED(XAudio2Create(&engine, 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
        FAILED(engine->CreateMasteringVoice(&master, 2, 48000, 0, nullptr, nullptr, AudioCategory_GameEffects))) {
        lis::Log("event=audio_init status=engine_failed");
        Shutdown();
        return false;
    }
    // Extra attenuation independent of file amplitude and requested gain.
    master->SetVolume(0.20f);
    wchar_t path[32768]{};
    const DWORD count = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
    bool any = false;
    for (size_t i = 0; i < sounds.size(); ++i) {
        bool loaded = false;
        try {
            if (count && count < std::size(path)) {
                const auto directory = std::filesystem::path(path).parent_path() / L"LISAccess" / L"audio";
                loaded = Load(directory / names[i], sounds[i]);
            }
        } catch (...) {
            lis::Log("event=audio_load status=load_exception");
        }
        if (!loaded && i >= detail::NavigationCount) {
            try { loaded = DefaultRewind(i); }
            catch (...) { lis::Log("event=audio_load status=default_exception"); }
        }
        any |= loaded;
        lis::Log(std::string("event=audio_load cue=") + labels[i] + (loaded ? " status=ready" : " status=missing_or_invalid"));
    }
    StopAll();
    if (!any) Shutdown();
    return any;
}
bool Available(Cue kind) {
    const size_t index = static_cast<size_t>(kind);
    return index < sounds.size() && sounds[index].voice;
}
bool Play(Cue kind, float pan, float gain, float rate) {
    const size_t index = static_cast<size_t>(kind);
    if (!gate.Allows(index) || !std::isfinite(pan) ||
        !std::isfinite(gain) || !std::isfinite(rate) || gain <= 0) return false;
    auto& sound = sounds[index];
    if (!sound.voice) return false;
    XAUDIO2_VOICE_STATE state{};
    if (kind == Cue::RewindCrossed) {
        // Synchronize cancellation so no approach buffer survives the crossing.
        engine->StopEngine();
        Flush(static_cast<size_t>(Cue::RewindApproach), static_cast<size_t>(Cue::RewindCrossed));
        if (FAILED(engine->StartEngine())) { StopAll(); return false; }
    } else if (kind == Cue::RewindApproach) {
        auto* crossed = sounds[static_cast<size_t>(Cue::RewindCrossed)].voice;
        if (crossed) {
            crossed->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued) return false;
        }
    }
    sound.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    if (state.BuffersQueued) return false; // One dedicated source per kind, no backlog.
    pan = std::clamp(pan, -1.0f, 1.0f);
    gain = std::clamp(gain, 0.0f, 1.0f);
    rate = std::clamp(rate, 0.5f, 2.0f);
    const float matrix[] = {std::sqrt((1.0f - pan) * 0.5f), std::sqrt((1.0f + pan) * 0.5f)};
    if (FAILED(sound.voice->SetOutputMatrix(master, 1, 2, matrix)) ||
        FAILED(sound.voice->SetVolume(gain)) || FAILED(sound.voice->SetFrequencyRatio(rate))) return false;
    XAUDIO2_BUFFER buffer{};
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = static_cast<UINT32>(sound.data.size());
    buffer.pAudioData = sound.data.data();
    if (FAILED(sound.voice->SubmitSourceBuffer(&buffer))) return false;
    if (FAILED(sound.voice->Start(0))) {
        sound.voice->FlushSourceBuffers();
        return false;
    }
    return true;
}
}
