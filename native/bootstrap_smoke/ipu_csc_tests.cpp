#include "ee_ipu.h"
#include "ee_ipu_dma.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <source_location>
#include <vector>

namespace {
using drakengard::EeIpu;
using drakengard::EeIpuDma;
using Qw=std::array<uint8_t,16>;
using Yuv=std::array<uint8_t,384>;
using Rgba=std::array<uint8_t,4>;
void check(bool value,const std::source_location where=std::source_location::current()) {
    if (!value) throw std::runtime_error("CSC regression line "+std::to_string(where.line()));
}
Yuv flat(unsigned y,unsigned cb=128,unsigned cr=128) {
    Yuv block;
    std::fill_n(block.begin(),256,uint8_t(y));
    std::fill_n(block.begin()+256,64,uint8_t(cb));
    std::fill_n(block.begin()+320,64,uint8_t(cr));
    return block;
}
void feed(EeIpu& ipu,const Yuv& block) {
    for (unsigned offset=0;offset<384;offset+=16)
        check(ipu.push(std::span<const uint8_t,16>(block.data()+offset,16)));
}
std::vector<uint8_t> drain(EeIpu& ipu) {
    Qw qw; std::vector<uint8_t> result;
    while (ipu.pop(qw)) result.insert(result.end(),qw.begin(),qw.end());
    return result;
}
void pixels(std::span<const uint8_t> actual,Rgba expected) {
    check(actual.size()==1024);
    for (unsigned p=0;p<256;++p) for (unsigned c=0;c<4;++c) check(actual[p*4+c]==expected[c]);
}
void testColorGoldensAndThresholds() {
    struct Vector { uint8_t y,cb,cr; Rgba rgba; };
    constexpr std::array<Vector,10> vectors{{
        {16,128,128,{0,0,0,128}}, {235,128,128,{255,255,255,128}},
        {81,90,240,{254,0,0,128}}, {145,54,34,{0,255,1,128}},
        {41,240,110,{0,0,255,128}}, {100,255,0,{0,152,255,128}},
        {0,0,255,{202,0,0,128}}, {255,128,128,{255,255,255,128}},
        {17,128,128,{1,1,1,128}}, {40,127,127,{26,28,25,128}},
    }};
    for (const auto& vector:vectors) {
        EeIpu ipu;
        ipu.write(EeIpu::Command,0x70000001);
        feed(ipu,flat(vector.y,vector.cb,vector.cr));
        const auto result=drain(ipu);
        pixels(result,vector.rgba);
        check(ipu.takeInterrupt() && !ipu.takeInterrupt());
    }
    struct Threshold { unsigned low,high; Rgba rgba; };
    constexpr std::array<Threshold,6> thresholds{{
        {28,0,{28,28,28,128}}, {29,0,{0,0,0,0}}, {0,28,{28,28,28,128}},
        {0,29,{28,28,28,64}}, {29,511,{0,0,0,0}}, {0,511,{28,28,28,64}},
    }};
    for (const auto& threshold:thresholds) {
        EeIpu ipu;
        ipu.write(EeIpu::Command,0x90000000u|threshold.low|(threshold.high<<16));
        ipu.takeInterrupt();
        ipu.write(EeIpu::Command,0x74000001); // DTE is ignored with RGB32 output.
        feed(ipu,flat(40));
        pixels(drain(ipu),threshold.rgba);
    }
}
void testPlanarLayout() {
    auto block=flat(81);
    constexpr std::array<uint8_t,4> cb{90,128,240,54},cr{240,128,110,34};
    constexpr std::array<Rgba,4> expected{{{254,0,0,128},{76,76,76,128},
                                           {47,46,255,128},{0,180,0,128}}};
    for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) {
        const unsigned quadrant=(y/4)*2+x/4;
        block[256+y*8+x]=cb[quadrant]; block[320+y*8+x]=cr[quadrant];
    }
    block[17]=235; // Only Y(1,1), not an entire tiled DCT block or chroma group.
    EeIpu ipu; ipu.write(EeIpu::Command,0x70000001); feed(ipu,block);
    const auto result=drain(ipu); check(result.size()==1024);
    for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x) {
        const auto want=y==1 && x==1?Rgba{255,178,178,128}:expected[(y/8)*2+x/8];
        for (unsigned c=0;c<4;++c) check(result[(y*16+x)*4+c]==want[c]);
    }
}
void testCompletionBeforeFinalFifoDrain() {
    EeIpu ipu;
    ipu.write(EeIpu::Command,0x70000001);
    check((ipu.read(EeIpu::Control)&0x80000000u) && !ipu.takeInterrupt());
    feed(ipu,flat(16));
    check(ipu.outputCount()==8 && (ipu.read(EeIpu::Control)&0x80000000u) && !ipu.takeInterrupt());
    Qw qw;
    for (unsigned i=0;i<56;++i) check(ipu.pop(qw));
    check(ipu.outputCount()==8 && !(ipu.read(EeIpu::Control)&0x80000000u));
    check(ipu.takeInterrupt() && !ipu.takeInterrupt()); // final8QW enqueued, not drained
    for (unsigned i=0;i<8;++i) check(ipu.pop(qw));
    check(!ipu.pop(qw) && !ipu.takeInterrupt() && ipu.read(EeIpu::Top)==0);
    check((ipu.read(EeIpu::Position)&0x7fu)==0); // exactly24 input QWs, no trailing TOP read
}
void testBitOffsetAndReset() {
    const auto block=flat(81,90,240);
    std::vector<uint8_t> packed(400);
    unsigned bit=3;
    const auto put=[&](unsigned value,unsigned width) {
        for (unsigned n=width;n;--n,++bit)
            if (value&(1u<<(n-1))) packed[bit/8]|=uint8_t(0x80u>>(bit&7));
    };
    for (auto value:block) put(value,8);
    put(0xa53cf087,32);
    EeIpu ipu;
    ipu.write(EeIpu::Command,3); ipu.takeInterrupt();
    ipu.write(EeIpu::Command,0x70000001);
    for (unsigned offset=0;offset<384;offset+=16)
        check(ipu.push(std::span<const uint8_t,16>(packed.data()+offset,16)));
    check(ipu.outputCount()==0 && !ipu.takeInterrupt()); // needs final3 source bits
    check(ipu.push(std::span<const uint8_t,16>(packed.data()+384,16)));
    pixels(drain(ipu),{254,0,0,128});
    check((ipu.read(EeIpu::Position)&0x7f)==3 && ipu.read(EeIpu::Top)==0);
    ipu.write(EeIpu::Command,0x40000000);
    check(ipu.read(EeIpu::Command)==0xa53cf087 && ipu.read(EeIpu::Top)==0xa53cf087);

    EeIpu reset;
    reset.write(EeIpu::Command,0x70000002);
    Qw partial{}; partial.fill(200);
    for (unsigned i=0;i<11;++i) check(reset.push(partial));
    reset.write(EeIpu::Control,0x40000000); reset.takeInterrupt();
    reset.write(EeIpu::Command,0x70000001); feed(reset,flat(16));
    reset.write(EeIpu::Control,0x40000000); reset.takeInterrupt(); // abandon staged RGB/FIFO
    check(reset.outputCount()==0 && !reset.pop(partial));
    reset.write(EeIpu::Command,0x70000001); feed(reset,flat(235));
    pixels(drain(reset),{255,255,255,128});
}
void testDmaBackpressureAndMacroblockCount() {
    EeIpu ipu; EeIpuDma dma;
    std::vector<uint8_t> ram(0x2000,0xa5);
    const auto black=flat(16),white=flat(235);
    std::copy(black.begin(),black.end(),ram.begin()+0x100);
    std::copy(white.begin(),white.end(),ram.begin()+0x280);
    constexpr uint32_t to=0x1000b400,from=0x1000b000;
    dma.write(to+0x10,0x100); dma.write(to+0x20,48); dma.write(to,0x101);
    ipu.write(EeIpu::Command,0x70000002);
    const auto initial=ram;
    dma.pump(ipu,ram,true); // no destination DMA: output pressure must stall input
    check(ipu.outputCount()==8 && dma.read(to+0x20)>0 && dma.takeCompletions()==0);
    check(ram==initial && !ipu.takeInterrupt());
    dma.write(from+0x10,0x800); dma.write(from+0x20,10); dma.write(from,0x100);
    dma.pump(ipu,ram,true);
    check(dma.read(from+0x10)==0x8a0 && dma.read(from+0x20)==0 && dma.takeCompletions()==8);
    check((ipu.read(EeIpu::Control)&0x80000000u) && !ipu.takeInterrupt());
    dma.write(from+0x20,118); dma.write(from,0x100);
    dma.pump(ipu,ram,true);
    check(dma.takeCompletions()==24 && dma.takeCompletions()==0);
    check(dma.read(from+0x10)==0x1000 && dma.read(to+0x10)==0x400 && dma.read(to+0x20)==0);
    pixels(std::span<const uint8_t>(ram).subspan(0x800,1024),{0,0,0,128});
    pixels(std::span<const uint8_t>(ram).subspan(0xc00,1024),{255,255,255,128});
    check(ram[0x7ff]==0xa5 && ram[0x1000]==0xa5 &&
          std::equal(initial.begin()+0x100,initial.begin()+0x400,ram.begin()+0x100));
    check(!(ipu.read(EeIpu::Control)&0x80000000u) && ipu.takeInterrupt() && !ipu.takeInterrupt());
}
void testZeroBlocksAndRgb16Refusal() {
    EeIpu ipu;
    ipu.write(EeIpu::Command,0x70000000);
    check(ipu.outputCount()==0 && !(ipu.read(EeIpu::Control)&0x80000000u) && ipu.takeInterrupt());
    try { ipu.write(EeIpu::Command,0x78000001); }
    catch (const std::runtime_error&) {
        check(ipu.outputCount()==0 && !(ipu.read(EeIpu::Control)&0x80000000u));
        return;
    }
    throw std::runtime_error("unsupported RGB16 CSC manufactured success");
}
}
int main() {
    try {
        testColorGoldensAndThresholds(); testPlanarLayout();
        testCompletionBeforeFinalFifoDrain(); testBitOffsetAndReset();
        testDmaBackpressureAndMacroblockCount(); testZeroBlocksAndRgb16Refusal();
        std::cout << "RGB32 CSC behavior passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
