#include "../iop_probe/cdvd_dma.h"
#include "cdvd_device.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

namespace {
void check(bool value) { if (!value) throw std::runtime_error("CDVD DMA3 contract check failed"); }
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unsupported CDVD DMA3 operation accepted");
}
struct Memory {
    static constexpr uint32_t RamSize = 0x10000;
    std::vector<uint8_t> ram = std::vector<uint8_t>(RamSize, 0xcd);
    std::map<uint32_t, uint32_t> words;
    uint32_t irq{};
    uint32_t read32(uint32_t address) const {
        const auto at = words.find(address); return at == words.end() ? 0 : at->second;
    }
    void write32(uint32_t address, uint32_t value) { words[address] = value; }
    bool writeRam(uint32_t address, const void* source, size_t size) {
        if (address > RamSize || size > RamSize - address) return false;
        std::memcpy(ram.data() + address, source, size); return true;
    }
    uint32_t interruptStatus() const { return irq; }
    void setInterruptStatus(uint32_t value) { irq = value; }
};
Memory configured(unsigned sectors = 1) {
    Memory m;
    m.write32(0x1f8010b0, 0x1000); m.write32(0x1f8010b4, (sectors * 16u << 16) | 32);
    m.write32(0x1f801578, 1); m.write32(0x1f8010f0, 0x8000);
    m.write32(0x1f8010f4, 0x00880000);
    return m;
}
void arm(Memory& m) {
    CdvdDma::validateStart(m, 0x41000200); m.write32(0x1f8010b8, 0x41000200);
}
void read(drakengard::CdvdDevice& drive, unsigned count) {
    const std::array<uint8_t, 11> parameters{24, 0, 0, 0, uint8_t(count), 0, 0, 0, 0, 0, 0};
    for (auto byte : parameters) drive.writeRegister(5, byte);
    drive.writeRegister(4, 6);
}
}

void checkCdvdDma(drakengard::IsoMedia& image, std::span<const uint8_t> original) {
    {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured(2);
        arm(m); const auto before = m.words;
        dma.pump(m, drive);
        check(m.words == before && !m.irq && !drive.takeInterruptEdge() && !dma.transferredBytes());
        refuses([&] { CdvdDma::validateStart(m, 0x41000200); });
        read(drive, 2);
        m.write32(0x1f801578, 0); dma.pump(m, drive);
        check(drive.pendingBytes() == 4096 && !dma.transferredBytes());
        m.write32(0x1f801578, 1); m.write32(0x1f8010f0, 0); dma.pump(m, drive);
        check(drive.pendingBytes() == 4096 && m.read32(0x1f8010b0) == 0x1000 && !m.irq);
        m.write32(0x1f8010f0, 0x8000); dma.pump(m, drive);
        check(drive.pendingBytes() == 2048 && m.read32(0x1f8010b0) == 0x1800 &&
            m.read32(0x1f8010b4) == 0x00100020 && m.read32(0x1f8010b8) == 0x41000200 &&
            !m.irq && !drive.takeInterruptEdge() && !dma.completedTransfers());
        dma.pump(m, drive);
        check(!drive.pendingBytes() && drive.actualSectorsRead() == 2 && drive.takeInterruptEdge() &&
            m.read32(0x1f8010b0) == 0x2000 && m.read32(0x1f8010b4) == 32 &&
            m.read32(0x1f8010b8) == 0x40000200 && m.read32(0x1f8010f4) == 0x88880000 &&
            m.irq == 8 && dma.transferredBytes() == 4096 && dma.completedTransfers() == 1);
        check(std::equal(m.ram.begin() + 0x1000, m.ram.begin() + 0x2000, original.begin() + 24 * 2048));
        check(m.ram[0xfff] == 0xcd && m.ram[0x2000] == 0xcd);
        const auto done = m.words; dma.pump(m, drive);
        check(m.words == done && !drive.takeInterruptEdge() && dma.completedTransfers() == 1);
    }
    // Each DMA completion is independent of the whole drive command. Channel
    // masking follows the pinned ICR profile; the existing INTC bits survive.
    for (uint32_t icr : {0u, 0x00080000u, 0x00880000u}) {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured();
        m.write32(0x1f8010f4, icr); m.irq = 0x40; read(drive, 2); arm(m); dma.pump(m, drive);
        check(drive.pendingBytes() == 2048 && !drive.takeInterruptEdge() && dma.completedTransfers() == 1);
        const auto expected = icr ? icr | 0x08000000u | ((icr & 0x00800000u) ? 0x80000000u : 0) : 0;
        check(m.read32(0x1f8010f4) == expected && m.irq == (icr ? 0x48 : 0x40));
        m.write32(0x1f8010b4, 0x00100020); arm(m); dma.pump(m, drive);
        check(!drive.pendingBytes() && drive.takeInterruptEdge() && dma.completedTransfers() == 2);
    }
    // Reject malformed ranges and modes before consuming any real sector.
    for (auto [reg, value] : std::map<uint32_t, uint32_t>{
        {0x1f8010b0, Memory::RamSize - 4}, {0x1f8010b4, 0x00010020}, {0x1f8010b8, 0x41000201}}) {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured(); read(drive, 1);
        arm(m); m.write32(reg, value); const auto before = m.words;
        refuses([&] { dma.pump(m, drive); });
        check(m.words == before && drive.pendingBytes() == 2048 && !m.irq &&
            !drive.takeInterruptEdge() && !dma.transferredBytes());
    }
    for (uint32_t address : {0x1001u, 0x80001000u, 0xffffff00u}) {
        auto m = configured(); m.write32(0x1f8010b0, address);
        refuses([&] { CdvdDma::validateStart(m, 0x41000200); });
    }
    for (uint32_t bcr : {0u, 32u, 0x00100000u, 0x00100010u, 0xffff0020u}) {
        auto m = configured(); m.write32(0x1f8010b4, bcr);
        refuses([&] { CdvdDma::validateStart(m, 0x41000200); });
    }
    {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured();
        arm(m); read(drive, 1); m.write32(0x1f801578, 2);
        refuses([&] { dma.pump(m, drive); });
        check(drive.pendingBytes() == 2048 && !dma.transferredBytes() && !m.irq);
    }
}
