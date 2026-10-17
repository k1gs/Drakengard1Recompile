#include "fileio_rpc.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value) { if (!value) throw std::runtime_error("FileIO synthetic assertion failed"); }
template<class F> void rejects(F action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid FileIO arguments accepted");
}
struct Host : drakengard::FileIoHost {
    std::ifstream file;
    std::array<uint8_t, 8192> ram;
    Host() { ram.fill(0x55); }
    bool validEe(uint32_t address, size_t bytes) override { return address <= ram.size() && bytes <= ram.size() - address; }
    void writeEe(uint32_t address, const void* data, size_t bytes) override {
        check(validEe(address, bytes)); std::memcpy(ram.data() + address, data, bytes);
    }
    int32_t fileOpen(const std::string& path, uint32_t flags) override {
        check(flags == 1); file.open(path, std::ios::binary); return file ? 3 : -1;
    }
    int32_t fileClose(int32_t descriptor) override {
        if (descriptor != 3 || !file.is_open()) return -1;
        file.close(); return 0;
    }
    int32_t fileRead(int32_t descriptor, uint32_t address, uint32_t bytes) override {
        if (descriptor != 3 || !file.is_open()) return -1;
        check(validEe(address, bytes));
        file.read(reinterpret_cast<char*>(ram.data() + address), bytes);
        return file.bad() ? -1 : static_cast<int32_t>(file.gcount());
    }
    int32_t fileSeek(int32_t descriptor, int32_t offset, uint32_t whence) override {
        if (descriptor != 3 || !file.is_open()) return -1;
        const std::ios::seekdir origins[] = {std::ios::beg, std::ios::cur, std::ios::end};
        file.clear(); file.seekg(offset, origins[whence]);
        return file ? static_cast<int32_t>(file.tellg()) : -1;
    }
};
std::vector<uint8_t> words(std::initializer_list<uint32_t> values) {
    std::vector<uint8_t> result;
    for (uint32_t value : values)
        for (unsigned n = 0; n < 4; ++n) result.push_back(uint8_t(value >> (8 * n)));
    return result;
}
}

int main(int argc, char** argv) {
    try {
        check(argc == 2); Host host;
        check(drakengard::fileIoRpc(0xff, words({0x1000, 0x1100}), host) == -1);
        check(host.ram[0x1000] == 0x55 && host.ram[0x1100] == 0x55);
        auto open = words({1});
        const std::string path = argv[1]; open.insert(open.end(), path.begin(), path.end()); open.push_back(0);
        check(drakengard::fileIoRpc(0, open, host) == 3);
        auto read = words({3, 0x1001, 5, 0x1800});
        check(drakengard::fileIoRpc(2, read, host) == 5);
        check(std::memcmp(host.ram.data() + 0x1001, "abc12", 5) == 0);
        check(host.ram[0x1000] == 0x55 && host.ram[0x1006] == 0x55);
        check(std::all_of(host.ram.begin() + 0x1800, host.ram.begin() + 0x1810, [](uint8_t value) { return value == 0; }));
        check(host.ram[0x1810] == 0x55);
        auto tail = words({3, 0x1103, 3, 0x1800});
        check(drakengard::fileIoRpc(2, tail, host) == 1 && host.ram[0x1103] == '3' && host.ram[0x1104] == 0x55);
        check(drakengard::fileIoRpc(2, tail, host) == 0);
        check(drakengard::fileIoRpc(4, words({3, 0, 0}), host) == 0);
        rejects([&] { drakengard::fileIoRpc(2, words({3, 8190, 4, 0x1800}), host); });
        rejects([&] { drakengard::fileIoRpc(2, words({3, 0x1000, 4, 0x1000}), host); });
        check(drakengard::fileIoRpc(4, words({3, 0, 1}), host) == 0);
        check(drakengard::fileIoRpc(4, words({3, uint32_t(-1), 2}), host) == 5);
        check(drakengard::fileIoRpc(1, words({3}), host) == 0);
        check(drakengard::fileIoRpc(2, read, host) == -1);
        check(!drakengard::fileIoRpc(3, words({}), host).has_value());
        auto writeOpen = open; writeOpen[0] = 2;
        rejects([&] { drakengard::fileIoRpc(0, writeOpen, host); });
        rejects([&] { drakengard::fileIoRpc(0, words({1, 0x41414141}), host); });
        rejects([&] { drakengard::fileIoRpc(2, words({3}), host); });
        std::cout << "Actual file bytes, unaligned reads, EOF, seek, close and refusal checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
