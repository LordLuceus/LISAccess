#pragma once
#include <cmath>
#include <cstdint>

namespace lis::engine::gfx {
// Reflected GFxUI.ASValue: Type ByteProperty@0 size1; B BoolProperty@4
// mask1; N FloatProperty@8; S StrProperty@12 size12. Native by-value copies
// need not zero padding or unused Boolean bits. Never decode them as values.
template<class String>
struct ScriptValue {
    std::uint8_t type=0;
    std::uint8_t padding[3]{};
    std::uint32_t boolean=0;
    float number=0;
    String text{};
};
enum class KeyResult { Unhandled, Handled, Failed };
// UE3 ASValue tags. A missing AS2 method returns undefined, which the native
// DNEFocusUtils dispatcher treats as false, not as a bridge failure.
inline KeyResult KeyReturn(unsigned type,unsigned boolean,float number,bool hasText) {
    switch(type) {
    case 0: case 1: return KeyResult::Unhandled; // undefined / null
    case 2: return number!=0&&!std::isnan(number)?KeyResult::Handled:KeyResult::Unhandled;
    case 3: return hasText?KeyResult::Handled:KeyResult::Unhandled;
    case 4: return (boolean&1)?KeyResult::Handled:KeyResult::Unhandled;
    default: return KeyResult::Failed;
    }
}
}
