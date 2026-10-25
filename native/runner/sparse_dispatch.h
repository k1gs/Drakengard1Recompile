#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace drakengard {
// Preparation emits nonempty tables sorted by exact guest address. These are
// probed for every application dispatch as well as actual system-code calls.
template<class Entry, std::size_t Count>
const Entry* findExactSparseEntry(const Entry (&entries)[Count], uint32_t address) {
    static_assert(Count > 0);
    if (address < entries[0].address || address > entries[Count - 1].address) return nullptr;
    const auto* last = entries + Count;
    const auto* entry = std::lower_bound(entries, last, address,
        [](const Entry& item, uint32_t pc) { return item.address < pc; });
    return entry != last && entry->address == address ? entry : nullptr;
}
}
