#include "engine/build_verification.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>

namespace lis {
bool VerifyBuild() {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, ARRAYSIZE(path))) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<unsigned char, 32> digest{};
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0;
    if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0;
    std::array<unsigned char, 65536> buffer{}; DWORD count = 0;
    while (ok) {
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        ok = BCryptHashData(hash, buffer.data(), count, 0) == 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    const unsigned char expected[32] = {0x09,0x73,0xa7,0xe1,0x93,0x72,0x80,0x88,0xed,0x43,0x8c,0x02,0xdd,0x1f,0x99,0xb4,0x90,0xeb,0x35,0x35,0x5a,0xd8,0xbe,0x0f,0x22,0x40,0xa3,0xd2,0x69,0x0f,0xa2,0x33};
    return ok && std::memcmp(digest.data(), expected, 32) == 0;
}
}
