#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace drakengard {
// Planar Y16x16/Cb8x8/Cr8x8 -> row-major RGBA8. IPU fixed-point BT.601
// coefficients use separate signed floor shifts before final rounding/clipping.
// C++20 signed right shifts round toward negative infinity.
inline void convertIpuCscRgb32(std::span<const uint8_t,384> input,
        std::span<uint8_t,1024> output,const std::array<uint16_t,2>& thresholds) {
    for (unsigned chroma=0;chroma<64;++chroma) {
        const int cb=int(input[256+chroma])-128,cr=int(input[320+chroma])-128;
        const int red=(204*cr)>>6;
        const int green=((-104*cr)>>6)+((-50*cb)>>6);
        const int blue=(258*cb)>>6;
        const unsigned origin=(chroma/8)*32+(chroma%8)*2;
        for (unsigned dy=0;dy<2;++dy) for (unsigned dx=0;dx<2;++dx) {
            const unsigned pixel=origin+dy*16+dx;
            const int luma=(149*std::max(0,int(input[pixel])-16))>>6;
            uint8_t r=uint8_t(std::clamp((luma+red+1)>>1,0,255));
            uint8_t g=uint8_t(std::clamp((luma+green+1)>>1,0,255));
            uint8_t b=uint8_t(std::clamp((luma+blue+1)>>1,0,255));
            uint8_t alpha=0x80;
            if (r<thresholds[0] && g<thresholds[0] && b<thresholds[0]) {
                r=g=b=alpha=0;
            } else if (r<thresholds[1] && g<thresholds[1] && b<thresholds[1]) alpha=0x40;
            output[pixel*4]=r; output[pixel*4+1]=g;
            output[pixel*4+2]=b; output[pixel*4+3]=alpha;
        }
    }
}
}
