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
    refuses([&] { ipu.write(EeIpu::Command,0x3c000000u); });
    ipu.write(EeIpu::Control,0x04000000u);
    refuses([&] { ipu.write(EeIpu::Command,0x34000000u); }); // D-picture remains unsupported.

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

void testPredictiveMacroblockType() {
    constexpr std::array<Vector,7> vectors{{
        {"1",0x8a}, {"01",2}, {"001",0x88}, {"00011",1},
        {"00010",0x9a}, {"00001",18}, {"000001",17},
    }};
    for (const auto& vector:vectors) {
        EeIpu ipu;
        ipu.write(EeIpu::Control,0x02620000u);
        push(ipu,stream(vector.bits,0x91abcdefu));
        ipu.write(EeIpu::Command,0x34000000u);
        check(ipu.read(EeIpu::Command)==vector.value,"P-MBT modes mismatch");
        check((ipu.read(EeIpu::Position)&0x7fu)==vector.bits.size(),"P-MBT consumed extra motion bits");
        check(ipu.read(EeIpu::Top)==0x91abcdefu,"P-MBT TOP mismatch");
        check(!(ipu.read(EeIpu::Control)&0x80004000u),"P-MBT BUSY/ECD remained set");
        check(ipu.takeInterrupt() && !ipu.takeInterrupt(),"P-MBT completion interrupt mismatch");
    }
    EeIpu invalid;
    invalid.write(EeIpu::Control,0x02000000u);
    push(invalid,stream("000000",0xf1234567u));
    invalid.write(EeIpu::Command,0x34000000u);
    check(invalid.read(EeIpu::Command)==0 && (invalid.read(EeIpu::Control)&0x4000u),
          "Malformed P-MBT failed to signal ECD");
    check((invalid.read(EeIpu::Position)&0x7fu)==0,"Malformed P-MBT consumed bits");

    EeIpu partial;
    partial.write(EeIpu::Control,0x02000000u);
    partial.write(EeIpu::Command,0x0000005eu); // BCLR BP=94
    partial.takeInterrupt();
    partial.write(EeIpu::Command,0x34000002u); // FB must advance once while starved
    check(partial.read(EeIpu::Command+4)==0x80000000u,"P-MBT failed to wait for input");
    Quadword first{}; first[12]=0x20; // two FB bits, then 001 at BP=96
    push(partial,first);
    check(partial.read(EeIpu::Command)==0x88,"P-MBT starved DATA mismatch");
    check((partial.read(EeIpu::Position)&0x7fu)==99,"P-MBT repeated FB/code on starvation");
    check(partial.read(EeIpu::Command+4)==0x80000000u && !partial.takeInterrupt(),
          "P-MBT completed before TOP refill");
    Quadword second{}; second[0]=0x80;
    push(partial,second);
    check(partial.read(EeIpu::Top)==4u,"P-MBT partial TOP mismatch");
    check(partial.read(EeIpu::Command)==0x88 && (partial.read(EeIpu::Position)&0x7fu)==99,
          "P-MBT decoded twice after TOP refill");
    check(partial.read(EeIpu::Command+4)==0 && partial.takeInterrupt(),
          "P-MBT failed to complete on TOP refill");
}

void testBidirectionalMacroblockType() {
    constexpr std::array<Vector,11> vectors{{
        {"10",0x8c},{"11",0x8e},{"010",0x84},{"011",0x86},
        {"0010",0x88},{"0011",0x8a},{"00010",0x9e},{"00011",0x81},
        {"000001",0x91},{"000010",0x96},{"000011",0x9a}
    }};
    for (const auto& vector:vectors) {
        EeIpu ipu; ipu.write(EeIpu::Control,0x03620000);
        push(ipu,stream(vector.bits,0x91abcdef)); ipu.write(EeIpu::Command,0x34000000);
        const uint32_t expected=(uint32_t(vector.bits.size())<<16)|vector.value;
        check(ipu.read(EeIpu::Command)==expected,"B-MBT DATA modes/length mismatch");
        check((ipu.read(EeIpu::Position)&127)==vector.bits.size(),"B-MBT consumed extra prediction bits");
        check(ipu.read(EeIpu::Top)==0x91abcdef,"B-MBT TOP mismatch");
        check(ipu.read(EeIpu::Command)==expected,"B-MBT DATA changed on repeated read");
        check(!(ipu.read(EeIpu::Control)&0x80004000),"B-MBT BUSY/ECD remained set");
        check(ipu.takeInterrupt() && !ipu.takeInterrupt(),"B-MBT completion edge mismatch");
    }
    // Exhaust all 64 lookahead prefixes against independent canonical strings.
    for (unsigned prefix=0;prefix<64;++prefix) {
        std::string bits;
        for (unsigned i=0;i<6;++i) bits+=((prefix>>(5-i))&1)?'1':'0';
        drakengard::IpuVlcResult expected;
        for (const auto& vector:vectors) if (bits.starts_with(vector.bits))
            expected={vector.value,unsigned(vector.bits.size())};
        const auto actual=drakengard::decodeIpuBidirectionalMacroblockType(prefix<<26);
        check(actual.value==expected.value && actual.length==expected.length,"B-MBT exhaustive prefix mismatch");
    }
    EeIpu invalid; invalid.write(EeIpu::Control,0x03000000);
    push(invalid,stream("000000",0xf1234567)); invalid.write(EeIpu::Command,0x34000000);
    check(invalid.read(EeIpu::Command)==0 && (invalid.read(EeIpu::Control)&0x4000),"B-MBT invalid prefix lacks ECD");
    check((invalid.read(EeIpu::Position)&127)==0,"B-MBT invalid prefix consumed bits");

    EeIpu partial; partial.write(EeIpu::Control,0x03000000);
    partial.write(EeIpu::Command,90); partial.takeInterrupt();
    partial.write(EeIpu::Command,0x34000006); // FB charged once before real input
    check(partial.read(EeIpu::Command+4)==0x80000000,"B-MBT did not wait for input");
    Quadword first{}; first[12]=4; push(partial,first); // 000001, length6, quant+intra
    check(partial.read(EeIpu::Command)==0x60091 && (partial.read(EeIpu::Position)&127)==102,
          "B-MBT starved DATA/FB mismatch");
    check(partial.read(EeIpu::Command+4)==0x80000000 && !partial.takeInterrupt(),"B-MBT completed before TOP");
    Quadword second{}; second[0]=0x80; push(partial,second);
    check(partial.read(EeIpu::Top)==32 && partial.read(EeIpu::Command)==0x60091,
          "B-MBT TOP retry decoded twice or consumed extra bits");
    check((partial.read(EeIpu::Position)&127)==102 && partial.takeInterrupt(),"B-MBT refill completion mismatch");
}

