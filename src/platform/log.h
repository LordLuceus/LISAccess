#pragma once
#include <string>
namespace lis {
void OpenLog();
void CloseLog();
// Thread-safe UTF-8 event fields; speech is not automatically logged.
void Log(const std::string& fields);
}
