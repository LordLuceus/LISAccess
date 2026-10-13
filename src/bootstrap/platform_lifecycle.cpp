#include "bootstrap/platform_lifecycle.h"
#include "platform/log.h"
#include "output/speech.h"

namespace lis {
namespace {
bool initialized = false;
DWORD ownerThread = 0;
}
void InitializePlatform(HMODULE module) {
    if (initialized) return;
    initialized = true;
    ownerThread = GetCurrentThreadId();
    OpenLog();
    Log("event=platform_initialize arch=x86");
    InitializeSpeech(module);
}
void ShutdownPlatform() {
    if (!initialized || ownerThread != GetCurrentThreadId()) return;
    ShutdownSpeech();
    Log("event=platform_shutdown");
    CloseLog();
    initialized = false;
    ownerThread = 0;
}
}
