#include "verified_ram_code.h"
#include <algorithm>

namespace {
constexpr uint32_t Base = 0x100000, Export = 0x800, Target = 0x200;
struct Memory {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x200000);
    bool targetOwned{true};
    bool ownsRamRange(uint32_t at, size_t size) const {
        return at <= bytes.size() && size <= bytes.size() - at &&
            (targetOwned || at != Target);
    }
    uint32_t read32(uint32_t at) const { return guest_imports::word(bytes, at); }
    void put(uint32_t at, uint32_t value) { guest_imports::put(bytes, at, value); }
};
struct Fixture {
    Memory memory;
    std::vector<uint8_t> source = std::vector<uint8_t>(44);
    Fixture() {
        guest_imports::put(source, 0, 0x03e00008);
        guest_imports::put(source, 8, 0x41e00000);
        guest_imports::put(source, 16, 0x101);
        guest_imports::put(source, 20, 0x6f6d6564); // Synthetic library "demo".
        guest_imports::put(source, 28, 0x03e00008);
        guest_imports::put(source, 32, 0x24000004);
        std::copy(source.begin(), source.end(), memory.bytes.begin() + Base);
        memory.put(Base + 16, 0x00020101);
        memory.put(Base + 28, 0x08000000 | (Target >> 2));
        memory.put(Export + 4, Base + 8);
        memory.put(Export + 8, 0x101);
        memory.put(Export + 12, 0x6f6d6564);
        for (unsigned n = 0; n <= 4; ++n) memory.put(Export + 20 + n * 4, Target);
    }
    VerifiedRamCode code() const { return VerifiedRamCode(Base, Base, source, 0x200, 0x1000); }
};
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unverified guest import accepted");
}
}
void checkGuestImports() {
    Fixture fixture; auto code = fixture.code();
    if (!code.allows(fixture.memory, Base) || !code.allows(fixture.memory, Base + 28) ||
        code.allows(fixture.memory, Base + 8) || code.allows(fixture.memory, Base + 36))
        throw std::runtime_error("Linked import code/data separation failed");
    fixture.memory.put(Base + 32, 0x24000005);
    refuses([&] { code.allows(fixture.memory, Base + 32); });
    Fixture wrongExport; wrongExport.memory.put(Export + 36, Target + 4);
    refuses([&] { auto policy = wrongExport.code(); policy.allows(wrongExport.memory, Base); });
    Fixture noCaller; noCaller.memory.put(Export + 4, 0);
    refuses([&] { auto policy = noCaller.code(); policy.allows(noCaller.memory, Base); });
    Fixture unowned; unowned.memory.targetOwned = false;
    refuses([&] { auto policy = unowned.code(); policy.allows(unowned.memory, Base); });
    Fixture escaped; escaped.memory.put(Base + 28, 0x08000000 | (0x110000 >> 2));
    refuses([&] { auto policy = escaped.code(); policy.allows(escaped.memory, Base); });
    Fixture changed; changed.memory.put(Base, 0);
    refuses([&] { auto policy = changed.code(); policy.allows(changed.memory, Base); });
    if (changed.memory.read32(Base)) throw std::runtime_error("Import verifier repaired guest instructions");
    Fixture version; version.memory.put(Export + 8, 0x201);
    refuses([&] { auto policy = version.code(); policy.allows(version.memory, Base); });
    Fixture unlinked; unlinked.memory.put(Base + 16, 0x101);
    refuses([&] { auto policy = unlinked.code(); policy.allows(unlinked.memory, Base); });
    Fixture ordinal; ordinal.memory.put(Base + 32, 0x24000005);
    refuses([&] { auto policy = ordinal.code(); policy.allows(ordinal.memory, Base); });
    Fixture cycle;
    cycle.memory.put(Export + 4, 0x900);
    cycle.memory.put(0x900, 0x41e00000); cycle.memory.put(0x904, 0x900);
    cycle.memory.put(0x90c, 0x6f6d6564);
    refuses([&] { auto policy = cycle.code(); policy.allows(cycle.memory, Base); });
    Fixture metadata;
    VerifiedRamCode badEntry(Base, Base + 8, metadata.source, 0x200, 0x1000);
    refuses([&] { badEntry.allows(metadata.memory, Base + 8); });
}
