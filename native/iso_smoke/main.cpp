#include "iso_media.h"
#include "cdvd_rpc.h"
#include "cdvd_device.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;
constexpr size_t Sector = 2048;
void check(bool value) { if (!value) throw std::runtime_error("ISO synthetic assertion failed"); }
void both(Bytes& bytes, size_t offset, uint32_t value) {
    for (unsigned n = 0; n < 4; ++n) {
        bytes[offset + n] = uint8_t(value >> (8 * n));
        bytes[offset + 4 + n] = uint8_t(value >> (8 * (3 - n)));
    }
}
Bytes record(const std::string& name, uint32_t lsn, uint32_t size, bool directory) {
    Bytes result((33 + name.size() + 1) & ~size_t(1), 0);
    result[0] = uint8_t(result.size()); result[25] = directory ? 2 : 0;
    both(result, 2, lsn); both(result, 10, size);
    result[28] = result[31] = 1;
    result[32] = uint8_t(name.size()); std::copy(name.begin(), name.end(), result.begin() + 33);
    return result;
}
void append(Bytes& image, size_t& offset, const Bytes& record) {
    std::copy(record.begin(), record.end(), image.begin() + offset); offset += record.size();
}
Bytes fixture() {
    Bytes result(32 * Sector, 0);
    for (const auto [sector, type] : {std::pair{16, 1}, std::pair{17, 255}}) {
        result[sector * Sector] = uint8_t(type);
        std::memcpy(result.data() + sector * Sector + 1, "CD001", 5);
        result[sector * Sector + 6] = 1;
    }
    both(result, 16 * Sector + 80, 32);
    result[16 * Sector + 129] = result[16 * Sector + 130] = 8;
    size_t offset = 16 * Sector + 156;
    append(result, offset, record(std::string(1, 0), 20, Sector, true));
    offset = 20 * Sector;
    append(result, offset, record(std::string(1, 0), 20, Sector, true));
    append(result, offset, record(std::string(1, 1), 20, Sector, true));
    append(result, offset, record("TEST.BIN;1", 24, 6, false));
    append(result, offset, record("SUBDIR", 21, Sector, true));
    offset = 21 * Sector;
    append(result, offset, record(std::string(1, 0), 21, Sector, true));
    append(result, offset, record(std::string(1, 1), 20, Sector, true));
    append(result, offset, record("SECOND.DAT;1", 25, 3, false));
    std::memcpy(result.data() + 24 * Sector, "abc123", 6);
    std::memcpy(result.data() + 25 * Sector, "xyz", 3);
    return result;
}
void write(const char* path, const Bytes& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()), data.size()); check(bool(file));
}
void rejects(const Bytes& data) {
    write("iso-invalid-fixture.iso", data);
    try { drakengard::IsoMedia media("iso-invalid-fixture.iso"); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid ISO accepted");
}
struct CdvdHost : drakengard::CdvdHost {
    Bytes ram = Bytes(65536, 0x55);
    bool validEe(uint32_t address, size_t bytes) override { return address <= ram.size() && bytes <= ram.size() - address; }
    void writeEe(uint32_t address, const void* data, size_t bytes) override {
        check(validEe(address, bytes)); std::memcpy(ram.data() + address, data, bytes);
    }
};
Bytes words(std::initializer_list<uint32_t> values) {
    Bytes bytes;
    for (uint32_t value : values)
        for (unsigned n = 0; n < 4; ++n) bytes.push_back(uint8_t(value >> (n * 8)));
    return bytes;
}
}

