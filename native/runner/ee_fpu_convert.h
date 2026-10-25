#pragma once
#include <bit>
#include <cstdint>

namespace drakengard {
// Pinned PCSX2 v2.8.2 COP1 CVT_W contract: truncate toward zero, saturating
// unrepresentable inputs by sign. Never depend on the host rounding mode.
inline int32_t eeFpuConvertWord(float value) {
    const uint32_t bits = std::bit_cast<uint32_t>(value);
    if ((bits & 0x7f800000u) <= 0x4e800000u)
        return static_cast<int32_t>(value);
    return (bits & 0x80000000u) ? INT32_MIN : INT32_MAX;
}
}
