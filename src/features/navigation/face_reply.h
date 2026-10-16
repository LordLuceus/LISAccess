#pragma once
#include <cstdint>
namespace lis::navigation::detail {
// Worker-owned generation. A late result from the game thread must not complete
// or narrate an older request, even when the new request selects the same actor.
struct FaceReplies {
    uint64_t generation=0;
    uint64_t Advance(){return ++generation;}
    bool Current(uint64_t token) const {return token!=0&&token==generation;}
};
}
