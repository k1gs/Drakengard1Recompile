#include "bare_call.h"
#include "services/iop_module_loader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uint32_t number(const char* text) {
    size_t consumed{};
    const auto value = std::stoull(text, &consumed, 0);
    if (consumed != std::string(text).size() || value > UINT32_MAX)
        throw std::runtime_error("Invalid 32-bit address");
    return static_cast<uint32_t>(value);
}
}

// One bare SYSMEM entry call, following the public UDNL ABI. No HLE imports,
// guessed MMIO values, IOP boot completion, guest callbacks or EE execution.
int main(int argc, char** argv) {
    if (argc != 4 || std::string(argv[1]) != "--input-complete") {
        std::cerr << "Pass --input-complete SYSMEM-file verified-boot-start-address\n";
        return 2;
    }
    try {
        using namespace ps2x::iop::detail;
        const uint32_t start = number(argv[3]);
        if (start > 0x100000 || (start & 15)) throw std::runtime_error("Unsupported bootstrap start");
        const std::filesystem::path path(argv[2]);
        const auto size = std::filesystem::file_size(path);
        if (size < 52 || size > 1024 * 1024) throw std::runtime_error("Unsupported SYSMEM image size");
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open SYSMEM read-only");
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
        if (bytes.size() != size || file.bad()) throw std::runtime_error("Short SYSMEM read");
        IopMemory memory;
        const auto loaded = IopModuleLoader::load(bytes, memory, start + 0x30);
        if (!loaded || !loaded.relocationsComplete || loaded.base != start + 0x30 ||
            loaded.size > IopMemory::RamSize - loaded.base)
            throw std::runtime_error("SYSMEM image/placement/relocation refused");
        std::cout << "[sysmem-entry] base=0x" << std::hex << loaded.base << " pc=0x" << loaded.entry
                  << " gp=0x" << loaded.gp << " ram_bytes=0x" << IopMemory::RamSize << std::dec << '\n';
        const auto result = bareCall(memory, loaded.entry, loaded.gp, IopMemory::RamSize,
                                     loaded.base, loaded.base + loaded.size);
        std::cout << "[sysmem-return] steps=" << result.instructions << " value=0x" << std::hex
                  << result.value << std::dec << "\nOne entry call returned; no IOP reboot completion established\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SYSMEM bootstrap probe stopped: " << error.what() << '\n';
        return 2;
    }
}
