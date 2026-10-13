#include "platform/foreground.h"
#include <Windows.h>

namespace lis {
bool IsGameForeground() {
    const HWND foreground = GetForegroundWindow();
    DWORD process = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &process);
    return process != 0 && process == GetCurrentProcessId();
}
}
