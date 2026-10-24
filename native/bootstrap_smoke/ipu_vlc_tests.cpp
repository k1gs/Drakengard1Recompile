#include "../runner/ee_ipu.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

namespace {
using drakengard::EeIpu;
using Quadword = std::array<uint8_t, 16>;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Quadword stream(std::string_view code, uint32_t top) {
    Quadword result{};
    unsigned bit = 0;
    for (const char value : code) {
        if (value == '1') result[bit / 8] |= uint8_t(0x80u >> (bit & 7u));
        ++bit;
    }
    for (unsigned i = 0; i < 32; ++i, ++bit) {
        if (top & (0x80000000u >> i)) result[bit / 8] |= uint8_t(0x80u >> (bit & 7u));
    }
    return result;
}

void push(EeIpu& ipu, const Quadword& value) {
    check(ipu.push(std::span<const uint8_t, 16>(value)), "IPU input FIFO rejected test quadword");
}

struct Vector { std::string_view bits; uint16_t value; };

template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("unsupported IPU VDEC operation was accepted");
}

// ISO/IEC 13818-2 Table B-1, written as independent golden bit strings.
constexpr std::array<Vector, 34> mba = {{
    {"1", 1}, {"011", 2}, {"010", 3}, {"0011", 4}, {"0010", 5},
    {"00011", 6}, {"00010", 7}, {"0000111", 8}, {"0000110", 9},
    {"00001011", 10}, {"00001010", 11}, {"00001001", 12}, {"00001000", 13},
    {"00000111", 14}, {"00000110", 15}, {"0000010111", 16}, {"0000010110", 17},
    {"0000010101", 18}, {"0000010100", 19}, {"0000010011", 20}, {"0000010010", 21},
    {"00000011000", 33}, {"00000011001", 32},
    {"00000011010", 31}, {"00000011011", 30}, {"00000011100", 29}, {"00000011101", 28},
    {"00000011110", 27}, {"00000011111", 26}, {"00000100000", 25}, {"00000100001", 24},
    {"00000100010", 23}, {"00000100011", 22},
    {"00000001000", 35},
}};

void testAllMbaIncrementCodes() {
    constexpr uint32_t top = 0xA1B2C3D4u;
    for (const auto& vector : mba) {
        EeIpu ipu;
        push(ipu, stream(vector.bits, top));
        ipu.write(EeIpu::Command, 0x30000000u);
        check(ipu.read(EeIpu::Command) == (uint32_t(vector.bits.size()) << 16 | vector.value),
              "VDEC MBAI DATA mismatch");
        check(ipu.read(EeIpu::Top) == top, "VDEC MBAI TOP mismatch");
        check(ipu.read(EeIpu::Command) == (uint32_t(vector.bits.size()) << 16 | vector.value),
              "VDEC MBAI DATA advanced on repeated read");
        check((ipu.read(EeIpu::Position) & 0x7fu) == vector.bits.size(),
              "VDEC MBAI BP mismatch");
        check((ipu.read(EeIpu::Top + 4) & 0x80000000u) == 0, "VDEC MBAI TOP stayed busy");
    }
}

void testMpeg1StuffingAndMpeg2Error() {
    constexpr uint32_t top = 0x11223344u;
    {
        EeIpu ipu;
        ipu.write(EeIpu::Control, 0x00800000u); // MP1
        push(ipu, stream("00000001111", top));
        ipu.write(EeIpu::Command, 0x30000000u);
        check(ipu.read(EeIpu::Command) == (11u << 16 | 34u), "MPEG1 stuffing mismatch");
        check((ipu.read(EeIpu::Position) & 0x7fu) == 11u, "MPEG1 stuffing BP mismatch");
    }
    {
        EeIpu ipu;
        push(ipu, stream("00000001111", top));
        ipu.write(EeIpu::Command, 0x30000000u);
        check(ipu.read(EeIpu::Command) == 0, "MPEG2 stuffing DATA must be zero");
        check((ipu.read(EeIpu::Control) & 0x4000u) != 0, "MPEG2 stuffing must set ECD");
        check((ipu.read(EeIpu::Position) & 0x7fu) == 0, "MPEG2 stuffing advanced BP");
        check(ipu.read(EeIpu::Top) == 0x01e22446u, "MPEG2 stuffing TOP must re-read at same BP");
    }
    {
        EeIpu ipu;
        push(ipu, stream("00000000000", top));
        ipu.write(EeIpu::Command, 0x30000000u);
        check(ipu.read(EeIpu::Command) == 0, "invalid MBAI DATA must be zero");
        check((ipu.read(EeIpu::Control) & 0x4000u) != 0, "invalid MBAI must set ECD");
        check((ipu.read(EeIpu::Position) & 0x7fu) == 0, "invalid MBAI advanced BP");
    }
}