int main() {
    try {
        const auto image = fixture(); write("iso-fixture.iso", image);
        drakengard::IsoMedia media("iso-fixture.iso");
        check(media.sectors() == 32 && media.fileCount() == 2);
        auto file = media.find("cdrom0:\\test.bin;1");
        check(file && file->lsn == 24 && file->size == 6);
        auto nested = media.find("/subdir/second.dat");
        check(nested && nested->lsn == 25 && nested->size == 3);
        check(!media.find("../TEST.BIN") && !media.find("TEST.BIN;2") && !media.find("MISSING.BIN"));
        std::array<uint8_t, Sector * 2> data{};
        check(media.readSectors(24, 2, data));
        check(std::memcmp(data.data(), "abc123", 6) == 0 && std::memcmp(data.data() + Sector, "xyz", 3) == 0);
        data.fill(0x55); check(!media.readSectors(31, 2, data));
        check(std::all_of(data.begin(), data.end(), [](uint8_t byte) { return byte == 0x55; }));
        check(!media.readSectors(24, 1, data) && media.readSectors(32, 0, {}));
        std::ifstream original("iso-fixture.iso", std::ios::binary);
        const Bytes after((std::istreambuf_iterator<char>(original)), std::istreambuf_iterator<char>());
        check(after == image);
        drakengard::CdvdRpc drive(media); CdvdHost host;
        std::array<uint8_t, 16> response{};
        check(drive.call(0x80000592, 0, words({0}), response, host));
        check(response[0] == 1 && response[4] == 0 && response[8] == 0);
        check(drive.call(0x8000059a, 0, words({0}), response, host) && response[0] == 2);
        check(drive.call(0x8000059c, 0, words({1}), response, host) && response[0] == 2);
        check(drive.call(0x80000593, 3, {}, response, host) && response[0] == 0x14);
        check(drive.call(0x80000595, 1, words({24, 2, 0x4001, 0, 0x3000, 0x3100}), {}, host));
        check(std::memcmp(host.ram.data() + 0x4001, "abc123", 6) == 0);
        check(std::memcmp(host.ram.data() + 0x4001 + Sector, "xyz", 3) == 0);
        check(host.ram[0x4000] == 0x55 && host.ram[0x5001] == 0x55);
        check(host.ram[0x3100] == 0 && host.ram[0x3101] == 0x10 && drive.readCount() == 1 && drive.sectorsRead() == 2);
        check(std::all_of(host.ram.begin() + 0x3000, host.ram.begin() + 0x3010, [](uint8_t value) { return value == 0; }));
        std::fill(host.ram.begin() + 0x4001, host.ram.begin() + 0x5001, 0x55);
        check(drive.call(0x80000595, 1, words({31, 2, 0x4001, 0, 0x3000, 0x3100}), {}, host));
        check(drive.call(0x80000593, 4, {}, response, host) && response[0] == 0x20);
        check(host.ram[0x4001] == 0x55 && drive.readCount() == 1 && host.ram[0x3101] == 0);
        Bytes search(292, 0); std::memcpy(search.data() + 32, "TEST.BIN;1", 10);
        auto address = words({0x2000}); std::copy(address.begin(), address.end(), search.begin() + 288);
        check(drive.call(0x80000597, 0, search, response, host) && response[0] == 1);
        check(host.ram[0x2000] == 24 && host.ram[0x2004] == 6 && std::memcmp(host.ram.data() + 0x2008, "TEST.BIN;1", 10) == 0);
        check(!drive.call(0x80000593, 1, {}, response, host)); // No invented RTC or TOC.
        check(!drive.call(0x80000595, 4, {}, response, host));
        drakengard::CdvdDevice device(media, std::array<uint8_t, 4>{1, 2, 3, 4});
        for (unsigned i = 0; i < 32; ++i) check(device.readRegister(0x18) == 0);
        check(device.readRegister(0x17) == 0x40 && !device.takeInterruptEdge());
        check(device.readRegister(5) == 0x40 && device.readRegister(0x0f) == 0x14);
        const auto request = words({24, 2});
        for (auto byte : request) device.writeRegister(5, byte);
        for (auto byte : {0, 0, 0}) device.writeRegister(5, uint8_t(byte));
        device.writeRegister(4, 6);
        check(device.pendingBytes() == 4096 && device.actualSectorsRead() == 2);
        check(device.readRegister(5) == 0x80 && device.readRegister(8) == 0 && !device.takeInterruptEdge());
        // Cancelling power readiness cannot finish an unrelated sector read.
        device.writeRegister(0x16, 0x1b);
        check(device.readRegister(0x17) == 0 && device.readRegister(0x18) == 0);
        check(device.readRegister(0x17) == 0x40 && device.pendingBytes() == 4096 &&
              device.actualSectorsRead() == 2 && device.readRegister(5) == 0x80 &&
              device.readRegister(8) == 0 && !device.takeInterruptEdge());
        std::array<uint8_t, 2048> one{};
        check(device.drainUserSectors(one) == 2048 && std::memcmp(one.data(), "abc123", 6) == 0);
        check(device.pendingBytes() == 2048 && !device.takeInterruptEdge());
        check(device.drainUserSectors(one) == 2048 && std::memcmp(one.data(), "xyz", 3) == 0);
        check(device.pendingBytes() == 0 && device.readRegister(5) == 0x40 && device.takeInterruptEdge());
        check(!device.takeInterruptEdge() && device.readRegister(8) == 1);
        device.writeRegister(0x16, 0x1b);
        check(device.readRegister(0x18) == 0 && device.readRegister(8) == 1 && !device.takeInterruptEdge());
        device.writeRegister(8, 1); check(device.readRegister(8) == 0);
        for (unsigned repeat = 0; repeat < 3; ++repeat) {
            device.writeRegister(0x16, 0x1b);
            check(device.readRegister(0x16) == 0x1b && device.readRegister(0x17) == 0);
            bool unreadRejected = false;
            try { device.writeRegister(0x16, 0x1b); } catch (const std::runtime_error&) { unreadRejected = true; }
            check(unreadRejected && device.readRegister(0x18) == 0 && device.readRegister(0x17) == 0x40);
        }
        device.writeRegister(0x17, 0); device.writeRegister(0x16, 3);
        for (uint8_t expected : {1, 2, 3, 4}) check(device.readRegister(0x18) == expected);
        unsigned clockReads = 0;
        device.setClockSource([&] { ++clockReads; return drakengard::CdvdCalendar{2000, 2, 29, 23, 59, 58}; });
        device.writeRegister(0x16, 8);
        check(clockReads == 1 && device.readRegister(0x17) == 0);
        for (uint8_t expected : {0, 0x58, 0x59, 0x23, 0, 0x29, 2, 0})
            check(device.readRegister(0x18) == expected);
        check(device.readRegister(0x17) == 0x40 && !device.takeInterruptEdge());
        device.setClockSource([] { return drakengard::CdvdCalendar{2025, 2, 29, 0, 0, 0}; });
        bool badDate = false;
        try { device.writeRegister(0x16, 8); } catch (const std::runtime_error&) { badDate = true; }
        check(badDate && device.readRegister(0x17) == 0x40 && !device.takeInterruptEdge());
        check(device.readRegister(0x17) == 0x40);
        for (unsigned i = 0; i < 32; ++i) check(device.readRegister(0x18) == 0);
        check(device.readRegister(0x17) == 0x40 && !device.takeInterruptEdge());
        device.writeRegister(0x17, 0); device.writeRegister(0x16, 3);
        for (uint8_t expected : {1, 2, 3, 4}) check(device.readRegister(0x18) == expected);
        bool rawRejected = false;
        try { device.writeRegister(4, 8); } catch (const std::runtime_error&) { rawRejected = true; }
        check(rawRejected && !device.takeInterruptEdge());
        drakengard::CdvdDevice malformedPower(media);
        malformedPower.writeRegister(0x17, 1);
        bool powerArgsRejected = false;
        try { malformedPower.writeRegister(0x16, 0x1b); } catch (const std::runtime_error&) { powerArgsRejected = true; }
        check(powerArgsRejected && malformedPower.readRegister(0x17) == 0x40 && !malformedPower.takeInterruptEdge());
        drakengard::CdvdDevice powerOff(media);
        bool powerOffRejected = false;
        try { powerOff.writeRegister(0x16, 0x0f); } catch (const std::runtime_error&) { powerOffRejected = true; }
        check(powerOffRejected && powerOff.readRegister(0x17) == 0x40 && !powerOff.takeInterruptEdge());
        auto bad = image; bad[16 * Sector + 84] ^= 1; rejects(bad);
        bad = image; bad[16 * Sector + 129] = 4; rejects(bad);
        bad = image; bad[16 * Sector + 156 + 1] = 1; rejects(bad);
        bad = image; both(bad, 16 * Sector + 156 + 2, 32); rejects(bad);
        bad = image; bad[20 * Sector] = 2; rejects(bad);
        bad = image; bad.pop_back(); rejects(bad);
        bad = image; bad[16 * Sector + 156 + 25] |= 0x80; rejects(bad);
        std::cout << "ISO directories, actual LSNs, bytes, read-only and malformed-media checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
