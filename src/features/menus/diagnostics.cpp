#include "features/menus/diagnostics.h"
#include "features/menus/state.h"
#include "engine/gfx.h"
#include "platform/text.h"
#include <Windows.h>
#include <fstream>
#include <sstream>

namespace lis::menus {
namespace {
std::string pendingRequest, completedResponse;
bool ValidPath(const std::string& path) {
    return path.size() < 256 && path.rfind("_root.", 0) == 0 &&
        path.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.$") == std::string::npos;
}
}
void InspectRequest() {
    std::string command;
    { std::lock_guard<std::mutex> lock(stateMutex); command.swap(pendingRequest); }
    if (command.empty()) return;
    std::istringstream in(command); std::string kind, path; in >> kind >> path;
    std::string result = "error invalid read-only request";
    if (ValidPath(path)) {
        if (kind == "string") result = "ok " + Utf8(engine::gfx::String(path));
        else if (kind == "number") result = "ok " + std::to_string(engine::gfx::Number(path));
        else if (kind == "text") result = "ok " + Utf8(engine::gfx::Text(path));
    }
    { std::lock_guard<std::mutex> lock(stateMutex); completedResponse = result; }
}
void Files(const std::wstring& localDirectory) {
    const auto requestPath = localDirectory + L"\\request.txt";
    std::ifstream input(requestPath, std::ios::binary);
    if (input) {
        char bytes[513]{}; input.read(bytes, 512); const auto n = input.gcount(); input.close();
        DeleteFileW(requestPath.c_str());
        std::lock_guard<std::mutex> lock(stateMutex); pendingRequest.assign(bytes, static_cast<size_t>(n));
    }
    std::string response;
    { std::lock_guard<std::mutex> lock(stateMutex); response.swap(completedResponse); }
    if (!response.empty()) {
        const auto temp = localDirectory + L"\\response.tmp";
        std::ofstream output(temp, std::ios::binary); output << response; output.close();
        MoveFileExW(temp.c_str(), (localDirectory + L"\\response.txt").c_str(), MOVEFILE_REPLACE_EXISTING);
    }
}
}
