#pragma once
#include <string>
namespace lis {
std::string Utf8(const std::wstring& value);
std::wstring Wide(const std::string& value);
}
