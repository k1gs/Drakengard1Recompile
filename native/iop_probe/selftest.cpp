#include "bare_call.h"
#include <cstring>
#include <initializer_list>

namespace {
using ps2x::iop::detail::IopMemory;
void code(IopMemory& memory, std::initializer_list<uint32_t> words) {
    uint32_t address = 0x1000;
    for (uint32_t word : words) { memory.write32(address, word); address += 4; }
}
template<class Action> void rejects(Action action, const std::string& reason) {
    try { action(); }
    catch (const std::runtime_error& error) {
        if (std::string(error.what()).find(reason) != std::string::npos) return;
        throw;
    }
    throw std::runtime_error("Strict IOP guard failed to reject " + reason);
}
}

int main() {
    try {
        IopMemory memory;
        code(memory, {0x2402002a, 0x03e00008, 0});
        auto result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030);
        if (result.value != 42 || result.instructions != 3)
            throw std::runtime_error("Bare IOP return/delay-slot contract failed");
        memory.reset(); code(memory, {0x8c820000, 0x03e00008, 0}); // lw v0,0(a0)
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf801450, 0x1000, 0x1030); }, "hardware contract");
        std::array<uint8_t, 28> seed{'D', '1', 'H', 'W', '0', '0', '1', 0};
        const auto put = [&](size_t at, uint32_t value) {
            for (unsigned n = 0; n < 4; ++n) seed[at + n] = uint8_t(value >> (8 * n));
        };
        put(8, 2); put(12, 0x1f801450); put(16, 0x12345678); put(20, 0x1f801578); put(24, 1);
        PassiveHardware hardware; hardware.seed(seed, memory);
        result = bareCall(memory, 0x1000, 0, 0xbf801450, 0x1000, 0x1030, &hardware);
        if (result.value != 0x12345678) throw std::runtime_error("Passive hardware read lost actual state");
        code(memory, {0xac800000, 0x8c820000, 0, 0x03e00008, 0}); // sw zero,0(a0); lw v0,0(a0)
        result = bareCall(memory, 0x1000, 0, 0xbf801578, 0x1000, 0x1030, &hardware);
        if (result.value != 0 || memory.read32(0xbf801578) != 0)
            throw std::runtime_error("DMA-global gate storage did not change");
        code(memory, {0x24020002, 0xac820000, 0x03e00008, 0}); // Unreviewed gate bits.
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf801578, 0x1000, 0x1030, &hardware); }, "hardware contract");
        put(20, 0x1f801450);
        rejects([&] { hardware.seed(seed, memory); }, "duplicate passive hardware register");
        if (memory.read32(0xbf801578) != 0) throw std::runtime_error("Rejected seed changed hardware");
        memory.reset(); code(memory, {0xfc000000}); // Unsupported instruction must raise an exception.
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "raised an exception");
        memory.reset();
        code(memory, {0x41e00000, 0, 0x100, 0, 0, 0x03e00008, 0x24000007, 0, 0});
        const char library[8] = "missing";
        if (!memory.writeRam(0x100c, library, sizeof(library))) throw std::runtime_error("Fixture write failed");
        rejects([&] { bareCall(memory, 0x1014, 0, 0, 0x1000, 0x1030); }, "Unresolved guest import missing:7");
        memory.reset();
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "owned bootstrap code");
        code(memory, {0x1000ffff, 0}); // Deliberate bounded loop.
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "instruction budget");
        std::cout << "Bare IOP return, exception, MMIO, unresolved import, ownership and budget checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
