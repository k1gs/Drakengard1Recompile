#pragma once
#include "ps2x/iop/iop_types.h"
#include "runtime/ps2_memory.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>

// Observe actual directional transfers; no writes, completion or synthetic replies.
inline void diagnoseSifTransfer(const uint8_t* ram, const ps2x::iop::SifTransfer& transfer) {
    static std::atomic<unsigned> count{0};
    if (count.fetch_add(1) >= 32) return;
    const auto flags = std::cerr.flags();
    std::cerr << "[sif-transfer] kind=" << unsigned(transfer.kind)
              << " phase=" << unsigned(transfer.phase) << " src=0x" << std::hex
              << transfer.sourceAddress << " dest=0x" << transfer.destinationAddress
              << " bytes=0x" << transfer.size;
    const uint32_t physical = transfer.sourceAddress & 0x1FFFFFFFu;
    if (transfer.kind == ps2x::iop::SifTransferKind::SetDma && ram && physical < PS2_RAM_SIZE) {
        const uint32_t bytes = std::min({transfer.size, 32u, PS2_RAM_SIZE - physical});
        for (uint32_t offset = 0; offset + 4 <= bytes; offset += 4) {
            uint32_t word;
            std::memcpy(&word, ram + physical + offset, 4);
            std::cerr << " word" << offset / 4 << "=0x" << word;
        }
    }
    std::cerr << std::endl;
    std::cerr.flags(flags);
}
