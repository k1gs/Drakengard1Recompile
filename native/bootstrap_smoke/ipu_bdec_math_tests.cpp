#include "ipu_bdec.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

using drakengard::IpuAcCode;
using drakengard::IpuVlcResult;

namespace {

void check(bool condition)
{
    if (!condition) throw std::runtime_error("ipu_bdec_math assertion failed");
}

uint32_t prefix(unsigned code, unsigned length)
{
    return length == 32 ? code : code << (32u - length);
}

void check_dc_table(bool chroma)
{
    // MPEG-2 ISO/IEC 13818-2 Tables B-12 and B-13, represented as
    // canonical prefixes rather than copied decoder lookup storage.
    static constexpr unsigned lum_code[] = {
        0b100, 0b00, 0b01, 0b101, 0b110, 0b1110,
        0b11110, 0b111110, 0b1111110, 0b11111110,
        0b111111110, 0b111111111};
    static constexpr unsigned lum_len[] = {3,2,2,3,3,4,5,6,7,8,9,9};
    static constexpr unsigned chr_code[] = {
        0b00, 0b01, 0b10, 0b110, 0b1110, 0b11110,
        0b111110, 0b1111110, 0b11111110, 0b111111110,
        0b1111111110, 0b1111111111};
    static constexpr unsigned chr_len[] = {2,2,2,3,4,5,6,7,8,9,10,10};
    const auto* codes = chroma ? chr_code : lum_code;
    const auto* lengths = chroma ? chr_len : lum_len;
    for (unsigned size = 0; size != 12; ++size) {
        const IpuVlcResult got = drakengard::decodeIpuDcSize(
            prefix(codes[size], lengths[size]), chroma);
        check(got.value == size);
        check(got.length == lengths[size]);
    }
}

void check_ac_vectors()
{
    // Selected canonical Table B-15 vectors.  The groups exercise the
    // short, long, EOB, and escape forms without importing the reference
    // implementation's lookup array.
    struct V { unsigned code, length, run, level; bool end, escape; };
    static constexpr V vectors[] = {
        {0b000001,6,0,0,false,true},
        {0b0000100,7,7,1,false,false}, {0b0000101,7,8,1,false,false},
        {0b0000110,7,6,1,false,false}, {0b0000111,7,2,2,false,false},
        {0b000100,6,0,7,false,false}, {0b000101,6,0,6,false,false},
        {0b000110,6,4,1,false,false}, {0b000111,6,5,1,false,false},
        {0b00100000,8,1,5,false,false}, {0b00100001,8,11,1,false,false},
        {0b00100010,8,0,11,false,false}, {0b00100011,8,0,10,false,false},
        {0b00100100,8,13,1,false,false}, {0b00100101,8,12,1,false,false},
        {0b00100110,8,3,2,false,false}, {0b00100111,8,1,4,false,false},
        {0b00101,5,2,1,false,false}, {0b00110,5,1,2,false,false},
        {0b00111,5,3,1,false,false}, {0b010,3,1,1,false,false},
        {0b0110,4,0,0,true,false}, {0b0111,4,0,3,false,false},
        {0b10,2,0,1,false,false}, {0b110,3,0,2,false,false},
        {0b11100,5,0,4,false,false}, {0b11101,5,0,5,false,false},
        {0b1111000,7,9,1,false,false}, {0b1111001,7,1,3,false,false},
        {0b1111010,7,10,1,false,false}, {0b1111011,7,0,8,false,false},
        {0b1111100,7,0,9,false,false}, {0b11111010,8,0,12,false,false},
        {0b11111011,8,0,13,false,false}, {0b11111100,8,2,3,false,false},
        {0b11111101,8,4,2,false,false}, {0b11111110,8,0,14,false,false},
        {0b11111111,8,0,15,false,false},
        {27,13,26,1,false,false}, {28,13,25,1,false,false},
        {29,13,24,1,false,false}, {30,13,23,1,false,false},
        {31,13,22,1,false,false},
    };
    for (const auto& v : vectors) {
        const IpuAcCode got = drakengard::decodeIpuIntraAc(prefix(v.code, v.length));
        check(got.run == v.run);
        check(got.level == v.level);
        check(got.length == v.length);
        check(got.end == v.end);
        check(got.escape == v.escape);
    }
}

std::array<uint8_t,64> oracle(const std::array<int16_t,64>& input)
{
    std::array<uint8_t,64> output{};
    constexpr double pi = 3.141592653589793238462643383279502884;
    for (unsigned y=0; y<8; ++y) for (unsigned x=0; x<8; ++x) {
        double sum = 0.0;
        for (unsigned v=0; v<8; ++v) for (unsigned u=0; u<8; ++u) {
            const double cu = u ? 1.0 : 1.0/std::sqrt(2.0);
            const double cv = v ? 1.0 : 1.0/std::sqrt(2.0);
            sum += cu*cv*double(input[v*8+u]) *
                std::cos((2.0*x+1.0)*u*pi/16.0) *
                std::cos((2.0*y+1.0)*v*pi/16.0);
        }
        const int rounded = static_cast<int>(std::floor(sum/4.0 + 0.5));
        output[y*8+x] = static_cast<uint8_t>(rounded < 0 ? 0 : rounded > 255 ? 255 : rounded);
    }
    return output;
}

void check_idct()
{
    std::array<int16_t,64> dc{};
    for (int value : {-1024, -256, 0, 256, 1024, 4095}) {
        dc.fill(0); dc[0] = static_cast<int16_t>(value);
        const auto got = drakengard::ipuInverseDct(dc);
        const auto want = oracle(dc);
        for (unsigned i=0; i<64; ++i) check(std::abs(int(got[i])-int(want[i])) <= 1);
    }

    std::array<int16_t,64> horizontal{};
    horizontal[1] = 200;
    const auto h = drakengard::ipuInverseDct(horizontal);
    check(h[0] != h[1] && h[0] == h[8]);
    std::array<int16_t,64> vertical{};
    vertical[8] = 200;
    const auto v = drakengard::ipuInverseDct(vertical);
    check(v[0] != v[8] && v[0] == v[1]);

    std::mt19937 rng(0xD3BDEC);
    std::uniform_int_distribution<int> dist(-512,512);
    for (unsigned sample=0; sample<32; ++sample) {
        std::array<int16_t,64> coefficients{};
        for (auto& coefficient : coefficients) coefficient = static_cast<int16_t>(dist(rng));
        const auto got = drakengard::ipuInverseDct(coefficients);
        const auto want = oracle(coefficients);
        for (unsigned i=0; i<64; ++i) check(std::abs(int(got[i])-int(want[i])) <= 1);
    }
}

} // namespace

int main()
{
    check_dc_table(false);
    check_dc_table(true);
    check_ac_vectors();
    check_idct();
    std::cout << "ipu_bdec_math: PASS\n";
}
