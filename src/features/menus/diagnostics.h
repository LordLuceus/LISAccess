#pragma once
#include <string>
namespace lis::menus {
// Worker file I/O and game-thread execution communicate through the state lock.
void Files(const std::wstring& localDirectory);
void InspectRequest();
}
