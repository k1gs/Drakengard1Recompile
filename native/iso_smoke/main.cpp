#include "iso_media.h"
#include "cdvd_rpc.h"
#include "cdvd_device.h"
#include "virtual_card_auth.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
void checkCdvdDma(drakengard::IsoMedia&, std::span<const uint8_t>);

namespace {
using Bytes = std::vector<uint8_t>;
constexpr size_t Sector = 2048;
void check(bool value) { if (!value) throw std::runtime_error("ISO synthetic assertion failed"); }
#ifdef _WIN32
void checkVirtualCardAuth(drakengard::IsoMedia& media) {
    drakengard::VirtualCardAuth auth;
    drakengard::CdvdDevice cdvd(media);
    cdvd.bindCardAuth(auth);
    const auto scmd = [&](uint8_t command, std::span<const uint8_t> parameters = {}) {
        for (auto byte : parameters) cdvd.writeRegister(0x17, byte);
        cdvd.writeRegister(0x16, command);
        Bytes response;
        while (cdvd.readRegister(0x17) == 0) {
            check(response.size() < 16);
            response.push_back(cdvd.readRegister(0x18));
        }
        check(!cdvd.takeInterruptEdge());
        return response;
    };
    const auto card = [&](uint8_t mode, std::span<const uint8_t> payload = {}, bool read = false) {
        Bytes request(read || !payload.empty() ? 14 : 5);
        request[0] = 0x81;
        request[1] = 0xf0;
        request[2] = mode;
        if (!payload.empty()) {
            check(payload.size() == 8);
            std::reverse_copy(payload.begin(), payload.end(), request.begin() + 3);
            for (auto byte : payload) request[11] ^= byte;
        }
        return auth.serial(request, request.size(), 0x55);
    };
    const auto readBlock = [&](uint8_t mode) {
        const auto response = card(mode, {}, true);
        check(response[3] == 0x2b && response[13] == 0x55);
        uint8_t checksum{};
        Bytes data(response.rbegin() + 2, response.rend() - 4);
        for (auto byte : data) checksum ^= byte;
        check(data.size() == 8 && checksum == response[12]);
        return data;
    };
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        auth.reset();
        const std::array<uint8_t, 1> slot{0};
        check(scmd(0x88) == Bytes{0x80});
        check(scmd(0x80, slot) == Bytes{0});
        check(auth.serial(std::array<uint8_t, 5>{0x81, 0xf7, 1, 0, 0}, 5, 0x55).back() == 0x55);
        check(card(0).back() == 0x55);
        check(scmd(0x81, slot) == Bytes{0});
        auto material = readBlock(1);
        const auto seed = readBlock(2);
        material.insert(material.end(), seed.begin(), seed.end());
        check(scmd(0x82, material) == Bytes{0});
        check(card(3).back() == 0x55);
        check(scmd(0x83, readBlock(4)) == Bytes{0});
        check(card(5).back() == 0x55 && scmd(0x8f) == Bytes{0});
        const auto first = scmd(0x84);
        const auto second = scmd(0x85);
        check(first.size() == 13 && second.size() == 13 && first[0] == 0 && second[0] == 0);
        Bytes challenge1(first.begin() + 1, first.begin() + 9);
        Bytes challenge2(first.begin() + 9, first.end());
        challenge2.insert(challenge2.end(), second.begin() + 1, second.begin() + 5);
        check(card(6, std::span(second).subspan(5)).back() == 0x55);
        check(card(7, challenge2).back() == 0x55);
        for (uint8_t mode : {8, 9, 10}) check(card(mode).back() == 0x55);
        if (attempt == 1) challenge1[0] ^= 1;
        check(card(11, challenge1).back() == 0x55);
        if (attempt == 1) {
            check(card(12).back() == 0x66 && scmd(0x88) == Bytes{0x80});
            continue;
        }
        for (uint8_t mode : {12, 13, 14}) check(card(mode).back() == 0x55);
        auto responses = readBlock(15);
        check(card(16).back() == 0x55);
        const auto response2 = readBlock(17);
        responses.insert(responses.end(), response2.begin(), response2.end());
        if (attempt == 2) responses[0] ^= 1;
        check(scmd(0x86, responses) == Bytes{0});
        check(card(18).back() == 0x55);
        check(scmd(0x87, readBlock(19)) == Bytes{0});
        check(scmd(0x8f) == Bytes{uint8_t(attempt == 0 ? 0 : 0x80)});
        if (attempt == 0) check(card(20).back() == 0x55);
        check(scmd(0x88) == Bytes{uint8_t(attempt == 0 ? 0 : 0x80)});
        check(scmd(0x88) == Bytes{0x80});
    }
}
#endif
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
        checkCdvdDma(media, image);
        {
            auto trailing = image; trailing.resize(image.size() + Sector);
            write("iso-trailing-fixture.iso", trailing);
            drakengard::IsoMedia extra("iso-trailing-fixture.iso");
            check(!extra.singleVolumeImage() && media.singleVolumeImage());
            drakengard::CdvdDevice drive(extra); drive.writeRegister(5, 0);
            bool refused = false;
            try { drive.writeRegister(4, 9); } catch (const std::runtime_error&) { refused = true; }
            check(refused && !drive.pendingBytes() && !drive.takeInterruptEdge());
        }
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
        check(device.readRegister(0x13) == 0x12);
        device.writeRegister(6, 0x80);
        device.writeRegister(6, 0x84);
        check(device.readRegister(6) == 0 && device.pendingBytes() == 4096 &&
              device.actualSectorsRead() == 2 && !device.takeInterruptEdge());
        for (unsigned value = 0; value < 256; ++value) device.writeRegister(6, uint8_t(value));
        check(device.readRegister(6) == 0 && device.pendingBytes() == 4096);
        check(device.readRegister(5) == 0x80 && device.readRegister(8) == 0 && !device.takeInterruptEdge());
        device.writeRegister(0x16, 5);
        check(device.readRegister(0x0b) == 0 && device.readRegister(0x17) == 0);
        bool trayUnreadRejected = false;
        try { device.writeRegister(0x16, 5); } catch (const std::runtime_error&) { trayUnreadRejected = true; }
        check(trayUnreadRejected && device.readRegister(0x18) == 0 && device.readRegister(0x17) == 0x40);
        check(device.pendingBytes() == 4096 && device.actualSectorsRead() == 2 &&
              device.readRegister(5) == 0x80 && device.readRegister(8) == 0 && !device.takeInterruptEdge());
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
        device.writeRegister(0x16, 5);
        check(device.readRegister(0x18) == 0 && device.readRegister(8) == 1 && !device.takeInterruptEdge());
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
        device.writeRegister(0x17, 0);
        device.writeRegister(0x16, 0x80);
        check(device.readRegister(0x16) == 0x80 && device.readRegister(0x17) == 0);
        check(device.readRegister(0x18) == 0x80 && device.readRegister(0x17) == 0x40 &&
              !device.takeInterruptEdge());
        bool missingAuthParameter = false;
        try { device.writeRegister(0x16, 0x80); } catch (const std::runtime_error&) { missingAuthParameter = true; }
        check(missingAuthParameter && device.readRegister(0x17) == 0x40);
#ifdef _WIN32
        checkVirtualCardAuth(media);
#endif
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
        drakengard::CdvdDevice malformedTray(media);
        malformedTray.writeRegister(0x17, 1);
        bool trayArgsRejected = false;
        try { malformedTray.writeRegister(0x16, 5); } catch (const std::runtime_error&) { trayArgsRejected = true; }
        check(trayArgsRejected && malformedTray.readRegister(0x17) == 0x40 &&
              malformedTray.readRegister(0x0b) == 0 && !malformedTray.takeInterruptEdge());
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
