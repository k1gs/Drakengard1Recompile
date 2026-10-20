#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace drakengard {
// PCSX2 v2.8.2 functional single-layer DVD packet, not physical raw-media
// reconstruction: ID + zero reference IED/CPR_MAI + actual user bytes + zero EDC.
// No physical CRC, encryption or authentication is claimed by this profile.
inline void referenceDvdFrame(uint32_t lsn, std::span<const uint8_t> user, std::span<uint8_t> frame) {
    if (user.size() != 2048 || frame.size() != 2064 || lsn > 0xfcffffu)
        throw std::runtime_error("Unsupported reference DVD frame range or size");
    const uint32_t id = lsn + 0x30000u;
    std::fill(frame.begin(), frame.end(), 0);
    frame[0] = 0x20; frame[1] = uint8_t(id >> 16); frame[2] = uint8_t(id >> 8); frame[3] = uint8_t(id);
    std::copy(user.begin(), user.end(), frame.begin() + 12);
}
// Fixed single-volume DVD structure from the same pinned ISO provider, with
// the final LSN derived from the actual ISO length. This is not physical TOC data.
inline void referenceDvdToc(uint32_t sectors, std::span<uint8_t> toc) {
    if (!sectors || sectors > 0xfd0000u || toc.size() != 2048)
        throw std::runtime_error("Unsupported reference DVD TOC range or size");
    std::fill(toc.begin(), toc.end(), 0);
    toc[0] = 4; toc[1] = 2; toc[2] = 0xf2; toc[4] = 0x86; toc[5] = 0x72;
    toc[12] = 1; toc[13] = 2; toc[14] = 1; toc[17] = 3;
    const uint32_t last = sectors + 0x2ffffu;
    for (unsigned n = 0; n < 4; ++n) toc[20 + n] = uint8_t(last >> (24 - n * 8));
}
}
