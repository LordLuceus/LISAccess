#pragma once
#include <string>
namespace lis::world {
struct Snapshot;
std::wstring FaceTarget(const std::string& expectedTarget,bool& submitted);
std::wstring TickFacing(const Snapshot& fresh,std::string& completed);
void CancelFacing();
bool FacingActive();
}
