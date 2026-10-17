#include "sif_channel.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <source_location>
#include <string>
#include <vector>

namespace {
void check(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) throw std::runtime_error("Synthetic SIF assertion failed at line " + std::to_string(where.line()));
}
template<class F> void rejects(F action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid request accepted");
}
struct Backend : drakengard::SifBackend {
    std::array<uint8_t, 16384> ee{}, iop{};
    unsigned interrupts{}, calls{};
    bool supportCall = true;
    std::string rebootArguments;
    uint32_t rebootMode{};
    void rebootIop(std::string_view arguments, uint32_t mode) override {
        rebootArguments = arguments; rebootMode = mode;
        throw std::runtime_error("Synthetic reboot endpoint intentionally unavailable");
    }
    bool range(uint32_t address, size_t size) { return address <= ee.size() && size <= ee.size() - address; }
    bool readIop(uint32_t address, void* data, size_t size) override {
        if (!range(address, size)) return false;
        if (size) std::memcpy(data, iop.data() + address, size);
        return true;
    }
    bool readEe(uint32_t address, void* data, size_t size) override {
        if (!range(address, size)) return false;
        if (size) std::memcpy(data, ee.data() + address, size);
        return true;
    }
    bool validEe(uint32_t address, size_t size) override { return range(address, size); }
    void writeEe(uint32_t address, const void* data, size_t size) override {
        check(range(address, size));
        if (size) std::memcpy(ee.data() + address, data, size);
    }
    void replyEe(uint32_t address, const void* data, size_t size, bool interrupt) override {
        writeEe(address, data, size); interrupts += interrupt;
    }
    drakengard::SifServer bind(uint32_t sid) override { return sid == 7 ? drakengard::SifServer{0x100, 0x400, 128} : drakengard::SifServer{}; }
    bool call(const drakengard::SifServer& server, uint32_t, uint32_t function,
              uint32_t size, uint32_t receive, uint32_t receiveSize) override {
        if (!supportCall || function != 3 || size != receiveSize) return false;
        check(server.buffer == 0x400 && range(receive, receiveSize));
        writeEe(receive, iop.data() + server.buffer, size);
        ++calls; return true;
    }
    void request(uint32_t command, uint32_t size = 64, uint32_t option = 0) {
        std::fill(iop.begin(), iop.begin() + 128, 0);
        put(iop.data(), 0, size); put(iop.data(), 2, command); put(iop.data(), 3, option);
        put(iop.data(), 4, 5); put(iop.data(), 5, 0x2000); put(iop.data(), 6, 9); put(iop.data(), 7, 0x2100);
    }
    static void put(uint8_t* data, size_t index, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) data[index * 4 + i] = uint8_t(value >> (8 * i));
    }
    void field(size_t index, uint32_t value) { put(iop.data(), index, value); }
    uint32_t result(uint32_t address, size_t index) {
        uint32_t value{}; for (unsigned i = 0; i < 4; ++i) value |= uint32_t(ee[address + index * 4 + i]) << (8 * i);
        return value;
    }
    void consume() { std::fill(ee.begin() + 0x1000, ee.begin() + 0x1080, 0); }
};
}

int main() {
    try {
        Backend host; drakengard::SifChannel link;
        host.request(0x80000002, 16, 1);
        rejects([&] { link.receive(0, 16, host); });
        host.request(0x80000002, 20); host.field(4, 0x1000);
        link.receive(0, 20, host); check(!link.ready() && host.interrupts == 0);
        host.request(0x80000002, 16, 1); link.receive(0, 16, host);
        check(link.ready() && host.result(0x1000, 2) == 0x80000001 && host.result(0x1000, 5) == 1);
        rejects([&] { link.receive(0, 16, host); }); // Unconsumed reply cannot be overwritten.
        host.consume(); host.request(0x80000009); host.field(8, 999);
        link.receive(0, 64, host); check(host.result(0x1000, 9) == 0 && host.result(0x1000, 10) == 0);
        host.consume(); host.request(0x80000009); host.field(8, 7);
        link.receive(0, 64, host); check(host.result(0x1000, 9) == 0x100 && host.result(0x1000, 10) == 0x400);
        host.consume(); host.request(0x8000000a); host.field(0, 64 | (4 << 8)); host.field(1, 0x400);
        host.field(8, 3); host.field(9, 4); host.field(10, 0x2800); host.field(11, 4);
        host.field(12, 1); host.field(13, 0x100); host.iop[0x400] = 42;
        link.receive(0, 64, host); check(host.ee[0x2800] == 42 && host.calls == 1 && host.ee[0x400] == 0);
        check(host.result(0x1000, 8) == 0x8000000a && host.result(0x1000, 5) == 0x2000);
        host.consume(); const unsigned before = host.interrupts;
        host.field(12, 0); link.receive(0, 64, host);
        check(host.interrupts == before && host.result(0x2000, 4) == 0 && host.result(0x2000, 6) == 0);
        host.field(12, 1); host.supportCall = false;
        rejects([&] { link.receive(0, 64, host); }); check(host.result(0x1000, 0) == 0);
        host.request(0x8000000c); host.field(8, 0x400); host.field(9, 0x2900); host.field(10, 4);
        link.receive(0, 64, host); check(host.ee[0x2900] == 42);
        rejects([&] { link.stopReceive(host); }); // Never discard a completed packet.
        host.consume(); link.stopReceive(host);
        host.request(0x80000009); host.field(8, 7);
        const auto stopped = host.ee;
        rejects([&] { link.receive(0, 64, host); }); check(host.ee == stopped);
        link.startReceive(); link.receive(0, 64, host);
        check(host.result(0x1000, 9) == 0x100);
        host.consume(); host.request(0x80000009); host.field(0, 12);
        rejects([&] { link.receive(0, 64, host); });
        host.request(0x80000003); rejects([&] { link.receive(0, 64, host); });
        host.request(0x80000003, 104); host.field(4, 5); host.field(5, 0);
        std::memcpy(host.iop.data() + 24, "hello", 5);
        rejects([&] { link.receive(0, 104, host); });
        check(host.rebootArguments == "hello" && host.rebootMode == 0);
        host.field(4, 81); rejects([&] { link.receive(0, 104, host); });
        host.request(0x80000001, 24); host.field(4, 32); rejects([&] { link.receive(0, 24, host); });
        link.reset(); check(!link.ready() && link.receiveAddress() == 0);
        host.request(0x80000009); rejects([&] { link.receive(0, 64, host); });
        std::cout << "Directional SIF packets, backend calls, completion and refusal checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
