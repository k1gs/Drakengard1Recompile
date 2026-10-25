#include "ee_fpu_convert.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool value) { if (!value) throw std::runtime_error("CVT.W.S regression"); }
int main() {
    struct Case { uint32_t bits; int32_t expected; };
    constexpr Case cases[] = {
        {0,0},{0x80000000u,0},{1,0},{0x807fffffu,0},
        {0x3fc00000u,1},{0xbfc00000u,-1},{0x3fffffffu,1},
        {0x4effffffu,2147483520},{0xceffffffu,-2147483520},
        {0x4f000000u,INT32_MAX},{0xcf000000u,INT32_MIN},
        {0x7f800000u,INT32_MAX},{0xff800000u,INT32_MIN},
        {0x7fffffffu,INT32_MAX},{0xffffffffu,INT32_MIN}
    };
    const int original = std::fegetround();
    for (int mode : {FE_TONEAREST,FE_TOWARDZERO,FE_DOWNWARD,FE_UPWARD}) {
        require(std::fesetround(mode)==0);
        for (auto c : cases) require(drakengard::eeFpuConvertWord(std::bit_cast<float>(c.bits))==c.expected);
    }
    require(std::fesetround(FE_TONEAREST)==0);
    volatile float smallGap = 2.0f / 1.3f;
    require(std::nearbyint(smallGap)==2.0f); // fail-before: gap never reaches one
    require(drakengard::eeFpuConvertWord(smallGap)==1);
    // A bounded original synthetic comb sort exercises conversion in control flow.
    std::array<float,51> keys;
    for (size_t n=0;n<keys.size();++n) keys[n]=float((n*37)%keys.size());
    int gap=int(keys.size()); bool swapped=true; unsigned passes=0;
    while (gap>1 || swapped) {
        require(++passes<1000);
        gap=(std::max)(1,drakengard::eeFpuConvertWord(float(gap)/1.3f));
        if (gap==9 || gap==10) gap=11;
        swapped=false;
        for (size_t n=0;n+gap<keys.size();++n)
            if (keys[n]>keys[n+gap]) { std::swap(keys[n],keys[n+gap]); swapped=true; }
    }
    require(std::is_sorted(keys.begin(),keys.end()));
    require(std::fesetround(original)==0);
    std::cout << "CVT.W.S truncation, saturation and bounded sort passed\n";
}