void testVdecTablesAndPartialTop() {
    EeIpu ipu;
    push(ipu, stream("1", 0x55667788u));
    for (uint32_t command:{0x38000000u,0x3c000000u})
        refuses([&] { ipu.write(EeIpu::Command,command); });
    ipu.write(EeIpu::Control,0x02000000u);
    refuses([&] { ipu.write(EeIpu::Command,0x34000000u); }); // P-picture MBT not supported.

    EeIpu skipped;
    skipped.write(EeIpu::Command,0x30000008u);
    check(skipped.read(EeIpu::Command+4)==0x80000000u,"VDEC failed to wait for input");
    Quadword skipInput{}; skipInput[1]=0x80;
    push(skipped,skipInput);
    check(skipped.read(EeIpu::Command)==0x10001u && (skipped.read(EeIpu::Position)&0x7f)==9,
          "VDEC consumed FB more than once on retry");

    EeIpu partial;
    partial.write(EeIpu::Command, 0x00000060u); // BCLR, BP=96
    Quadword first{};
    first[12] = 0x80; // MBAI=1 at the last 32-bit word
    push(partial, first);
    partial.write(EeIpu::Command, 0x30000000u);
    check(partial.read(EeIpu::Command) == (1u << 16 | 1u), "VDEC decoded DATA before TOP arrived");
    check(partial.read(EeIpu::Command + 4) == 0x80000000u, "VDEC did not stall on missing TOP");
    check((partial.read(EeIpu::Position) & 0x7fu) == 97u, "VDEC FB/code advance mismatch");
    push(partial, stream("00000000", 0xCAFEBABEu));
    check(partial.read(EeIpu::Top) == 0x00000000u, "partial TOP fixture was not aligned as expected");
    check(partial.read(EeIpu::Command + 4) == 0, "VDEC did not complete after TOP input");
    check(partial.read(EeIpu::Command) == (1u << 16 | 1u), "VDEC decoded DATA changed on retry");
}

void testIntraMacroblockType() {
    for (const auto& vector:std::array<Vector,2>{{{"1",1},{"01",17}}}) {
        EeIpu ipu;
        ipu.write(EeIpu::Control,0x01000000u); // PCT=I
        push(ipu,stream(vector.bits,0x11223344u));
        ipu.write(EeIpu::Command,0x34000000u);
        check(ipu.read(EeIpu::Command)==vector.value,"I-MBT modes result differs");
        check((ipu.read(EeIpu::Position)&0x7f)==vector.bits.size(),"I-MBT consumed bits differ");
        check(ipu.read(EeIpu::Top)==0x11223344u,"I-MBT TOP differs");
    }
    EeIpu invalid;
    push(invalid,stream("00",0x11223344u));
    invalid.write(EeIpu::Command,0x34000000u);
    check(invalid.read(EeIpu::Command)==0 && (invalid.read(EeIpu::Control)&0x4000),"Invalid I-MBT did not signal ECD");
    check((invalid.read(EeIpu::Position)&0x7f)==0,"Invalid I-MBT consumed code bits");
}
}

int main() {
    try {
        testAllMbaIncrementCodes();
        testMpeg1StuffingAndMpeg2Error();
        testVdecTablesAndPartialTop();
        testIntraMacroblockType();
        std::cout << "IPU VDEC MBAI vectors passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
