#include "read_only_rom.h"
#include "sif_registers.h"
#include <iostream>

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
        refuses([] { ReadOnlyRom wrong(std::vector<uint8_t>(16)); });
        std::vector<uint8_t> bytes(0x400000);
        bytes[0] = 0x78; bytes[1] = 0x56; bytes[2] = 0x34; bytes[3] = 0x92;
        bytes.back() = 0xa5;
        ReadOnlyRom rom(std::move(bytes));
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
        std::cout << "Read-only ROM bounds/aliases and directional SIF registers passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
