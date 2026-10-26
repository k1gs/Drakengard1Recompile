#pragma once
#include "gs_dma_source.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <ostream>

namespace drakengard {
struct GsGifPacketSummary {
    uint64_t id{};
    uint32_t path{}, bytes{}, status{}, tags{}, parsedBytes{}, tagOffset{};
    uint64_t expectedEnd{}, tagLo{}, tagHi{};
    bool activeTagSeen{};
    uint32_t activeTagOffset{};
    uint64_t activeTagLo{}, activeTagHi{};
    bool dmaKnown{},cpuKnown{},spanOverflow{};
    uint32_t dmaTadr{},dmaChcr{},dmaPc{},dmaRa{},dmaSp{};
    std::array<uint64_t, 8> prefix{};
};
inline thread_local GsGifPacketSummary gsGifPacketContext;
inline void observeGsGifTag(uint32_t offset, uint64_t lo, uint64_t hi) {
    if (!gsGifPacketContext.id) return;
    gsGifPacketContext.activeTagSeen = true;
    gsGifPacketContext.activeTagOffset = offset;
    gsGifPacketContext.activeTagLo = lo;
    gsGifPacketContext.activeTagHi = hi;
}

// Describes one submitted buffer, not whether hardware permits a split stream.
// status: 0 self-contained, 1 short header, 2 short payload, 3 scan budget reached.
inline GsGifPacketSummary inspectGsGifPacket(const uint8_t* data, uint32_t size) {
    GsGifPacketSummary result; result.bytes = size;
    if (!data) { result.status = 1; result.expectedEnd = 16; return result; }
    std::memcpy(result.prefix.data(), data, size < sizeof(result.prefix) ? size : sizeof(result.prefix));
    uint64_t offset = 0;
    while (offset < size) {
        result.parsedBytes = uint32_t(offset);
        if (size - offset < 16) { result.status = 1; result.expectedEnd = offset + 16; return result; }
        if (result.tags == 256) { result.status = 3; return result; }
        result.tagOffset = uint32_t(offset);
        std::memcpy(&result.tagLo, data + offset, 8);
        std::memcpy(&result.tagHi, data + offset + 8, 8);
        ++result.tags;
        const uint64_t loops = result.tagLo & 0x7fffu;
        const auto mode = (result.tagLo >> 58) & 3u;
        uint64_t registers = result.tagLo >> 60;
        if (!registers) registers = 16;
        const uint64_t payload = mode == 0 ? loops * registers * 16 :
            mode == 1 ? ((loops * registers * 8 + 15) & ~uint64_t(15)) : loops * 16;
        result.expectedEnd = offset + 16 + payload;
        if (result.expectedEnd > size) { result.status = 2; return result; }
        offset = result.expectedEnd;
    }
    result.parsedBytes = size;
    return result;
}
struct GsGifPacketCounters {
    std::array<uint64_t, 4> packets{}, bytes{};
    std::array<uint64_t, 4> framing{};
    uint64_t unknownPaths{};
    void observe(const GsGifPacketSummary& packet) {
        if (packet.path >= 1 && packet.path <= 3) {
            ++packets[packet.path]; bytes[packet.path] += packet.bytes;
        } else ++unknownPaths;
        ++framing[packet.status];
    }
};
inline void writeGsGifPacketJson(std::ostream& out, const GsGifPacketSummary& p) {
    out << "{\"id\":" << p.id << ",\"path\":" << p.path << ",\"bytes\":" << p.bytes
        << ",\"status\":" << p.status << ",\"tags\":" << p.tags
        << ",\"parsed_bytes\":" << p.parsedBytes << ",\"expected_end\":" << p.expectedEnd
        << ",\"tag_offset\":" << p.tagOffset << ",\"tag_lo\":" << p.tagLo << ",\"tag_hi\":" << p.tagHi
        << ",\"active_tag_seen\":" << (p.activeTagSeen ? "true" : "false")
        << ",\"active_tag_offset\":" << p.activeTagOffset << ",\"active_tag_lo\":" << p.activeTagLo
        << ",\"active_tag_hi\":" << p.activeTagHi
        << ",\"dma_known\":" << (p.dmaKnown?"true":"false") << ",\"cpu_known\":" << (p.cpuKnown?"true":"false")
        << ",\"dma_tadr\":" << p.dmaTadr << ",\"dma_chcr\":" << p.dmaChcr
        << ",\"dma_pc\":" << p.dmaPc << ",\"dma_ra\":" << p.dmaRa << ",\"dma_sp\":" << p.dmaSp
        << ",\"source_span_overflow\":" << (p.spanOverflow?"true":"false")
        << ",\"prefix_words\":[";
    for (size_t i = 0; i < p.prefix.size(); ++i) { if (i) out << ','; out << p.prefix[i]; }
    out << "]}";
}
}
