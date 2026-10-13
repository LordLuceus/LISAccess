#pragma once
#include <Windows.h>
#include <string>
namespace lis {
// All speech lifecycle and output calls belong to the runtime worker thread.
void InitializeSpeech(HMODULE module);
void Speak(const std::wstring& text, bool interrupt = true);
void ShutdownSpeech();
}
