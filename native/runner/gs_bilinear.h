#pragma once
#include <cmath>
#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define DRAKENGARD_GS_BILINEAR_SSE2 1
#endif

namespace drakengard {
// Keep the scalar operation order and lround's half-away-from-zero rule.
inline uint32_t filterGsBilinearScalar(uint32_t c00, uint32_t c10, uint32_t c01,
                                     uint32_t c11, float fx, float fy) {
    uint32_t result = 0;
    for (unsigned shift = 0; shift != 32; shift += 8) {
        const float a = float((c00 >> shift) & 255u), b = float((c10 >> shift) & 255u);
        const float c = float((c01 >> shift) & 255u), d = float((c11 >> shift) & 255u);
        const float top = a + (b - a) * fx;
        const float bottom = c + (d - c) * fx;
        const long rounded = std::lround(top + (bottom - top) * fy);
        const auto channel = uint32_t(rounded < 0 ? 0 : rounded > 255 ? 255 : rounded);
        result |= channel << shift;
    }
    return result;
}

// SampleTexture supplies finite fractional coordinates in [0,1]. No texture
// reads, wrapping, CLUT state or filtering weights are changed here. Build with
// precise floating-point arithmetic: reassociation/FMA would change edge pixels.
inline uint32_t filterGsBilinearRgba(uint32_t c00, uint32_t c10, uint32_t c01,
                                   uint32_t c11, float fx, float fy) {
#if defined(DRAKENGARD_GS_BILINEAR_SSE2)
    const auto unpack = [](uint32_t rgba) {
        const __m128i bytes = _mm_cvtsi32_si128(static_cast<int>(rgba));
        const __m128i words = _mm_unpacklo_epi8(bytes, _mm_setzero_si128());
        return _mm_cvtepi32_ps(_mm_unpacklo_epi16(words, _mm_setzero_si128()));
    };
    const __m128 a = unpack(c00), b = unpack(c10), c = unpack(c01), d = unpack(c11);
    const __m128 wx = _mm_set1_ps(fx), wy = _mm_set1_ps(fy);
    const __m128 top = _mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), wx));
    const __m128 bottom = _mm_add_ps(c, _mm_mul_ps(_mm_sub_ps(d, c), wx));
    const __m128 value = _mm_add_ps(top, _mm_mul_ps(_mm_sub_ps(bottom, top), wy));
    // Adding 0.5 before truncation incorrectly rounds the float just below 0.5
    // up to 1. Compare the exact fractional part instead. Values are nonnegative.
    const __m128i whole = _mm_cvttps_epi32(value);
    const __m128 fraction = _mm_sub_ps(value, _mm_cvtepi32_ps(whole));
    const __m128i increment = _mm_and_si128(_mm_castps_si128(_mm_cmpge_ps(fraction, _mm_set1_ps(0.5f))),
                                          _mm_set1_epi32(1));
    const __m128i rounded = _mm_add_epi32(whole, increment);
    const __m128i words = _mm_packs_epi32(rounded, _mm_setzero_si128());
    return static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_packus_epi16(words, _mm_setzero_si128())));
#else
    return filterGsBilinearScalar(c00, c10, c01, c11, fx, fy);
#endif
}
}
