#pragma once
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>
#include "guest_import_bindings.h"

// The caller derives this reference from a separately hash-gated original image.
// Verification never loads or repairs the guest image. Activation requires the
// real entry and all executable words; import metadata is never executable.
// Each later instruction is rechecked.
class VerifiedRamCode {
    uint32_t base, entry;
    std::vector<uint8_t> text;
    std::vector<uint8_t> executable;
    uint32_t trustedBegin{}, trustedEnd{};
    bool active{};
    bool uncachedAlias{}, aliasObserved{};
    uint32_t word(size_t offset) const {
        return uint32_t(text[offset]) | uint32_t(text[offset + 1]) << 8 |
            uint32_t(text[offset + 2]) << 16 | uint32_t(text[offset + 3]) << 24;
    }
public:
    VerifiedRamCode(uint32_t base, uint32_t entry, std::span<const uint8_t> text,
                    uint32_t trustedBegin = 0, uint32_t trustedEnd = 0)
        : base(base), entry(entry), text(text.begin(), text.end()),
          executable(text.size() / 4, 1), trustedBegin(trustedBegin), trustedEnd(trustedEnd) {
        if ((base & 3) || (entry & 3) || text.empty() || text.size() % 4 || text.size() > 65536 ||
            base >= 0x200000 || text.size() > 0x200000 - base || entry < base || entry - base >= text.size() ||
            (trustedBegin & 3) || (trustedEnd & 3) || trustedBegin > trustedEnd || trustedEnd > 0x200000)
            throw std::runtime_error("Invalid verified RAM code profile");
    }
    void enableUncachedAlias() {
        if (active || uncachedAlias)
            throw std::runtime_error("Uncached module alias requires inactive single activation");
        uncachedAlias = true;
    }
    template<class Memory> bool allows(const Memory& memory, uint32_t pc) {
        const bool uncached = pc >> 29 == 5;
        if (uncached && (!uncachedAlias || !active)) return false;
        const uint32_t address = uncached ? pc & 0x1fffffffu : pc;
        if ((address & 3) || address < base || uint64_t(address) + 4 > uint64_t(base) + text.size()) return false;
        if (!active && pc != entry) return false;
        if (active && !executable[(address - base) / 4]) return false;
        if (!memory.ownsRamRange(active ? address : base, active ? 4 : text.size()))
            throw std::runtime_error("Verified module text is not wholly owned guest RAM");
        const auto check = [&](size_t offset) {
            const auto actual = memory.read32(base + uint32_t(offset)), expected = word(offset);
            if (actual != expected) {
                std::cerr << "[iop-ram-code-mismatch] address=0x" << std::hex << base + offset
                          << " expected=0x" << expected << " actual=0x" << actual << std::dec << '\n';
                throw std::runtime_error("Guest module text differs from its verified relocated reference");
            }
        };
        if (!active) {
            auto linked = guest_imports::linkedReference(memory, base, text, trustedBegin, trustedEnd);
            // Commit the local reference only after all executable bytes match.
            for (size_t offset = 0; offset < text.size(); offset += 4) {
                if (linked.executable[offset / 4] && memory.read32(base + uint32_t(offset)) !=
                    guest_imports::word(linked.bytes, offset)) {
                    std::cerr << "[iop-ram-code-mismatch] address=0x" << std::hex << base + offset
                              << " expected=0x" << guest_imports::word(linked.bytes, offset)
                              << " actual=0x" << memory.read32(base + uint32_t(offset)) << std::dec << '\n';
                    throw std::runtime_error("Guest module text differs from its verified relocated reference");
                }
            }
            if (!linked.executable[(entry - base) / 4])
                throw std::runtime_error("Guest module entry points into import metadata");
            text = std::move(linked.bytes); executable = std::move(linked.executable);
            active = true;
            std::cerr << "[iop-ram-code-verified] base=0x" << std::hex << base << " entry=0x" << entry
                      << std::dec << " text_bytes=" << text.size() << " imports=" << linked.imports << '\n';
        } else check(address - base);
        if (uncached && !aliasObserved) {
            aliasObserved = true;
            std::cerr << "[iop-ram-code-alias] pc=0x" << std::hex << pc << " physical=0x" << address
                      << std::dec << "; original instruction verified\n";
        }
        return true;
    }
};
