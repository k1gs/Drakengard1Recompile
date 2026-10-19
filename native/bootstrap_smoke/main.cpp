#include "read_only_rom.h"
#include "sif_registers.h"
#include "ee_sif_dma.h"
#include <iostream>
#include <algorithm>
#include <vector>
void checkSif0();
void checkPioCapture();

namespace {
void require(bool value) { if (!value) throw std::runtime_error("Bootstrap interface check failed"); }
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unsupported interface operation was accepted");
}
struct WordBus {
    uint32_t value{0xcafebabe}, lastAddress{};
    uint32_t read32(uint32_t) const { return value; }
    void write32(uint32_t address, uint32_t word) { lastAddress = address; value = word; }
};
}
int main() {
    try {
        checkSif0();
        checkPioCapture();
        refuses([] { ReadOnlyRom wrong(std::vector<uint8_t>(16)); });
        std::vector<uint8_t> bytes(0x400000);
        bytes[0] = 0x78; bytes[1] = 0x56; bytes[2] = 0x34; bytes[3] = 0x92;
        bytes.back() = 0xa5;
        ReadOnlyRom rom(std::move(bytes));
        require(!rom.ownsCode(0xbfc4a000));
        rom.enableIopBootCode();
        refuses([&] { rom.enableIopBootCode(); });
        require(rom.ownsCode(0xbfc4a000) && rom.ownsCode(0xbfc4b11c));
        for (auto address : {0xbfc49ffcu, 0xbfc4b120u, 0xbfc4a001u, 0x1fc4a000u, 0x9fc4a000u, 0xffc4a000u})
            require(!rom.ownsCode(address));
        WordBus bus;
        for (uint32_t address : {0x1fc00000u, 0x9fc00000u, 0xbfc00000u}) {
            const auto old = rom.stage(bus, address, 0x23);
            require(bus.value == 0x92345678 && bus.lastAddress == address);
            bus.write32(old.first, old.second); require(bus.value == 0xcafebabe);
        }
        const auto old = rom.stage(bus, 0xbfffffff, 0x24);
        require(bus.value == 0xa5000000 && old.first == 0xbffffffc);
        for (uint32_t address : {0x1fbfffffu, 0x20000000u, 0x7fc00000u, 0xc0000000u, 0xfffe0130u})
            refuses([&] { rom.stage(bus, address, 0x23); });
        refuses([&] { rom.stage(bus, 0xbfc00000, 0x2b); });
        SifRegisters sif;
        require(SifRegisters::kernelAddress(1) == 0x1000f200 && SifRegisters::kernelAddress(2) == 0x1000f210 &&
                SifRegisters::kernelAddress(3) == 0x1000f220 && SifRegisters::kernelAddress(4) == 0x1000f230);
        refuses([] { SifRegisters::kernelAddress(0); });
        refuses([] { SifRegisters::kernelAddress(0x80000000); });
        sif.write(0x1d000000, 0xffffffff); sif.write(0x1d000010, 0x12345678);
        sif.write(0x1d000030, 0x10000); sif.write(0x1d000030, 0x40000);
        sif.write(0x1d000020, 0xffffffff);
        require(!sif.read(0x1d000000) && sif.read(0x1d000010) == 0x12345678 &&
                sif.read(0x1d000030) == 0x50000 && !sif.read(0x1d000020));
        sif.write(0x1d000040, 0x20); require(sif.read(0x1d000040) == 0xf0002022);
        sif.write(0x1d000040, 0x20); require(sif.read(0x1d000040) == 0xf0002002);
        sif.write(0x1d000060, 0xffffffff); require(!sif.read(0x1d000060));
        refuses([&] { sif.read(0x1d000050); });
        require(!sif.allows(0x1d000000, 0x25));
        sif.writeEe(0x1000f200, 0x123400); require(sif.read(0x1d000000) == 0x123400);
        sif.writeEe(0x1000f220, 0x10000); sif.writeEe(0x1000f220, 0x40000);
        sif.write(0x1d000020, 0x10000); require(sif.readEe(0x1000f220) == 0x40000);
        sif.writeEe(0x1000f230, 0x10000); require(sif.read(0x1d000030) == 0x40000);
        sif.writeEe(0x1000f240, 0x100); require(sif.read(0x1d000040) == 0xf0002102);
        refuses([&] { sif.writeEe(0x1000f240, 1u << 18); });
        require(sif.readEe(0x1000f240) == 0xf0002102); // Rejected strobe has no side effect.
        sif.writeEe(0x1000f240, 0);
        require(sif.read(0x1d000040) == 0xf0002002 && sif.readEe(0x1000f240) == 0xf0002102);
        refuses([&] { sif.writeEe(0x1000f210, 0x1234); });
        sif.writeEe(0x1000f260, 0xff);
        require(sif.readEe(0x1000f260) == 0xff && sif.read(0x1d000060) == 0);
        sif.write(0x1d000060, 0xffffffff); require(sif.readEe(0x1000f260) == 0);
        refuses([&] { sif.readEe(0x1000f250); });
        EeSifDma receiver;
        receiver.write(0x1000c000, 0x184);
        require(receiver.read(0x1000c000) == 0x184 && receiver.read(0x1000c000) == 0x184 &&
                receiver.read(0x1000c020) == 0); // Reading never completes an empty receiver.
        receiver.write(0x1000c430, 0x212c0); require(receiver.read(0x1000c430) == 0x212c0);
        refuses([&] { receiver.write(0x1000c400, 0x184); });
        require(receiver.read(0x1000c400) == 0);
        refuses([&] { receiver.write(0x1000c000, 0x101); });
        refuses([&] { receiver.read(0x1000c040); });
        std::vector<uint8_t> dmaRam(0x2000), delivered;
        const auto word = [&](size_t at, uint32_t value) {
            for (unsigned n = 0; n < 4; ++n) dmaRam[at + n] = uint8_t(value >> (8 * n));
        };
        for (unsigned n = 0; n < 48; ++n) dmaRam[0x400 + n] = uint8_t(n + 1);
        word(0x100, 3); word(0x104, 0x400); // REFE, three actual quadwords.
        EeSifDma sender; sender.seedEnableWord(0x1201); sender.enableSif1();
        require(sender.read(0x1000f520) == 0x1201);
        refuses([&] { sender.seedEnableWord(0); });
        sender.write(0x1000c430, 0x100); sender.write(0x1000c400, 0x184);
        require(!sender.takeCompletion());
        sender.pumpSif1(dmaRam, [&](auto quantum) { delivered.insert(delivered.end(), quantum.begin(), quantum.end()); });
        require(delivered.size() == 48 && std::equal(delivered.begin(), delivered.end(), dmaRam.begin() + 0x400));
        require(sender.read(0x1000c410) == 0x430 && sender.read(0x1000c420) == 0 &&
                sender.read(0x1000c430) == 0x110 && !(sender.read(0x1000c400) & 0x100));
        require(sender.takeCompletion() && !sender.takeCompletion());
        word(0x100, 0x30000001); word(0x110, 0x20000000); word(0x114, 0x150);
        word(0x150, 1); word(0x154, 0x420); // REF -> zero-length NEXT -> REFE.
        delivered.clear(); sender.write(0x1000c430, 0x100); sender.write(0x1000c400, 0x184);
        sender.pumpSif1(dmaRam, [&](auto quantum) { delivered.insert(delivered.end(), quantum.begin(), quantum.end()); });
        require(delivered.size() == 32 && delivered.front() == 1 && delivered[16] == 33 &&
                sender.read(0x1000c430) == 0x160 && sender.takeCompletion());
        word(0x100, 0x10000001); // Unsupported CNT cannot silently turn into REFE.
        sender.write(0x1000c430, 0x100); sender.write(0x1000c400, 0x184);
        unsigned calls = 0;
        refuses([&] { sender.pumpSif1(dmaRam, [&](auto) { ++calls; }); });
        require(!calls && !sender.takeCompletion() && (sender.read(0x1000c400) & 0x100));
        sender.write(0x1000c400, 0); word(0x100, 2); word(0x104, 0x1ff0); // Truncated source.
        sender.write(0x1000c400, 0x184);
        refuses([&] { sender.pumpSif1(dmaRam, [&](auto) { ++calls; }); });
        require(!calls && !sender.takeCompletion());
        sender.write(0x1000c400, 0); word(0x100, 0x20000000); word(0x104, 0x100);
        sender.write(0x1000c430, 0x100); sender.write(0x1000c400, 0x184);
        refuses([&] { sender.pumpSif1(dmaRam, [&](auto) { ++calls; }); });
        require(!calls && !sender.takeCompletion()); // Bounded cyclic tags do not complete.
        sender.write(0x1000c400, 0); word(0x100, 1); word(0x104, 0x400);
        sender.write(0x1000f590, 0x10000); require(sender.read(0x1000f520) == 0x10000);
        sender.write(0x1000c430, 0x100); sender.write(0x1000c400, 0x184);
        refuses([&] { sender.pumpSif1(dmaRam, [&](auto) { ++calls; }); });
        require(!calls && !sender.takeCompletion());
        sender.write(0x1000c400, 0); sender.write(0x1000f590, 0x1201);
        sender.write(0x1000c430, 0x100); sender.write(0x1000c400, 0x184);
        refuses([&] { sender.pumpSif1(dmaRam, [&](auto) { throw std::runtime_error("receiver not ready"); }); });
        require(!sender.takeCompletion() && sender.read(0x1000c420) == 1 &&
                (sender.read(0x1000c400) & 0x100));
        std::cout << "Read-only ROM, directional SIF, source chains and failure bounds passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
