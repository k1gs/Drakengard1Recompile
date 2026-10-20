#include "verified_ram_code.h"
#include <array>

namespace {
void check(bool value) { if (!value) throw std::runtime_error("RAM code verification check failed"); }
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unverified RAM code was accepted");
}
struct Memory {
    bool owned{true};
    std::array<uint32_t, 3> words{0x2402002a, 0x03e00008, 0};
    bool ownsRamRange(uint32_t base, size_t size) const {
        return owned && base >= 0x1000 && base <= 0x100c && size <= 0x100c - base;
    }
    uint32_t read32(uint32_t address) const { return words.at((address - 0x1000) / 4); }
};
}
void checkRamCode() {
    const std::array<uint8_t, 12> bytes{0x2a, 0, 2, 0x24, 8, 0, 0xe0, 3, 0, 0, 0, 0};
    refuses([&] { VerifiedRamCode invalid(0x1001, 0x1000, bytes); });
    refuses([&] { VerifiedRamCode invalid(0x1ffffc, 0x1ffffc, bytes); });
    Memory memory; VerifiedRamCode code(0x1000, 0x1000, bytes);
    check(!code.allows(memory, 0x1004) && !code.allows(memory, 0x1001));
    memory.owned = false; refuses([&] { code.allows(memory, 0x1000); }); memory.owned = true;
    memory.words[2] = 1; refuses([&] { code.allows(memory, 0x1000); });
    memory.words[2] = 0;
    check(code.allows(memory, 0x1000) && code.allows(memory, 0x1004));
    memory.words[1] = 0; refuses([&] { code.allows(memory, 0x1004); });
    check(!code.allows(memory, 0x100c) && !code.allows(memory, 0x80001000));
}
