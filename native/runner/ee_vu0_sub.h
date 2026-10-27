#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <immintrin.h>
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace drakengard {
// Preserve evaluation of a generated VF0 assignment, including memory reads,
// with an observable host sink. There is no guest register/state mutation.
inline void discardVu0Write(__m128 value) {
    alignas(16) uint32_t bits[4];
    _mm_store_si128(reinterpret_cast<__m128i*>(bits),_mm_castps_si128(value));
    [[maybe_unused]] volatile uint32_t discarded[4];
    for (unsigned n=0;n<4;++n) discarded[n]=bits[n];
}
// Functional macro-VU SUB/SUBbc for normal/zero finite operands and results.
// This does not model FMAC latency. Use only after auditing flag-read spacing.
// Reference contracts: PCSX2 pcsx2/VUops.cpp (_getDst, applyBinaryMACOp),
// pcsx2/VUflags.cpp (VU_MAC_UPDATE, VU_STAT_UPDATE), read 2026-10-27:
// https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/VUops.cpp
// https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/VUflags.cpp
// VF0 discards writes while arithmetic still updates flags. MAC lane bits are
// x=8,y=4,z=2,w=1; inactive lanes clear. Signed zero sets both S and Z.
// Denormal/extended, underflow and overflow cases fail before any mutation;
// they require a separate reviewed normalization contract, not host NaNs.
template<class Context>
inline void eeVu0Sub(Context* ctx,unsigned fd,unsigned destMask,__m128 fs,__m128 ft) {
    if (fd>=32u || destMask>15u)
        throw std::runtime_error("Unsupported macro-VU SUB register/mask");
    alignas(16) float left[4],right[4],result[4],destination[4];
    _mm_store_ps(left,fs); _mm_store_ps(right,ft);
    _mm_store_ps(destination,ctx->vu0_vf[fd]);
    uint32_t mac=0;
    for (unsigned component=0;component<4;++component) {
        const uint32_t lane=1u<<(3u-component);
        if (!(destMask&lane)) continue;
        for (float value:{left[component],right[component]}) {
            const uint32_t bits=std::bit_cast<uint32_t>(value),exponent=bits&0x7f800000u;
            if (exponent==0x7f800000u || (!exponent && (bits&0x7fffffffu)))
                throw std::runtime_error("Unsupported macro-VU SUB operand normalization");
        }
        const double exact=double(left[component])-double(right[component]);
        const double magnitude=std::fabs(exact);
        if (magnitude>double(std::numeric_limits<float>::max()) ||
            (magnitude && magnitude<double(std::numeric_limits<float>::min())))
            throw std::runtime_error("Unsupported macro-VU SUB result normalization");
        // SSE subtraction retains the existing macro path's host rounding and
        // signed-zero behavior; the checks above exclude exceptional formats.
        result[component]=left[component]-right[component];
        const uint32_t bits=std::bit_cast<uint32_t>(result[component]);
        if (bits&0x80000000u) mac|=lane<<4;
        if (!(bits&0x7fffffffu)) mac|=lane;
        destination[component]=result[component];
    }
    const uint32_t current=(mac&15u?1u:0u)|(mac&0xf0u?2u:0u);
    // Current U/O are zero in this bounded subset; preserve D/I and every
    // sticky flag, then accumulate this operation's Z/S sticky flags.
    ctx->vu0_status=static_cast<uint16_t>((ctx->vu0_status&0xff0u)|current|(current<<6));
    ctx->vu0_mac_flags=mac;
    if (fd) ctx->vu0_vf[fd]=_mm_load_ps(destination);
}
}
