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
// Explicit receivers for other verified GFx movies and their clip wrappers.
bool ValidMovie(uintptr_t movie);
bool ObjectQueriesReady();
std::wstring ObjectString(uintptr_t object,const std::string& member);
std::wstring String(uintptr_t movie,const std::string& path);
float Number(uintptr_t movie,const std::string& path);
// Typed conversion for composition visibility properties; no reliance on GFx
// GetVariableString coercing booleans or numbers.
std::wstring DisplayString(const std::string& path);
std::wstring DisplayString(uintptr_t movie,const std::string& path);
}
