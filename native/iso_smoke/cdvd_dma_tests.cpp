#include "../iop_probe/cdvd_dma.h"
#include "cdvd_device.h"
#include "dvd_frame.h"
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
void read(drakengard::CdvdDevice& drive, unsigned count, uint8_t command = 6) {
    const std::array<uint8_t, 11> parameters{24, 0, 0, 0, uint8_t(count), 0, 0, 0, 0, 0, 0};
    for (auto byte : parameters) drive.writeRegister(5, byte);
    drive.writeRegister(4, command);
}
}

void checkCdvdDma(drakengard::IsoMedia& image, std::span<const uint8_t> original) {
    {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured();
        m.write32(0x1f8010b4, 0x00810004); arm(m);
        drive.writeRegister(5, 0); drive.writeRegister(4, 9);
        check(drive.pendingToc() && drive.pendingBytes() == 2048 && !drive.actualSectorsRead() &&
            !drive.takeInterruptEdge());
        std::array<uint8_t, 2048> toc;
        refuses([&] { drive.drainUserSectors(toc); });
        dma.pump(m, drive);
        check(!drive.pendingToc() && !drive.pendingBytes() && drive.takeInterruptEdge() &&
            !drive.actualSectorsRead() && m.read32(0x1f8010b0) == 0x1000 &&
            m.read32(0x1f8010b4) == 0x00810004 && m.read32(0x1f8010b8) == 0x40000200 &&
            dma.transferredBytes() == 2048 && dma.completedTransfers() == 1 && m.irq == 8);
        std::array<uint8_t, 2048> expected{};
        for (auto [at, byte] : std::map<unsigned, uint8_t>{{0,4},{1,2},{2,0xf2},{4,0x86},{5,0x72},
            {12,1},{13,2},{14,1},{17,3},{21,3},{23,31}}) expected[at] = byte;
        check(std::equal(expected.begin(), expected.end(), m.ram.begin() + 0x1000));
        check(m.ram[0xfff] == 0xcd && std::all_of(m.ram.begin() + 0x1800, m.ram.begin() + 0x1810,
            [](auto byte) { return byte == 0xcd; }));
        dma.pump(m, drive); check(dma.completedTransfers() == 1 && !drive.takeInterruptEdge());
    }
    {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured(); arm(m);
        drive.writeRegister(5, 0); drive.writeRegister(4, 9);
        refuses([&] { dma.pump(m, drive); });
        check(drive.pendingToc() && drive.pendingBytes() == 2048 && !m.irq && !dma.transferredBytes());
    }
    {
        drakengard::CdvdDevice drive(image); drive.writeRegister(5, 1);
        refuses([&] { drive.writeRegister(4, 9); });
        check(!drive.pendingBytes() && !drive.actualSectorsRead() && !drive.takeInterruptEdge());
    }
    {
        std::array<uint8_t, 2048> user;
        for (size_t n = 0; n < user.size(); ++n) user[n] = uint8_t(n * 31 + 7);
        std::array<uint8_t, 2064> frame;
        frame.fill(0xcd); drakengard::referenceDvdFrame(0xff, user, frame);
        check(frame[0] == 0x20 && frame[1] == 3 && frame[2] == 0 && frame[3] == 0xff);
        check(std::equal(user.begin(), user.end(), frame.begin() + 12));
        check(std::all_of(frame.begin() + 4, frame.begin() + 12, [](auto b) { return b == 0; }) &&
            std::all_of(frame.begin() + 2060, frame.end(), [](auto b) { return b == 0; }));
        drakengard::referenceDvdFrame(0x100, user, frame);
        check(frame[1] == 3 && frame[2] == 1 && frame[3] == 0);
        const auto saved = frame;
        refuses([&] { drakengard::referenceDvdFrame(0xfd0000, user, frame); });
        refuses([&] { drakengard::referenceDvdFrame(0, std::span(user).first(2047), frame); });
        check(frame == saved);
    }
    for (uint32_t words : {4u, 12u, 129u, 516u}) {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured();
        m.write32(0x1f8010b4, ((1032 / words) << 16) | words); arm(m); read(drive, 2, 8);
        check(drive.dmaSectorSize() == 2064 && drive.pendingBytes() == 4128);
        std::array<uint8_t, 2048> user;
        refuses([&] { drive.drainUserSectors(user); });
        check(drive.pendingBytes() == 4128);
        dma.pump(m, drive);
        check(m.read32(0x1f8010b4) == ((516 / words) << 16 | words) && m.read32(0x1f8010b0) == 0x1810 &&
            m.read32(0x1f8010b8) == 0x41000200 && !drive.takeInterruptEdge() && !m.irq);
        dma.pump(m, drive);
        check(m.read32(0x1f8010b4) == words && m.read32(0x1f8010b0) == 0x2020 &&
            m.read32(0x1f8010b8) == 0x40000200 && drive.takeInterruptEdge() &&
            dma.transferredBytes() == 4128 && dma.completedTransfers() == 1 && drive.actualSectorsRead() == 2);
        for (unsigned n = 0; n < 2; ++n) {
            const auto at = 0x1000 + n * 2064;
            check(m.ram[at] == 0x20 && m.ram[at + 1] == 3 && m.ram[at + 2] == 0 && m.ram[at + 3] == 24 + n);
            check(std::equal(original.begin() + (24 + n) * 2048, original.begin() + (25 + n) * 2048,
                m.ram.begin() + at + 12));
            for (auto j : {4, 5, 6, 7, 8, 9, 10, 11, 2060, 2061, 2062, 2063}) check(m.ram[at + j] == 0);
        }
        check(m.ram[0xfff] == 0xcd && m.ram[0x2020] == 0xcd);
    }
    {
        drakengard::CdvdDevice drive(image); CdvdDma dma; auto m = configured();
        m.write32(0x1f8010b4, 0x00810004); arm(m); read(drive, 1);
        refuses([&] { dma.pump(m, drive); });
        check(drive.pendingBytes() == 2048 && !dma.transferredBytes() && !m.irq);
    }
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
