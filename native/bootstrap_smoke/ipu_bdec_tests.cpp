#include "ee_ipu.h"
#include "ee_ipu_dma.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <source_location>
#include <string>
#include <vector>

namespace {
using drakengard::EeIpu;
using drakengard::EeIpuDma;
using Qw=std::array<uint8_t,16>;
void check(bool value,const std::source_location where=std::source_location::current()) {
    if (!value) throw std::runtime_error("BDEC regression line "+std::to_string(where.line()));
}
struct Stream {
    std::string bits;
    void put(unsigned value,unsigned size) {
        for (unsigned i=size;i;--i) bits+=((value>>(i-1))&1)?'1':'0';
    }
    void dc(int difference,bool chroma=false) {
        unsigned size=0; for (unsigned magnitude=unsigned(std::abs(difference));magnitude;magnitude>>=1) ++size;
        constexpr unsigned codes[]{4,0,1,5,6,14,30,62,126,254,510,511};
        constexpr unsigned lengths[]{3,2,2,3,3,4,5,6,7,8,9,9};
        if (chroma) put(size<3?size:size==11?1023:(1u<<(size<11?size:10))-2,size<3?2:size<11?size:10);
        else put(codes[size],lengths[size]);
        if (size) put(difference<0?unsigned(difference+int((1u<<size)-1)):unsigned(difference),size);
    }
    void eob() { put(6,4); }
    void block(int difference,bool chroma=false) { dc(difference,chroma); eob(); }
    void macro(const std::array<int,6>& differences) {
        for (unsigned b=0;b<6;++b) block(differences[b],b>=4);
    }
    std::vector<Qw> quads() const {
        std::vector<Qw> result((bits.size()+127)/128+1);
        for (unsigned i=0;i<bits.size();++i) if (bits[i]=='1') result[i/128][(i%128)/8]|=uint8_t(1u<<(7-i%8));
        return result;
    }
};
void feed(EeIpu& ipu,const Stream& stream) { for (const auto& q:stream.quads()) check(ipu.push(q)); }
std::vector<uint8_t> drain(EeIpu& ipu) {
    std::vector<uint8_t> bytes;
    Qw q{};
    while (ipu.pop(q)) bytes.insert(bytes.end(),q.begin(),q.end());
    return bytes;
}
void setup(EeIpu& ipu) {
    ipu.write(EeIpu::Command,0x50000000);
    Qw matrix{}; matrix.fill(16);
    for (unsigned i=0;i<4;++i) check(ipu.push(matrix));
    check(ipu.takeInterrupt());
    ipu.write(EeIpu::Control,0x01620000);
}
void testLayoutAndPredictors() {
    EeIpu ipu; setup(ipu);
    Stream s; s.macro({32,32,-64,64,-32,64}); s.put(0xabcdef01,32); feed(ipu,s);
    ipu.write(EeIpu::Command,0x2c010000);
    check(ipu.outputCount()==8 && (ipu.read(EeIpu::Control)&0x80000000) && !ipu.takeInterrupt());
    auto bytes=drain(ipu);
    check(bytes.size()==768 && ipu.outputCount()==0);
    check(!(ipu.read(EeIpu::Control)&0x80000000) && ipu.takeInterrupt() && !ipu.takeInterrupt());
    check(ipu.read(EeIpu::Top)==0xabcdef01);
    // IDP2: one DC unit gives 1/4 pixel; Y blocks share a predictor.
    for (unsigned i=0;i<384;++i) {
        const unsigned y=i/16,x=i%16;
        const unsigned want=i>=320?144:i>=256?120:y<8?(x<8?136:144):(x<8?128:144);
        check(bytes[2*i]==want && bytes[2*i+1]==0);
    }
    // Next command retains the Y/Cb/Cr predictors when DCR=0, after a real BCLR.
    ipu.write(EeIpu::Command,0); ipu.takeInterrupt();
    Stream next; next.macro({0,0,0,0,0,0}); next.put(0xfedcba98,32); feed(ipu,next);
    ipu.write(EeIpu::Command,0x28010000);
    bytes=drain(ipu);
    for (unsigned i=0;i<384;++i) check(bytes[i*2]==(i>=320?144:i>=256?120:144));
}
void testStarvationAndStartCode() {
    EeIpu ipu; setup(ipu);
    Stream s;
    // Force several real quadword boundaries: signed escape at each position.
    for (unsigned b=0;b<6;++b) {
        s.dc(0,b>=4);
        for (unsigned i=0;i<5;++i) { s.put(1,6); s.put(0,6); s.put(i%2?4095:1,12); }
        s.eob();
    }
    while (s.bits.size()%8) s.put(0,1);
    s.put(0,8); s.put(0x000001b3,32);
    auto quads=s.quads();
    ipu.write(EeIpu::Command,0x2c010000);
    check((ipu.read(EeIpu::Control)&0x80000000) && !ipu.takeInterrupt());
    for (unsigned i=0;i<quads.size();++i) {
        check(ipu.push(quads[i]));
        if (i+1<quads.size()-1) check(!ipu.takeInterrupt());
    }
    check(drain(ipu).size()==768);
    check(ipu.takeInterrupt() && !(ipu.read(EeIpu::Control)&0x80000000));
    check((ipu.read(EeIpu::Control)&0xc000)==0x8000);
    check(ipu.read(EeIpu::Top)==0x000001b3);
    // Reset discards pending output and busy command, preserving tables.
    ipu.write(EeIpu::Command,0); ipu.takeInterrupt();
    Stream flat; flat.macro({0,0,0,0,0,0}); flat.put(0x80808080,32); feed(ipu,flat);
    ipu.write(EeIpu::Command,0x2c010000); check(ipu.outputCount()==8);
    ipu.write(EeIpu::Control,0x40000000); check(ipu.outputCount()==0 && ipu.takeInterrupt());
    check(!(ipu.read(EeIpu::Control)&0x80000000));
}
void testScratchDma() {
    EeIpu ipu; setup(ipu); EeIpuDma dma;
    std::vector<uint8_t> ram(0x4000,0xa5),scratch(0x4000,0xcc);
    Stream s; s.macro({0,0,0,0,0,0}); s.put(0xabcdef01,32); feed(ipu,s);
    constexpr uint32_t from=0x1000b000;
    dma.write(from+0x10,0x80001800); dma.write(from+0x20,48); dma.write(from,0x100);
    dma.pump(ipu,ram,scratch,true);
    check(dma.read(from+0x20)==48 && !dma.takeCompletions());
    ipu.write(EeIpu::Command,0x2c010000);
    dma.pump(ipu,ram,scratch,false);
    check(dma.read(from+0x20)==48 && scratch[0x1800]==0xcc);
    dma.pump(ipu,ram,scratch,true);
    check(dma.read(from+0x20)==0 && dma.read(from+0x10)==0x80001b00 && !(dma.read(from)&0x100));
    check(dma.takeCompletions()==8 && !dma.takeCompletions() && ipu.takeInterrupt());
    for (unsigned i=0;i<scratch.size();++i)
        check(scratch[i]==(i<0x1800 || i>=0x1b00?0xcc:(i%2?0:128)));
    check(std::all_of(ram.begin(),ram.end(),[](uint8_t v){return v==0xa5;}));
    // Invalid SPR MADRs fail before a quadword is popped or RDRAM touched.
    for (auto address:{0x80004000u,0xa0001800u,0x70004000u}) {
        ipu.write(EeIpu::Command,0); ipu.takeInterrupt(); feed(ipu,s);
        ipu.write(EeIpu::Command,0x2c010000);
        dma.write(from+0x10,address); dma.write(from+0x20,1); dma.write(from,0x100);
        bool refused=false;
        try { dma.pump(ipu,ram,scratch,true); } catch (const std::runtime_error&) { refused=true; }
        check(refused && ipu.outputCount()==8 && dma.read(from+0x20)==1 && !dma.takeCompletions());
        check(std::all_of(ram.begin(),ram.end(),[](uint8_t v){return v==0xa5;}));
        dma.write(from,0); ipu.write(EeIpu::Control,0x41620000); ipu.takeInterrupt();
    }
}
void testAcPixelsAndPartialDma() {
    EeIpu ipu; setup(ipu); EeIpuDma dma;
    Stream s; s.dc(0); s.put(2,2); s.put(0,1); s.eob(); // run0,+1 => horizontal AC
    for (unsigned b=1;b<6;++b) s.block(0,b>=4);
    s.put(0xabcdef01,32); feed(ipu,s);
    ipu.write(EeIpu::Command,0x2c080000); // scale8 => coefficient8 with matrix16
    std::vector<uint8_t> ram(768,0xa5),scratch(0x4000,0xcc);
    constexpr uint32_t from=0x1000b000;
    dma.write(from+0x20,8); dma.write(from,0x100); dma.pump(ipu,ram,scratch,true);
    check(dma.takeCompletions()==8 && !ipu.takeInterrupt() && (ipu.read(EeIpu::Control)&0x80000000));
    dma.write(from+0x20,40); dma.write(from,0x100); dma.pump(ipu,ram,scratch,true);
    check(dma.takeCompletions()==8 && ipu.takeInterrupt());
    constexpr unsigned expected[]{129,129,129,128,128,127,127,127};
    for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) check(ram[(y*16+x)*2]==expected[x]);
    check(ram[16]==128 && ram[512]==128 && ram[640]==128);
}
void testSkipAndFieldDct() {
    EeIpu ipu; setup(ipu);
    Stream s; s.put(0xa5,8); s.macro({32,32,-64,64,0,0}); s.put(0xabcdef01,32);
    // FB is charged once on command start even when no input exists yet.
    ipu.write(EeIpu::Command,0x2e010008);
    check((ipu.read(EeIpu::Position)&127)==8 && !ipu.takeInterrupt());
    feed(ipu,s); const auto bytes=drain(ipu);
    check(bytes.size()==768 && ipu.takeInterrupt() && ipu.read(EeIpu::Top)==0xabcdef01);
    // Field DCT places blocks 0/1 on even luma lines and 2/3 on odd lines.
    for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x) {
        const unsigned want=(y%2)?(x<8?128:144):(x<8?136:144);
        check(bytes[(y*16+x)*2]==want);
    }
}
}
int main() {
    try { testLayoutAndPredictors(); testStarvationAndStartCode(); testScratchDma(); testAcPixelsAndPartialDma(); testSkipAndFieldDct(); std::cout<<"BDEC state/output regressions passed\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
