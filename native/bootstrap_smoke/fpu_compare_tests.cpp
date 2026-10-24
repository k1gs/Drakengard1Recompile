#include "ee_fpu_compare.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
constexpr float operand(uint32_t bits) { return std::bit_cast<float>(bits); }
void require(bool result,const char* message) { if (!result) throw std::runtime_error(message); }
void comparisons() {
    struct Case { uint32_t left,right; bool equal,less,lessEqual; };
    constexpr std::array cases{
        Case{0x3f800000,0x40000000,false,true,true}, // ordinary positive values
        Case{0xc0000000,0xbf800000,false,true,true}, // reversed negative magnitudes
        Case{0xbf800000,0x3f800000,false,true,true},
        Case{0x40000000,0x3f800000,false,false,false},
        Case{0x80000000,0x00000000,true,false,true}, // signed zeros
        Case{0x007fffff,0x80000001,true,false,true}, // denormals compare as zero
        Case{0x00800000,0x007fffff,false,false,false}, // first normal remains nonzero
        Case{0x80800000,0x80000001,false,true,true},
        Case{0x3f800000,0x7fffffff,false,true,true}, // real PS2 max sentinel, not host NaN
        Case{0xffffffff,0xbf800000,false,true,true}, // negative max sentinel
        Case{0x7fffffff,0x7f800000,true,false,true}, // all exponent255 patterns clamp
        Case{0xff800001,0xffc00000,true,false,true},
        Case{0x7f7fffff,0x7fffffff,true,false,true}, // pinned clamp boundary
        Case{0xff7fffff,0xffffffff,true,false,true},
        Case{0x7f7ffffe,0x7fffffff,false,true,true},
        Case{0xff7ffffe,0xffffffff,false,false,false},
        Case{0x3f800000,0x3f800001,false,true,true} // no incidental low-bit truncation
    };
    for (const auto& item:cases) {
        const auto left=operand(item.left),right=operand(item.right);
        require(drakengard::eeFpuEqual(left,right)==item.equal,"C.EQ domain mismatch");
        require(drakengard::eeFpuLess(left,right)==item.less,"C.LT domain mismatch");
        require(drakengard::eeFpuLessEqual(left,right)==item.lessEqual,"C.LE domain mismatch");
        require(drakengard::eeFpuEqual(right,left)==item.equal,"C.EQ is not symmetric");
        require(drakengard::eeFpuLess(right,left)==(!item.equal && !item.less),"reverse ordering mismatch");
    }
}
void nearestCandidate() {
    // Synthetic consumer: IEEE comparison with this sentinel selects nothing,
    // although both real finite candidates are within the PS2 comparison range.
    struct Candidate { unsigned id; float distance; };
    constexpr std::array input{Candidate{17,9.0f},Candidate{23,2.0f},Candidate{41,5.0f}};
    auto best=operand(0x7fffffffu); unsigned selected{};
    for (const auto& candidate:input)
        if (drakengard::eeFpuLess(candidate.distance,best)) {
            selected=candidate.id; best=candidate.distance;
        }
    require(selected==23 && std::bit_cast<uint32_t>(best)==0x40000000u,"PS2 max-sentinel consumer selected no/wrong candidate");
}
}
int main() {
    try { comparisons(); nearestCandidate(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "EE scalar comparison domain passed\n";
}
