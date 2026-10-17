#include "core/iop_memory.h"
#include "services/iop_module_loader.h"
#include "imports/iop_imports.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Optional local diagnostic against pinned upstream. It never executes IOP code,
// acknowledges reboot, modifies input, or registers guest export libraries.
int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[1]) != "--input-complete") {
        std::cerr << "Pass --input-complete module-file [module-file ...]\n";
        return 2;
    }
    try {
        using namespace ps2x::iop::detail;
        IopMemory memory;
        IopImportRegistry imports(memory);
        uint32_t cursor = 0x10000;
        for (int index = 2; index < argc; ++index) {
            const std::filesystem::path path(argv[index]);
            const auto size = std::filesystem::file_size(path);
            if (size < 52 || size > 16 * 1024 * 1024)
                throw std::runtime_error("Unsupported module input size");
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Cannot open module read-only");
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
            if (bytes.size() != size || file.bad()) throw std::runtime_error("Short module read");
            const auto loaded = IopModuleLoader::load(bytes, memory, cursor);
            std::cout << "[iop-image] file=" << path.filename().string()
                      << " error=" << unsigned(loaded.error) << " base=0x" << std::hex << loaded.base
                      << " size=0x" << loaded.size << " entry=0x" << loaded.entry
                      << " gp=0x" << loaded.gp << std::dec
                      << " relocations_complete=" << loaded.relocationsComplete << '\n';
            if (!loaded || !loaded.relocationsComplete)
                throw std::runtime_error("Module image rejected before any execution");
            const auto physical = IopMemory::physicalAddress(loaded.entry);
            if (!loaded.size || loaded.base >= IopMemory::RamSize ||
                loaded.size > IopMemory::RamSize - loaded.base || physical < loaded.base ||
                physical >= uint64_t(loaded.base) + loaded.size || !memory.ownsRamRange(physical, 4))
                throw std::runtime_error("Loaded entry outside owned module RAM");
            std::map<std::string, std::set<uint16_t>> dependencies;
            for (uint32_t address = loaded.base; address + 8 <= loaded.base + loaded.size; address += 4) {
                if (memory.read32(address) != 0x03e00008u) continue;
                const auto call = imports.decode(address);
                if (call) dependencies[call->library].insert(call->ordinal);
            }
            for (const auto& [library, ordinals] : dependencies) {
                std::cout << "[iop-imports] library=" << library << " ordinals=";
                for (const auto ordinal : ordinals) std::cout << ordinal << ',';
                std::cout << '\n';
            }
            cursor = loaded.nextModuleCursor;
        }
        std::cout << "Image/relocation probe completed; no IOP instructions executed\n";
    } catch (const std::exception& error) {
        std::cerr << "IOP image probe stopped: " << error.what() << '\n';
        return 2;
    }
}
