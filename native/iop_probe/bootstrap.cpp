#include "bare_call.h"
#include "services/iop_module_loader.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <vector>

namespace {
uint32_t word(std::span<const uint8_t> data, size_t offset) {
    if (offset > data.size() || data.size() - offset < 4) throw std::runtime_error("Short boot bundle word");
    return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 |
        uint32_t(data[offset + 2]) << 16 | uint32_t(data[offset + 3]) << 24;
}
}

int main(int argc, char** argv) {
    if ((argc != 3 && argc != 5) || std::string(argv[1]) != "--input-complete" ||
        (argc == 5 && std::string(argv[3]) != "--hardware-state")) {
        std::cerr << "Pass --input-complete private-boot-plan.bin [--hardware-state reviewed-seed.bin]\n"; return 2;
    }
    try {
        using namespace ps2x::iop::detail;
        const auto size = std::filesystem::file_size(argv[2]);
        if (size < 16 || size > 512 * 1024) throw std::runtime_error("Unsupported boot bundle size");
        std::ifstream file(argv[2], std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open boot bundle read-only");
        const std::vector<uint8_t> bundle((std::istreambuf_iterator<char>(file)), {});
        if (bundle.size() != size || file.bad() || std::memcmp(bundle.data(), "D1IOP01\0", 8))
            throw std::runtime_error("Invalid boot bundle");
        const auto start = word(bundle, 8), count = word(bundle, 12);
        if (start > 0x100000 || start % 16 || count < 2 || count > 128)
            throw std::runtime_error("Invalid bootstrap layout");
        constexpr uint32_t SourcePool = 0xf0000, StackGuard = IopMemory::RamSize - 8192;
        IopMemory memory;
        PassiveHardware hardware;
        if (argc == 5) hardware.load(argv[4], memory);
        const uint32_t pointers = SourcePool + 32;
        uint32_t source = (pointers + (count + 1) * 4 + 15) & ~15u;
        size_t position = 16;
        std::vector<std::span<const uint8_t>> modules;
        for (uint32_t index = 0; index < count; ++index) {
            if (position > bundle.size() || bundle.size() - position < 16)
                throw std::runtime_error("Short boot module record");
            const std::string name(reinterpret_cast<const char*>(bundle.data() + position), 10);
            if ((index == 0 && name != std::string("SYSMEM\0\0\0\0", 10)) ||
                (index == 1 && name != std::string("LOADCORE\0\0", 10)) || bundle[position + 10] || bundle[position + 11])
                throw std::runtime_error("Invalid boot module identity/order");
            const uint32_t bytes = word(bundle, position + 12); position += 16;
            if (bytes < 52 || bytes > bundle.size() - position || source >= StackGuard || bytes > StackGuard - source)
                throw std::runtime_error("Bootstrap module source exceeds protected pool");
            auto image = std::span(bundle).subspan(position, bytes);
            if (std::memcmp(image.data(), "\x7f" "ELF", 4) || !memory.writeRam(source, image.data(), image.size()))
                throw std::runtime_error("Invalid module source image");
            memory.write32(pointers + index * 4, source);
            modules.push_back(image); position += bytes;
            source = (source + bytes + 15) & ~15u;
        }
        if (position != bundle.size() || source + 256 >= StackGuard)
            throw std::runtime_error("Trailing/oversized bootstrap source pool");
        memory.write32(pointers + count * 4, 0);
        const std::array<uint32_t, 8> reset{2, 3, 0, start + 0x30, SourcePool,
                                           source - SourcePool + 256, count, pointers};
        for (size_t i = 0; i < reset.size(); ++i) memory.write32(SourcePool + uint32_t(i * 4), reset[i]);
        const auto sysmem = IopModuleLoader::load(modules[0], memory, start + 0x30);
        if (!sysmem || !sysmem.relocationsComplete || sysmem.base != start + 0x30 ||
            sysmem.base + uint64_t(sysmem.size) >= SourcePool)
            throw std::runtime_error("SYSMEM bootstrap image refused");
        const auto initialized = bareCall(memory, sysmem.entry, sysmem.gp, IopMemory::RamSize, start, SourcePool, &hardware);
        std::cout << "[bootstrap-sysmem] steps=" << initialized.instructions << " free_start=0x"
                  << std::hex << initialized.value << std::dec << '\n';
        if (initialized.value % 16 || initialized.value >= SourcePool - 0x30)
            throw std::runtime_error("SYSMEM returned an invalid free-memory boundary");
        const auto loadcore = IopModuleLoader::load(modules[1], memory, initialized.value + 0x30);
        if (!loadcore || !loadcore.relocationsComplete || loadcore.base != initialized.value + 0x30 ||
            loadcore.base + uint64_t(loadcore.size) >= SourcePool)
            throw std::runtime_error("LOADCORE bootstrap image refused");
        std::cout << "[bootstrap-loadcore] base=0x" << std::hex << loadcore.base << " entry=0x"
                  << loadcore.entry << " reset_data=0x" << SourcePool << std::dec << " modules=" << count << '\n';
        const auto result = bareCall(memory, loadcore.entry, loadcore.gp, SourcePool, start, SourcePool, &hardware);
        std::cout << "[bootstrap-loadcore-return] value=0x" << std::hex << result.value << std::dec
                  << " steps=" << result.instructions << "\nNo EE notification or BOOTEND has been issued\n";
    } catch (const std::exception& error) {
        std::cerr << "IOP bootstrap stopped: " << error.what() << '\n'; return 2;
    }
}
