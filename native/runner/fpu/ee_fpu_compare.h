#pragma once
#include <bit>
#include <cstdint>

namespace drakengard {
// Functional comparison domain pinned to PCSX2 v2.8.2 COP1 C_EQ/C_LT/C_LE:
// exponent-zero operands compare as signed zero; exponent-255 operands clamp
// to signed host-finite maximum. This is not a PS2 arithmetic/rounding model.
// Integer keys avoid host NaN handling and ambient DAZ/FTZ changing branches.
constexpr uint32_t eeFpuCompareKey(float value) {
    const auto bits=std::bit_cast<uint32_t>(value);
    const auto magnitude=bits&0x7fffffffu;
    if (magnitude<0x00800000u) return 0x80000000u;
    const auto bounded=magnitude>0x7f7fffffu?0x7f7fffffu:magnitude;
    return (bits&0x80000000u)?~(0x80000000u|bounded):(0x80000000u|bounded);
}
constexpr bool eeFpuEqual(float left,float right) { return eeFpuCompareKey(left)==eeFpuCompareKey(right); }
constexpr bool eeFpuLess(float left,float right) { return eeFpuCompareKey(left)<eeFpuCompareKey(right); }
constexpr bool eeFpuLessEqual(float left,float right) { return eeFpuCompareKey(left)<=eeFpuCompareKey(right); }
}
