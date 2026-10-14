#pragma once
#include <Windows.h>

namespace lis::audio {
enum class Cue { Obstacle, Person, Object, Destination, RewindApproach, RewindCrossed };
// All calls must be serialized on the runtime worker, never DllMain.
// Loads <module directory>/LISAccess/audio/{obstacle,person,object,destination}.wav.
// Also prepares rewind defaults (independent of navigation WAV availability).
// Returns true if the engine and at least one cue are available. Both groups start disabled.
bool Initialize(HMODULE module);
void Shutdown();
// Runtime worker only. [0,1] full linear volume range; nonfinite values rejected.
// Initialize resets the conservative default to 0.20. Returns false without engine.
bool SetMasterGain(float gain);
bool Available(Cue kind);
// Call false immediately on focus loss, menus, unknown state or navigation disabled.
// Stops and flushes navigation voices only; rewind has independent gating.
void SetEnabled(bool enabled);
// Rewind defaults are generated; optional rewind_approach.wav/rewind_crossed.wav
// in the same directory replace them only when valid. Starts disabled.
void SetRewindEnabled(bool enabled);
// Disables and flushes both groups.
void StopAll();
// Nonblocking playback; no queue. Busy cues are dropped. Crossing cancels approach;
// approach is suppressed while crossing plays.
// Equal-power stereo left/right only, NOT full 3D. Per-cue gain clamped to [0,1].
// Rate is clamped to [0.5, 2]; invalid/nonfinite inputs are rejected.
bool Play(Cue kind, float pan, float gain, float rate = 1.0f);
}
