#pragma once
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

// INT+FFMD scans a half-height framebuffer into an interlaced display.
// PCSX2 v2.8.2 GSState::GSPCRTCRegs::SetRects rounds odd read heights up.
inline uint32_t gsScanoutReadHeight(uint32_t height,uint64_t smode2) {
    return (smode2&3u)==3u ? height/2u+height%2u : height;
}

inline void expandGsFrameScanout(std::span<uint8_t> pixels,uint32_t stride,
                                uint32_t height,uint64_t smode2) {
    if ((smode2&3u)!=3u) return;
    if (!stride || uint64_t(stride)*height>pixels.size())
        throw std::runtime_error("GS scanout exceeds presentation storage");
    // Bottom-up permits expansion in place and never reads the other half of VRAM.
    for (uint32_t y=height;y-->0;) {
        const uint32_t source=y/2u;
        if (source!=y) std::memcpy(pixels.data()+size_t(y)*stride,
                                 pixels.data()+size_t(source)*stride,stride);
    }
}
