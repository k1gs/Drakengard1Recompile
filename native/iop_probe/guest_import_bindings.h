#pragma once
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace guest_imports {
inline uint32_t word(std::span<const uint8_t> bytes, size_t at) {
    return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 |
        uint32_t(bytes[at + 2]) << 16 | uint32_t(bytes[at + 3]) << 24;
}
inline void put(std::span<uint8_t> bytes, size_t at, uint32_t value) {
    for (unsigned n = 0; n < 4; ++n) bytes[at + n] = uint8_t(value >> (n * 8));
}
struct Reference {
    std::vector<uint8_t> bytes, executable;
    uint32_t imports{};
};
// Observe original guest linking; never write its tables or install host calls.
// Import headers/list links are data. Only a real J to an existing owned bootstrap
// function, backed by a matching registered export/caller chain, is executable.
template<class Memory> Reference linkedReference(const Memory& memory, uint32_t base,
    std::span<const uint8_t> source, uint32_t trustedBegin, uint32_t trustedEnd) {
    Reference result{{source.begin(), source.end()}, std::vector<uint8_t>(source.size() / 4, 1), 0};
    const auto owned = [&](uint32_t at, size_t size) {
        return !(at & 3) && at < 0x200000 && size <= 0x200000 - at && memory.ownsRamRange(at, size);
    };
    const auto named = [&](uint32_t at, size_t table) {
        return memory.read32(at + 12) == word(source, table + 12) &&
               memory.read32(at + 16) == word(source, table + 16);
    };
    for (size_t table = 0; table + 28 <= source.size(); table += 4) {
        if (word(source, table) != 0x41e00000) continue;
        const auto actualTable = base + uint32_t(table);
        const auto originalVersion = word(source, table + 8);
        const auto linkedVersion = (originalVersion & ~0x00060000u) | 0x00020000u;
        if (memory.read32(actualTable) != 0x41e00000 || !named(actualTable, table) ||
            memory.read32(actualTable + 8) != linkedVersion)
            throw std::runtime_error("Guest import identity/version/link mode differs from original table");
        for (size_t at = table; at < table + 20; at += 4) result.executable[at / 4] = 0;
        size_t stub = table + 20;
        unsigned count = 0;
        for (; stub + 8 <= source.size(); stub += 8) {
            const auto first = word(source, stub), delay = word(source, stub + 4);
            if (!first && !delay) break;
            if (first != 0x03e00008 || (delay & 0xffff0000u) != 0x24000000 || ++count > 1024)
                throw std::runtime_error("Malformed original guest import stubs");
            const auto jump = memory.read32(base + uint32_t(stub));
            const auto target = ((base + uint32_t(stub) + 4) & 0xf0000000u) | ((jump & 0x03ffffffu) << 2);
            const auto ordinal = delay & 0xffff;
            if (jump >> 26 != 2 || target < trustedBegin || uint64_t(target) + 4 > trustedEnd ||
                !owned(target, 4) || ordinal >= 1024)
                throw std::runtime_error("Guest import is unresolved or leaves existing owned bootstrap code");
            bool matched = false;
            for (uint32_t candidate = trustedBegin; uint64_t(candidate) + 20 <= trustedEnd; candidate += 4) {
                if (!named(candidate, table) || !owned(candidate, 20)) continue;
                const auto version = memory.read32(candidate + 8);
                const auto previous = memory.read32(candidate);
                if ((version & 0xff00u) != (originalVersion & 0xff00u) || (version & 0x10000u) ||
                    (previous && (previous < trustedBegin || uint64_t(previous) + 20 > trustedEnd || !owned(previous, 20))))
                    continue;
                uint32_t caller = memory.read32(candidate + 4);
                bool ownsCaller = false;
                for (unsigned links = 0; caller && links < 128; ++links) {
                    if (!owned(caller, 20) || memory.read32(caller) != 0x41e00000 || !named(caller, table)) break;
                    if (caller == actualTable) { ownsCaller = true; break; }
                    caller = memory.read32(caller + 4);
                }
                if (!ownsCaller || uint64_t(candidate) + 20 + (ordinal + 1) * 4 > trustedEnd ||
                    !owned(candidate + 20, (ordinal + 1) * 4)) continue;
                bool terminated = false;
                for (unsigned index = 0; index <= ordinal; ++index)
                    if (!memory.read32(candidate + 20 + index * 4)) { terminated = true; break; }
                if (terminated || memory.read32(candidate + 20 + ordinal * 4) != target) continue;
                matched = true;
                break;
            }
            if (!matched) throw std::runtime_error("Guest import jump has no matching registered export/caller chain");
            put(result.bytes, stub, jump);
            ++result.imports;
        }
        if (!count || stub + 8 > source.size()) throw std::runtime_error("Unterminated original guest import table");
        result.executable[stub / 4] = result.executable[stub / 4 + 1] = 0;
        table = stub + 4;
    }
    return result;
}
}
