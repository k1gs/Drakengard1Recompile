#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace drakengard {
struct EeToSprVu0State { uint32_t control{},address{},count{},scratch{}; };

// Functional normal DMA9 from the owner's actual VU0 data RAM. No source
// mirrors/crossings or asynchronous VU arbitration are synthesized here.
inline bool transferEeVu0ToSpr(EeToSprVu0State& state,std::span<const uint8_t> vu0,
                              std::span<uint8_t> scratchpad,bool enabled) {
    if (!(state.control&0x100u) || !enabled) return false;
    if ((state.control&0xffffu)!=0x101u || state.count>0xffffu)
        throw std::runtime_error("Unsupported VU0-data toSPR direction/mode/options");
    if (vu0.size()!=0x1000u || scratchpad.size()!=0x4000u)
        throw std::runtime_error("VU0-data toSPR requires actual 4-KiB VU0 and 16-KiB SPR");
    const uint32_t bytes=state.count*16u;
    if ((state.address&15u) || state.address<0x11004000u || state.address>=0x11005000u ||
        bytes>0x11005000u-state.address)
        throw std::runtime_error("Unsupported VU0-data toSPR source mirror/boundary");
    const uint32_t offset=state.address-0x11004000u;
    uint32_t destination=state.scratch&0x3ff0u,copied=0;
    while (copied<bytes) {
        const uint32_t chunk=std::min(bytes-copied,0x4000u-destination);
        std::memcpy(scratchpad.data()+destination,vu0.data()+offset+copied,chunk);
        destination=(destination+chunk)&0x3fffu; copied+=chunk;
    }
    state.address+=bytes; state.count=0; state.scratch=destination;
    state.control&=~0x100u;
    return true; // The caller queues cause9, once, after the real copy.
}
}
