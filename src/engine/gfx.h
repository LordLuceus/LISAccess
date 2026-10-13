#pragma once
#include <cstdint>
#include <string>

namespace lis::engine::gfx {
// Discover runs during worker startup, then only on the game thread.
void Initialize(uintptr_t executableBase);
void Discover();
bool Bound();
bool RegisteredMenu();
unsigned char Opening();
std::string DiscoveryState();
// Native queries must run on the game thread inside the caller's SEH boundary.
std::wstring String(const std::string& path);
float Number(const std::string& path);
std::wstring Text(const std::string& objectPath);
}
