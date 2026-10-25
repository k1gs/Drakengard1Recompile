#include "gs_ppm.h"
#include <iostream>
#include <sstream>
#include <vector>

int main() {
    // Exercise alpha removal, binary bytes, and both sides of the chunk boundary.
    for (const uint32_t width : {1u, 3u, 4095u, 4096u, 4097u}) {
        const uint32_t height = 3;
        std::vector<uint8_t> rgba(size_t(width) * height * 4);
        for (size_t at = 0; at < rgba.size(); ++at)
            rgba[at] = static_cast<uint8_t>((at * 73 + at / 7) & 255);
        std::ostringstream expected, actual;
        expected << "P6\n" << width << ' ' << height << "\n255\n";
        for (size_t at = 0; at < rgba.size(); at += 4)
            expected.write(reinterpret_cast<const char*>(rgba.data() + at), 3);
        writeGsPpm(actual, width, height, rgba);
        if (!actual || actual.str() != expected.str()) {
            std::cerr << "PPM bytes differ at width " << width << '\n';
            return 1;
        }
    }
    std::ostringstream failed;
    failed.setstate(std::ios::badbit);
    const std::array<uint8_t, 4> pixel{0, 255, 17, 42};
    writeGsPpm(failed, 1, 1, pixel);
    if (failed || !failed.str().empty()) return 2;
    std::cout << "PPM binary compatibility passed\n";
}
