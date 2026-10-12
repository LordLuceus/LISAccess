#pragma once
#include <Windows.h>
#include <string>

namespace lis {
// Initialize, Speak and Shutdown must all run on the runtime worker thread.
// Log is thread-safe; messages are UTF-8 key=value fields, not captured text.
void InitializePlatform(HMODULE module);
void Log(const std::string& fields);
void Speak(const std::wstring& text, bool interrupt = true);
void ShutdownPlatform();
bool IsGameForeground();
void RunRuntime(HMODULE module);
}
