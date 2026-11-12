#include "quiescent_sio2.h"
#include "sio2_dma.h"
#include <chrono>
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

void checkReadOnlyCard() {
    using drakengard::Ps2MemoryCard;
    const auto directory=std::filesystem::temp_directory_path()/("drakengard-card-test-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    expect(std::filesystem::create_directory(directory));
    const auto path=directory/"fixture.ps2";
    struct RemoveFixture {
        std::filesystem::path file, directory;
        ~RemoveFixture() {
            std::error_code ignored;
            auto lease=file;
            lease += ".drakengard.lock";
            std::filesystem::remove(lease,ignored);
            std::filesystem::remove(file,ignored);
            std::filesystem::remove(directory,ignored);
        }
    } cleanup{path,directory};

    QuiescentSio2 sio;
    sio.enable();
    rejects([&] { sio.attachCard(path); });
    sio.enableVirtualPad();
    {
        std::ofstream shortImage(path,std::ios::binary);
        shortImage.put(0);
    }
    rejects([&] { sio.attachCard(path); });
    std::vector<uint8_t> image(Ps2MemoryCard::ImageBytes,0xff);
    for (unsigned i=0;i<Ps2MemoryCard::PageBytes;++i)
        image[Ps2MemoryCard::PageBytes+i]=uint8_t(3*i+7);
    {
        std::ofstream file(path,std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(image.data()),image.size());
        file.close();
        expect(bool(file));
    }
    sio.attachCard(path);
    rejects([&] { sio.attachCard(path); });
    Registers regs;
    const auto store=[&](uint32_t address,uint32_t value) {
        regs.write32(address,value);
        sio.afterWrite(regs,address);
    };
    const auto descriptor=[&](uint32_t length,unsigned port=0) {
        store(0x1f808200,(length<<18)|(length<<8)|port);
    };
    const auto pio=[&](std::initializer_list<uint8_t> request) {
        descriptor(uint32_t(request.size()));
        for (auto byte:request) sio.fifoWrite(byte);
        std::vector<uint8_t> reply;
        while (sio.availableBytes()) reply.push_back(sio.fifoRead());
        return reply;
    };
    const auto rejectDma=[&](std::initializer_list<uint8_t> request) {
        descriptor(uint32_t(request.size()));
        std::array<uint8_t,16> block{};
        std::copy(request.begin(),request.end(),block.begin());
        rejects([&] { sio.dmaBlock(block); });
        expect(!sio.availableBytes());
    };
    store(0x1f808268,0x3bc);
    expect(pio({0x81,0x11,0,0})==std::vector<uint8_t>({0,0,0x2b,0x55}));
    expect(sio.read(0x1f80826c)==0x1100);
    expect(pio({0x81,0x26})==std::vector<uint8_t>({0,0,0x2b,0,2,16,0,0,64,0,0,0x52,0x55}));
    expect(pio({0x81,0x27,0x5a})==std::vector<uint8_t>({0,0,0,0x2b,0x5a}));
    expect(pio({0x81,0xbf,0,0,0})==std::vector<uint8_t>({0,0,0xff,0x2b,0x5a}));
    rejectDma({0x81,0xbf,0,0});
    expect(pio({0x81,0x28})==std::vector<uint8_t>({0,0,0x2b,0x5a,0x5a}));
    expect(pio({0x81,0xf3})==std::vector<uint8_t>({0,0,0,0x2b,0x55}));
    expect(pio({0x81,0x52,0})==std::vector<uint8_t>({0,0,0x5a}));
    rejectDma({0x81,0x52,0,0,0});
    rejectDma({0x81,0x43,1});
    rejectDma({0x81,0x23,1,0,0,0,0});
    expect(pio({0x81,0x23,1,0,0,0,1})==std::vector<uint8_t>({0,0,0,0,0,0,0,0x2b,0x55}));
    expect(pio({0x81,0x43,2})==std::vector<uint8_t>({0,0,0,0x2b,7,10,13,0x55}));

    constexpr unsigned payload=128, block=136, destination=120;
    descriptor(payload+6);
    regs.ram[0]=0x81; regs.ram[1]=0x43; regs.ram[2]=payload;
    regs.write32(0x1f801540,0); regs.write32(0x1f801544,0x10000|block/4);
    regs.write32(0x1f801550,destination); regs.write32(0x1f801554,0x10000|block/4);
    regs.write32(0x1f801578,1); regs.write32(0x1f801570,0x880000); regs.write32(0x1f801574,0x00b00000);
    regs.write32(0x1f801548,0x01000201); regs.write32(0x1f801558,0x41000200);
    Sio2Dma dma(sio);
    for (unsigned i=0;i<40;++i) dma.pump(regs);
    expect(regs.read32(0x1f801540)==block && regs.read32(0x1f801550)==destination+block);
    expect(regs.read32(0x1f801548)==0x201 && regs.read32(0x1f801558)==0x40000200 && regs.pending==8);
    uint8_t checksum=0;
    for (unsigned i=0;i<payload;++i) {
        const auto byte=image[Ps2MemoryCard::PageBytes+2+i];
        expect(regs.ram[destination+4+i]==byte);
        checksum^=byte;
    }
    expect(regs.ram[destination+3]==0x2b && regs.ram[destination+payload+4]==checksum);
    expect(regs.ram[destination+payload+5]==0x55 && regs.ram[destination+payload+6]==0);
    expect(pio({0x81,0x81})==std::vector<uint8_t>({0,0,0x2b,0x55}));
    rejectDma({0x81,0x43,1});
    rejectDma({0x81,0x22,1,0,0,0,1});
    rejectDma({0x81,0xf0,0});
    descriptor(4,1);
    sio.dmaBlock(std::array<uint8_t,8>{0x81,0x11,0,0});
    expect(sio.read(0x1f80826c)==0x1d100);
    for (unsigned i=0;i<8;++i) expect(sio.fifoRead()==(i<6?0xff:0));

    QuiescentSio2 writable;
    writable.enable(); writable.enableVirtualPad();
    writable.attachCard(path,true);
    rejects([&] { Ps2MemoryCard secondWriter(path,true); });
    regs.write32(0x1f808268,0x3bc);
    writable.afterWrite(regs,0x1f808268);
    const auto transfer=[&](std::span<const uint8_t> request, unsigned length) {
        std::array<uint8_t,144> buffer{};
        std::copy(request.begin(),request.end(),buffer.begin());
        regs.write32(0x1f808200,(length<<18)|(length<<8));
        writable.afterWrite(regs,0x1f808200);
        writable.dmaBlock(buffer);
        for (auto& byte:buffer) byte=writable.fifoRead();
        return buffer;
    };
    const std::array<uint8_t,7> selectWrite{0x81,0x22,1,0,0,0,1};
    const std::array<uint8_t,2> end{0x81,0x81};
    const std::array<uint8_t,3> readOne{0x81,0x43,1};
    const std::array<uint8_t,7> selectRead{0x81,0x23,1,0,0,0,1};
    expect(transfer(selectWrite,9)[8]==0x55);
    rejects([&] { transfer(end,4); });
    std::array<uint8_t,132> chunk{0x81,0x42,128};
    std::fill_n(chunk.begin()+3,128,uint8_t{0x3c});
    chunk.back()=1;
    rejects([&] { transfer(chunk,134); });
    chunk.back()=0;
    for (unsigned i=0;i<4;++i) {
        const auto reply=transfer(chunk,134);
        expect(reply[3]==0x2b && reply[132]==0 && reply[133]==0x55);
    }
    std::array<uint8_t,20> spare{0x81,0x42,16};
    std::fill_n(spare.begin()+3,16,uint8_t{0x55});
    expect(transfer(spare,22)[21]==0x55);
    const auto readCommitted=[&] {
        Ps2MemoryCard reopened(path);
        reopened.exchange(selectRead,9);
        return reopened.exchange(readOne,7)[4];
    };
    expect(readCommitted()==7);
#ifdef _WIN32
    {
        // Force the atomic replacement to fail without disturbing the old image.
        struct OpenImage {
            HANDLE handle;
            ~OpenImage() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } blocker{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        expect(blocker.handle != INVALID_HANDLE_VALUE);
        rejects([&] { transfer(end,4); });
        expect(readCommitted()==7);
    }
#endif
    expect(transfer(end,4)[3]==0x55);
    expect(readCommitted()==(7&0x3c));
    {
        std::ifstream saved(path,std::ios::binary);
        saved.seekg(Ps2MemoryCard::PageBytes+512);
        expect(saved.get()==(image[Ps2MemoryCard::PageBytes+512]&0x55));
    }
    const std::array<uint8_t,7> unaligned{0x81,0x21,1,0,0,0,1};
    rejects([&] { transfer(unaligned,9); });
    const std::array<uint8_t,7> selectErase{0x81,0x21,0,0,0,0,0};
    const std::array<uint8_t,2> erase{0x81,0x82}, eraseEnd{0x81,0x12};
    expect(transfer(selectErase,9)[8]==0x55);
    expect(transfer(erase,4)[3]==0x55);
    expect(transfer(eraseEnd,4)[3]==0x55);
    expect(readCommitted()==0xff);
}
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

    QuiescentSio2 edges; Registers edgeRegisters; edges.enable(); edges.enableVirtualPad();
    const auto edgeStore=[&](uint32_t address,uint32_t value) {
        edgeRegisters.write32(address,value); edges.afterWrite(edgeRegisters,address);
    };
    edgeStore(0x1f808268,0x3bc);
    VirtualPadInput released; released.connected=true; edges.publishInput(released);
    auto pressed=released; pressed.buttons=0xff7f;
    edges.publishInput(pressed); edges.publishInput(released); // Real tap between serial polls.
    const auto readReport=[&] {
        edgeStore(0x1f808200,0x140540);
        for (auto byte:poll) edges.fifoWrite(byte);
        std::array<uint8_t,5> result{};
        for (auto& byte:result) byte=edges.fifoRead();
        return result;
    };
    expect(readReport()==std::array<uint8_t,5>{0xff,0x41,0x5a,0x7f,0xff});
    expect(readReport()==std::array<uint8_t,5>{0xff,0x41,0x5a,0xff,0xff});
    edges.publishInput(pressed); released.connected=false; edges.publishInput(released);
    released.connected=true; edges.publishInput(released);
    expect(readReport()==std::array<uint8_t,5>{0xff,0x41,0x5a,0xff,0xff});

    // Negotiate pressure mode (ID 0x79) through the FIFO, then release
    // a short tap before the next report. The report must retain the pressure
    // byte from the actual press snapshot, not fabricate 255.
    edgeStore(0x1f808200,0x140540);
    for (auto byte:std::array<uint8_t,5>{1,0x43,0,1,0}) edges.fifoWrite(byte);
    for (unsigned i=0;i<5;++i) edges.fifoRead();
    edgeStore(0x1f808200,0x180600);
    for (auto byte:std::array<uint8_t,6>{1,0x4f,0,0xff,0xff,3}) edges.fifoWrite(byte);
    for (unsigned i=0;i<6;++i) edges.fifoRead();
    edgeStore(0x1f808200,0x140540);
    for (auto byte:std::array<uint8_t,5>{1,0x43,0,0,0}) edges.fifoWrite(byte);
    for (unsigned i=0;i<5;++i) edges.fifoRead();
    auto pressurePressed=released; pressurePressed.buttons=uint16_t(~(1u<<14));
    pressurePressed.pressure[6]=200;
    edges.publishInput(pressurePressed); edges.publishInput(released);
    edgeStore(0x1f808200,0x541500);
    std::array<uint8_t,21> pressurePoll{}; pressurePoll[0]=1; pressurePoll[1]=0x42;
    for (auto byte:pressurePoll) edges.fifoWrite(byte);
    std::array<uint8_t,21> pressureReply{};
    for (auto& byte:pressureReply) byte=edges.fifoRead();
    expect(pressureReply[1]==0x79 && pressureReply[9+6]==200);
    edgeStore(0x1f808200,0x541500);
    for (auto byte:pressurePoll) edges.fifoWrite(byte);
    for (auto& byte:pressureReply) byte=edges.fifoRead();
    expect(pressureReply[9+6]==0);

    // Focus cancellation clears the latch while retaining the connected pad.
    auto focusPressed= released; focusPressed.buttons=uint16_t(~(1u<<14)); focusPressed.pressure[6]=177;
    edges.publishInput(focusPressed);
    auto focusLost=released; focusLost.cancelPending=true; edges.publishInput(focusLost);
    expect(readReport()==std::array<uint8_t,5>{0xff,0x79,0x5a,0xff,0xff});
    checkReadOnlyCard();
}
