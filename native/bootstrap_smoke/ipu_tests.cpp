#include "ee_ipu.h"
#include "ee_ipu_dma.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <source_location>

using drakengard::EeIpu;
using drakengard::EeIpuDma;

namespace {
void check(bool value,const std::source_location where=std::source_location::current()) {
    if (!value) throw std::runtime_error("IPU behavioral regression failed at line "+std::to_string(where.line()));
}

template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unsupported IPU operation was accepted");
}

using Quadword = std::array<uint8_t, 16>;

Quadword quad(uint8_t first) {
    Quadword value{};
    for (unsigned i = 0; i < value.size(); ++i) value[i] = uint8_t(first + i);
    return value;
}

void push(EeIpu& ipu, const Quadword& value) {
    check(ipu.push(std::span<const uint8_t, 16>(value)));
}

void testBitCommands() {
    EeIpu ipu;

    // BCLR is an input-state operation: the low seven bits are the new BP,
    // and data already in the input FIFO is discarded.
    push(ipu, quad(0x10));
    ipu.write(EeIpu::Command, 0x00000055u);
    check((ipu.read(EeIpu::Position) & 0x7fu) == 0x55u);
    check((ipu.read(EeIpu::Position) >> 8) == 0u);
    check((ipu.read(EeIpu::Control) & 0x80000000u) == 0u);

    // FDEC must expose a real high-bit stall until input is available.
    ipu.write(EeIpu::Command, 0x40000000u);
    check(ipu.read(EeIpu::Command + 4) == 0x80000000u);
    check(ipu.read(EeIpu::Top + 4) == 0x80000000u);
    check((ipu.read(EeIpu::Control) & 0x80000000u) != 0u);
    push(ipu, quad(0x12));
    check(ipu.read(EeIpu::Command + 4) == 0u);
    check((ipu.read(EeIpu::Control) & 0x80000000u) == 0u);
    check(ipu.read(EeIpu::Command) == 0x83a3c3e4u);

    // A 32-bit extraction crossing the first 128-bit quadword is still
    // big-endian bitstream data, and TOP is a latched result, not a read/peek
    // cursor that advances the stream.
    ipu.write(EeIpu::Command, 0x0000007du); // BCLR, BP = 125
    push(ipu, quad(0x00));
    push(ipu, quad(0x80));
    ipu.write(EeIpu::Command, 0x40000000u); // FDEC, skip 0 bits
    check(ipu.read(EeIpu::Command) == 0xf0103050u);
    check(ipu.read(EeIpu::Command) == 0xf0103050u);
    check(ipu.read(EeIpu::Top) == 0xf0103050u);
    check(ipu.read(EeIpu::Top) == 0xf0103050u);
    check(ipu.read(EeIpu::Top + 4) == 0u);
    // Skip eight bits exactly once even if a new quadword arrives later.
    ipu.write(EeIpu::Command,0x00000078u);
    push(ipu,quad(0));
    ipu.write(EeIpu::Command,0x40000008u);
    check(ipu.read(EeIpu::Command+4)==0x80000000u);
    check((ipu.read(EeIpu::Position)&0x7f)==0);
    push(ipu,quad(0x80));
    check(ipu.read(EeIpu::Command)==0x80818283u);
    const auto position=ipu.read(EeIpu::Position);
    check(ipu.read(EeIpu::Command)==0x80818283u && ipu.read(EeIpu::Position)==position);
}

void testTablesAndReset() {
    EeIpu ipu;
    std::array<uint8_t, 64> intra{};
    std::array<uint8_t, 64> nonIntra{};
    std::array<uint8_t, 32> palette{};
    for (unsigned i = 0; i < intra.size(); ++i) {
        intra[i] = uint8_t(0x20u + i);
        nonIntra[i] = uint8_t(0xa0u + i);
    }
    for (unsigned i = 0; i < palette.size(); ++i) palette[i] = uint8_t(0xe0u + i);

    ipu.write(EeIpu::Command, 0x50000000u); // SETIQ, intra matrix
    for (unsigned at = 0; at < intra.size(); at += 16) {
        Quadword q{}; std::copy_n(intra.data() + at, q.size(), q.data()); push(ipu, q);
    }
    ipu.write(EeIpu::Command, 0x58000000u); // SETIQ, non-intra matrix
    for (unsigned at = 0; at < nonIntra.size(); at += 16) {
        Quadword q{}; std::copy_n(nonIntra.data() + at, q.size(), q.data()); push(ipu, q);
    }
    ipu.write(EeIpu::Command, 0x60000000u); // SETVQ
    push(ipu, Quadword{palette[0],palette[1],palette[2],palette[3],palette[4],palette[5],palette[6],palette[7],
                       palette[8],palette[9],palette[10],palette[11],palette[12],palette[13],palette[14],palette[15]});
    push(ipu, Quadword{palette[16],palette[17],palette[18],palette[19],palette[20],palette[21],palette[22],palette[23],
                       palette[24],palette[25],palette[26],palette[27],palette[28],palette[29],palette[30],palette[31]});
    check(std::equal(intra.begin(), intra.end(), ipu.quantization(false).begin()));
    check(std::equal(nonIntra.begin(), nonIntra.end(), ipu.quantization(true).begin()));
    check(std::equal(palette.begin(), palette.end(), ipu.palette().begin()));

    ipu.write(EeIpu::Command, 0x91de01ffu); // SETTH: only each 9-bit field survives
    check(ipu.alphaThresholds()[0] == 0x1ffu && ipu.alphaThresholds()[1] == 0x1deu);
    ipu.write(EeIpu::Control, 0x40000000u); // reset FIFO/command state, not tables
    check(std::equal(intra.begin(), intra.end(), ipu.quantization(false).begin()));
    check(std::equal(nonIntra.begin(), nonIntra.end(), ipu.quantization(true).begin()));
    check(std::equal(palette.begin(), palette.end(), ipu.palette().begin()));
}

