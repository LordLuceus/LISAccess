#pragma once
#include "features/world/world.h"
#include "output/audio_cues.h"

namespace lis::navigation::detail {
float Distance(const world::Snapshot& snapshot,const world::Target& target);
float Bearing(const world::Snapshot& snapshot,const world::Target& target);
std::wstring Describe(const world::Snapshot& snapshot,const world::Target& target);
std::wstring ActionText(const world::Snapshot& snapshot);
std::wstring FocusText(const world::Snapshot& snapshot);
audio::Cue CueFor(world::Kind kind);
const wchar_t* PreviewName(int index);
int NextPreview(int index);
}
