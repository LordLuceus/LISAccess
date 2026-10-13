#include "platform/log.h"
#include <Windows.h>
#include <shlobj.h>

namespace lis {
namespace {
SRWLOCK logLock = SRWLOCK_INIT;
HANDLE logFile = INVALID_HANDLE_VALUE;
}
void OpenLog() {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) return;
    const std::wstring directory = std::wstring(local) + L"\\LISAccess";
    CoTaskMemFree(local);
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return;
    AcquireSRWLockExclusive(&logLock);
    logFile = CreateFileW((directory + L"\\prototype.log").c_str(), FILE_APPEND_DATA,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    ReleaseSRWLockExclusive(&logLock);
}
void Log(const std::string& fields) {
    // One stable line per event; speech content is never automatically logged.
    std::string line = "[a11y] " + fields;
    for (char& c : line) if (c == '\r' || c == '\n' || c == '\0') c = ' ';
    line += "\r\n";
    AcquireSRWLockExclusive(&logLock);
    if (logFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(logFile, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    }
    ReleaseSRWLockExclusive(&logLock);
}
void CloseLog() {
    AcquireSRWLockExclusive(&logLock);
    if (logFile != INVALID_HANDLE_VALUE) CloseHandle(logFile);
    logFile = INVALID_HANDLE_VALUE;
    ReleaseSRWLockExclusive(&logLock);
}
}