void testCompletionBoundaries() {
    EeIpu ipu;
    check(!ipu.takeInterrupt());
    ipu.write(EeIpu::Command,0x50000000u);
    for (unsigned i=0;i<3;++i) {
        push(ipu,quad(uint8_t(i*16)));
        check((ipu.read(EeIpu::Control)&0x80000000u) && !ipu.takeInterrupt());
    }
    push(ipu,quad(48));
    check(!(ipu.read(EeIpu::Control)&0x80000000u));
    check(ipu.takeInterrupt() && !ipu.takeInterrupt());
    for (unsigned i=0;i<64;++i) check(ipu.quantization(false)[i]==i);
    ipu.write(EeIpu::Command,0x40000000u);
    check(ipu.read64(EeIpu::Command)==0x8000000000000000ull);
    check(ipu.read64(EeIpu::Top)==0x8000000000000000ull);
    ipu.write(EeIpu::Control,0x40000000u); // Abort a real stalled operation.
    check(ipu.read64(EeIpu::Command)==0 && ipu.read64(EeIpu::Top)==0);
    check(ipu.read(EeIpu::Position)==0 && ipu.takeInterrupt() && !ipu.takeInterrupt());
    check(ipu.quantization(false)[63]==63); // Reset did not erase the loaded matrix.
}

void testFifoAndDma() {
    EeIpu ipu;
    for (unsigned i = 0; i < 8; ++i) check(ipu.push(std::span<const uint8_t, 16>(quad(uint8_t(i * 16)))));
    check(!ipu.canAcceptInput());
    check(!ipu.push(std::span<const uint8_t, 16>(quad(0xf0))));
    ipu.write(EeIpu::Command, 0x00000000u); // BCLR empties the bounded FIFO
    check(ipu.canAcceptInput());

    std::vector<uint8_t> ram(0x200, 0);
    for (unsigned i = 0; i < 32; ++i) ram[0x40 + i] = uint8_t(0x30u + i);
    EeIpuDma dma;
    const uint32_t to = 0x1000b400u;
    dma.write(to + 0x10, 0x40u);
    dma.write(to + 0x20, 2u);
    dma.write(to, 0x101u); // STR + to-IPU normal mode
    dma.pump(ipu, ram, false);
    check(dma.read(to + 0x20) == 2u && !dma.takeCompletions());
    dma.pump(ipu, ram, true);
    check(dma.read(to + 0x20) == 0u && dma.read(to + 0x10) == 0x60u);
    check(dma.takeCompletions() == (1u << 4));
    check(!dma.takeCompletions());
    ipu.write(EeIpu::Command,0x40000000u);
    check(ipu.read(EeIpu::Command)==0x30313233u); // Data came from actual DMA RAM.

    // A full input FIFO backpressures a normal transfer without consuming RAM.
    ipu.write(EeIpu::Command, 0x00000000u);
    for (unsigned i = 0; i < 8; ++i) push(ipu, quad(uint8_t(i)));
    dma.write(to + 0x10, 0x40u); dma.write(to + 0x20, 1u); dma.write(to, 0x101u);
    dma.pump(ipu, ram, true);
    check(dma.read(to + 0x20) == 1u && (dma.read(to) & 0x100u) != 0u);
    ipu.write(EeIpu::Command, 0x00000000u);
    dma.pump(ipu, ram, true);
    check(dma.read(to + 0x20) == 0u && dma.takeCompletions() == (1u << 4));

    refuses([&] { dma.write(to, 0x100u); }); // wrong direction
    refuses([&] { dma.write(to, 0x141u); }); // chain/tag-transfer mode
    dma.write(to, 0u);
    // Normal-mode zero count means 65536 QW; a full FIFO must not complete it.
    for (unsigned i=0;i<7;++i) push(ipu,quad(0));
    dma.write(to+0x20,0); dma.write(to,0x101);
    dma.pump(ipu,ram,true);
    check(dma.read(to+0x20)==0x10000u && !dma.takeCompletions());
    refuses([&] { dma.write(to+0x10,0x80); });
    refuses([&] { dma.write(to,0x101); });
    dma.write(to,0); // Actual stop, without a completion IRQ.
    const uint32_t from=0x1000b000u;
    dma.write(from+0x20,1); dma.write(from,0x100);
    dma.pump(ipu,ram,true);
    check(dma.read(from+0x20)==1 && (dma.read(from)&0x100) && !dma.takeCompletions());
    refuses([&] { ipu.write(EeIpu::Command, 0x10000001u); }); // invalid opcode
    refuses([&] { ipu.write(EeIpu::Command, 0x40000040u); }); // invalid FDEC option
}
}

void checkIpu() {
    testBitCommands();
    testTablesAndReset();
    testCompletionBoundaries();
    testFifoAndDma();
}

int main() {
    try { checkIpu(); std::cout << "IPU bitstream, table and DMA regressions passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
