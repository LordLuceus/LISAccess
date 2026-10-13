#pragma once
#include <Windows.h>
#include <string>
#include <cstdint>
namespace lis {
// All speech lifecycle and output calls belong to the runtime worker thread.
void InitializeSpeech(HMODULE module);
void Speak(const std::wstring& text, bool interrupt = true);
uint64_t SpeechSerial();
// Cancel an obsolete readout only if no newer output has replaced/queued after it.
void StopSpeechIfCurrent(uint64_t serial);
void ShutdownSpeech();
// Diagnostic readback on the owning output thread; no game state is queried.
const char* SpeechBackendName();
}
