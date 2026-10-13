#pragma once
#include <mutex>

namespace lis::menus {
// Shared by menu speech and diagnostic mailboxes to preserve lock ordering.
extern std::mutex stateMutex;
}
