#include "quiescent_sio2.h"
#include "sio2_dma.h"
#include <map>

namespace {
void expect(bool value) { if (!value) throw std::runtime_error("Virtual pad protocol check failed"); }
template<class Action> void rejects(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unsupported virtual pad operation accepted");
}
struct Registers {
    static constexpr uint32_t RamSize=256;
    std::array<uint8_t,RamSize> ram{};
    std::map<uint32_t,uint32_t> values;
    uint32_t pending{};
    uint32_t read32(uint32_t address) const { const auto p=values.find(address); return p==values.end()?0:p->second; }
    void write32(uint32_t address,uint32_t value) { values[address]=value; }
    uint8_t read8(uint32_t address) const { return ram.at(address); }
    void write8(uint32_t address,uint8_t value) { ram.at(address)=value; }
    uint32_t interruptStatus() const { return pending; }
    void setInterruptStatus(uint32_t value) { pending=value; }
};
}
void checkVirtualPad() {
    Dualshock2 pad; VirtualPadInput input; input.connected=true; input.buttons=0xbff7;
    const std::array<uint8_t,5> poll{1,0x42,0,0,0};
    expect(pad.exchange(poll,input)==std::vector<uint8_t>({0xff,0x41,0x5a,0xf7,0xbf}));
    const std::array<uint8_t,5> enter{1,0x43,0,1,0};
    pad.exchange(enter,input); expect(pad.inConfig());
    const std::array<uint8_t,9> status{1,0x45};
    expect(pad.exchange(status,input)==std::vector<uint8_t>({0xff,0xf3,0x5a,3,2,0,2,1,0}));
    const std::array<uint8_t,9> analog{1,0x44,0,1,3};
    pad.exchange(analog,input); expect(pad.currentMode()==0x73);
    const std::array<uint8_t,9> pressure{1,0x4f,0,0xff,0xff,3};
    pad.exchange(pressure,input); expect(pad.currentMode()==0x79);
    const std::array<uint8_t,9> exit{1,0x43};
    pad.exchange(exit,input); expect(!pad.inConfig());
    std::array<uint8_t,21> fullPoll{1,0x42};
    input.axes={0,255,42,127}; input.pressure[6]=200;
    const auto full=pad.exchange(fullPoll,input);
    expect(full[1]==0x79 && full[5]==0 && full[6]==255 && full[7]==42 && full[15]==200);
    input.connected=false;
    expect(pad.exchange(poll,input)==std::vector<uint8_t>(5,0xff));
    input.connected=true;
    const std::array<uint8_t,5> unknown{1,0x99};
    rejects([&] { pad.exchange(unknown,input); });
    expect(pad.exchange(std::span<const uint8_t>(poll).first(2),input)==std::vector<uint8_t>({0xff,0x79}));
    rejects([&] { pad.exchange({},input); });

    QuiescentSio2 sio; Registers regs;
    sio.enable(); sio.enableVirtualPad(); sio.publishInput(input);
    const auto store=[&](uint32_t address,uint32_t value) {
        regs.values[address]=value; sio.afterWrite(regs,address);
    };
    store(0x1f808268,0x3bc);
    store(0x1f808200,0x140540); store(0x1f808204,0);
    for (auto byte:poll) sio.fifoWrite(byte);
    expect(!sio.takeInterruptEdge()); // FIFO work alone is not START/IRQ.
    store(0x1f808268,0x3bd);
    expect(sio.takeInterruptEdge() && !sio.takeInterruptEdge() && sio.read(0x1f808280)==1);
    rejects([&] { store(0x1f808200,0x140540); }); // Unread data cannot be discarded.
    std::vector<uint8_t> reply; for (unsigned i=0;i<5;++i) reply.push_back(sio.fifoRead());
    expect(reply==std::vector<uint8_t>({0xff,0x41,0x5a,0xf7,0xbf}));
    rejects([&] { sio.fifoRead(); });
    store(0x1f808280,1); expect(sio.read(0x1f808280)==0);
    store(0x1f808268,0x3bc); store(0x1f808200,0x140541);
    for (auto byte:poll) sio.fifoWrite(byte);
    for (unsigned i=0;i<5;++i) expect(sio.fifoRead()==0xff);
    expect((sio.read(0x1f80826c)&0x2d000)==0x2d000); // Unattached port2 remains absent.

    store(0x1f808200,0x140540); store(0x1f808204,0x140541); store(0x1f808208,0);
    for (unsigned i=0;i<5;++i) { regs.ram[0x20+i]=poll[i]; regs.ram[0x30+i]=poll[i]; }
    Sio2Dma dma(sio);
    regs.values[0x1f801540]=0x20; regs.values[0x1f801544]=0x20004;
    regs.values[0x1f801550]=0x80; regs.values[0x1f801554]=0x20004;
    regs.values[0x1f801578]=1; regs.values[0x1f801570]=0x880000;
    regs.values[0x1f801574]=0xb00000; regs.pending=4;
    dma.validateStart(regs,0x1f801548,0x01000201);
    dma.validateStart(regs,0x1f801558,0x41000200);
    regs.values[0x1f801558]=0x41000200;
    dma.pump(regs); expect(regs.ram[0x80]==0); // RX waits for real TX data.
    regs.values[0x1f801548]=0x01000201;
    dma.pump(regs);
    expect((regs.read32(0x1f801548)&0x1000000) && (regs.read32(0x1f801558)&0x1000000));
    for (unsigned i=0;i<20;++i) dma.pump(regs);
    expect(!(regs.read32(0x1f801548)&0x1000000) && !(regs.read32(0x1f801558)&0x1000000));
    expect(regs.read32(0x1f801540)==0x40 && regs.read32(0x1f801550)==0xa0);
    expect(regs.read32(0x1f801544)==0x20004 && regs.read32(0x1f801554)==0x20004);
    expect(regs.ram[0x80]==0xff && regs.ram[0x81]==0x41 && regs.ram[0x83]==0xf7 && regs.ram[0x85]==0);
    expect(regs.ram[0x90]==0xff && regs.ram[0x91]==0xff && regs.ram[0x95]==0);
    expect((regs.read32(0x1f801574)&0x30000000)==0x30000000 && regs.pending==12);
    regs.values[0x1f801540]=0xfc;
    rejects([&] { dma.validateStart(regs,0x1f801548,0x01000201); });
    store(0x1f808200,0x100472);
    std::array<uint8_t,16> probe{0x81,0x11}; sio.dmaBlock(probe);
    expect(sio.read(0x1f80826c)==0x1d100 && sio.availableBytes()==16);
    for (unsigned i=0;i<16;++i) expect(sio.fifoRead()==(i<6?0xff:0));
    store(0x1f808200,0x140572); probe[1]=0xf3; sio.dmaBlock(probe);
    expect(sio.read(0x1f80826c)==0x1d100);
    for (unsigned i=0;i<16;++i) expect(sio.fifoRead()==(i<6?0xff:0));
    store(0x1f808200,0x100472); probe[1]=0x42;
    rejects([&] { sio.dmaBlock(probe); }); // Save/write protocols remain closed.
    store(0x1f808200,0xc0372);
    for (uint8_t byte:{0x81,0x28,0}) sio.fifoWrite(byte);
    expect(sio.read(0x1f80826c)==0x1d100 && sio.availableBytes()==3);
    for (unsigned i=0;i<3;++i) expect(sio.fifoRead()==0xff);
}
