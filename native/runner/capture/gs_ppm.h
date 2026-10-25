#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>

// Serialize actual RGBA pixels as P6 RGB without per-pixel stream calls.
// Fixed-size chunks also support final captures wider than a normal GS frame.
inline void writeGsPpm(std::ostream& file, uint32_t width, uint32_t height,
                       std::span<const uint8_t> rgba) {
    file << "P6\n" << width << ' ' << height << "\n255\n";
    std::array<char, 4096 * 3> rgb;
    const size_t pixels = rgba.size() / 4;
    for (size_t first = 0; first < pixels; first += 4096) {
        const size_t count = (std::min)(size_t{4096}, pixels - first);
        for (size_t pixel = 0; pixel < count; ++pixel)
            for (size_t channel = 0; channel < 3; ++channel)
                rgb[pixel * 3 + channel] = static_cast<char>(rgba[(first + pixel) * 4 + channel]);
        file.write(rgb.data(), static_cast<std::streamsize>(count * 3));
        if (!file) return;
    }
}
