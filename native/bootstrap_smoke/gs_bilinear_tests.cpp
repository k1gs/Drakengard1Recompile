#include "gs_bilinear.h"
#include <array>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {
// Independent channel oracle follows the pre-optimization filter, including
// the host lround and clamp. All textures/colors here are synthetic.
uint32_t reference(uint32_t a, uint32_t b, uint32_t c, uint32_t d, float x, float y) {
    uint32_t result = 0;
    for (unsigned shift : {0u, 8u, 16u, 24u}) {
        float top = float(uint8_t(a >> shift)) +
            (float(uint8_t(b >> shift)) - float(uint8_t(a >> shift))) * x;
        float bottom = float(uint8_t(c >> shift)) +
            (float(uint8_t(d >> shift)) - float(uint8_t(c >> shift))) * x;
        int value = int(std::lround(top + (bottom - top) * y));
        if (value < 0) value = 0;
        if (value > 255) value = 255;
        result |= uint32_t(value) << shift;
    }
    return result;
}
uint32_t randomState = 0x731ac429u;
uint32_t next() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}
bool check(uint32_t a, uint32_t b, uint32_t c, uint32_t d, float x, float y) {
    const auto expected = reference(a, b, c, d, x, y);
    const auto actual = drakengard::filterGsBilinearRgba(a, b, c, d, x, y);
    if (actual == expected) return true;
    std::cerr << "Bilinear mismatch: " << std::hex << expected << " != " << actual
              << std::dec << " weights=" << x << ',' << y << '\n';
    return false;
}
}
int main() {
    uint64_t checked = 0;
    // Exact ties and adjacent floats, including the classic +0.5 rounding trap.
    for (float x : {0.0f, std::nextafter(0.5f, 0.0f), 0.5f,
                    std::nextafter(0.5f, 1.0f), std::nextafter(1.0f, 0.0f), 1.0f})
        for (float y : {0.0f, 0.5f, 1.0f}) {
            if (!check(0u, 0x01010101u, 0u, 0x01010101u, x, y)) return 1;
            ++checked;
        }
    if (drakengard::filterGsBilinearRgba(0u, 0x01010101u, 0u, 0x01010101u,
                                       std::nextafter(0.5f, 0.0f), 0.0f) != 0u) return 2;
    // Every pair of byte values, both rows and all 1/16 sprite fractions.
    for (uint32_t a = 0; a != 256; ++a)
        for (uint32_t b = 0; b != 256; ++b)
            for (unsigned fraction = 0; fraction != 16; ++fraction) {
                const uint32_t c00 = a | (b << 8) | (a << 16) | ((255u - b) << 24);
                const uint32_t c10 = b | (a << 8) | ((255u - a) << 16) | (a << 24);
                if (!check(c00, c10, ~c10, ~c00, fraction / 16.0f, (15u - fraction) / 16.0f)) return 3;
                ++checked;
            }
    // Arbitrary triangle fractions plus axis/endpoints under host rounding modes.
    const int originalRounding = std::fegetround();
    for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) != 0) return 4;
        for (unsigned i = 0; i != 250000; ++i) {
            const auto a = next(), b = next(), c = next(), d = next();
            const float x = float(next() & 0xffffffu) / 16777216.0f;
            const float y = float(next() & 0xffffffu) / 16777216.0f;
            if (!check(a, b, c, d, x, y)) return 5;
            ++checked;
        }
    }
    std::fesetround(originalRounding);
    std::cout << "Exact synthetic RGBA bilinear comparisons: " << checked << '\n';
}
