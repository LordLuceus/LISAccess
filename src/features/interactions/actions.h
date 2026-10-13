#pragma once
#include <string>
namespace lis::world {
// Revalidates native focus/actions on the bound game thread; empty means delivered.
std::wstring ActivateFocused(const std::string& expectedFocus,unsigned slot);
}