void testMotionCodes() {
    // Independent magnitude bitstrings, followed by a sign bit except zero.
    constexpr std::array<std::string_view,17> codes{{
        "1","01","001","0001","000011","0000101","0000100","0000011",
        "000001011","000001010","000001001","0000010001","0000010000",
        "0000001111","0000001110","0000001101","0000001100",
    }};
    for (unsigned magnitude=0;magnitude<codes.size();++magnitude) {
        for (unsigned sign=0;sign<(magnitude?2u:1u);++sign) {
            const std::string code=std::string(codes[magnitude])+(magnitude?(sign?"1":"0"):"");
            const uint32_t expected=!magnitude?0x10000u:sign?0u-magnitude:
                (uint32_t(codes[magnitude].size())<<16)|magnitude;
            EeIpu ipu;
            push(ipu,stream(code,0xa53cf087u));
            ipu.write(EeIpu::Command,0x38000000u);
            check(ipu.read(EeIpu::Command)==expected,"Motion signed DATA/length mismatch");
            check(ipu.read(EeIpu::Top)==0xa53cf087u,"Motion consumed sign or residual bits incorrectly");
            check((ipu.read(EeIpu::Position)&0x7f)==code.size(),"Motion BP mismatch");
            check(!(ipu.read(EeIpu::Control)&0x80004000u),"Motion BUSY/ECD mismatch");
            check(ipu.takeInterrupt() && !ipu.takeInterrupt(),"Motion completion IRQ mismatch");
        }
    }
    EeIpu invalid;
    push(invalid,stream("0000000000",0xa53cf087u));
    invalid.write(EeIpu::Command,0x38000000u);
    check(invalid.read(EeIpu::Command)==0 && (invalid.read(EeIpu::Control)&0x4000),
          "Invalid motion prefix did not set ECD");
    check((invalid.read(EeIpu::Position)&0x7f)==0,"Invalid motion prefix consumed bits");
    invalid.write(EeIpu::Command,0x3800000au);
    check(invalid.read(EeIpu::Command)==0x10000u && !(invalid.read(EeIpu::Control)&0x4000),
          "Valid zero motion did not clear ECD");

    EeIpu partial;
    partial.write(EeIpu::Command,0x0000005eu); // BP94, two FB bits before BP96 code
    partial.takeInterrupt();
    partial.write(EeIpu::Command,0x38000002u);
    check(partial.read(EeIpu::Command+4)==0x80000000u,"Motion did not wait for input");
    Quadword first{}; first[12]=0x60; // 011 = motion -1
    push(partial,first);
    check(partial.read(EeIpu::Command)==0xffffffffu,"Starved negative motion DATA differs");
    check((partial.read(EeIpu::Position)&0x7f)==99,"Motion FB/sign consumed twice");
    check(partial.read(EeIpu::Command+4)==0x80000000u && !partial.takeInterrupt(),
          "Motion completed before TOP refill");
    Quadword next{}; next[0]=0x80;
    push(partial,next);
    check(partial.read(EeIpu::Command)==0xffffffffu && partial.read(EeIpu::Top)==4,
          "Motion re-decoded on TOP refill");
    check((partial.read(EeIpu::Position)&0x7f)==99 && partial.read(EeIpu::Command+4)==0 &&
          partial.takeInterrupt() && !partial.takeInterrupt(),"Motion retry completion differs");
}
}

int main() {
    try {
        testAllMbaIncrementCodes();
        testMpeg1StuffingAndMpeg2Error();
        testVdecTablesAndPartialTop();
        testIntraMacroblockType();
        testPredictiveMacroblockType();
        testBidirectionalMacroblockType();
        testMotionCodes();
        std::cout << "IPU VDEC MBAI vectors passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
