#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace drakengard {
#if defined(_MSC_VER)
#define DRAKENGARD_GIF_PROBE_INLINE __forceinline
#else
#define DRAKENGARD_GIF_PROBE_INLINE __attribute__((always_inline)) inline
#endif
inline constexpr uint32_t EeGifProbeRamBytes = 32u * 1024u * 1024u;
inline constexpr size_t EeGifProbeCapacity = 128;

struct EeGifWriteAttempt {
    uint64_t sequence{}, low{}, high{};
    uint32_t pc{}, ra{}, sp{}, address{}, width{};
};

struct EeGifProbeReport {
    uint64_t generation{}, armedPacket{}, consumedPacket{}, totalAttempts{}, droppedAttempts{};
    uint32_t address{}, bytes{}, armedDmaTag{}, consumedDmaTag{};
    size_t count{};
    std::array<EeGifWriteAttempt, EeGifProbeCapacity> attempts{};
};

// Single serialized EE-executor ownership is required for arm/observe/consume.
// The GS CPU Submit callback runs synchronously on that executor. UI/other threads
// must receive a copied report, or inspect only after the executor has stopped.
// This observes CPU store ATTEMPTS only: unchanged writes are retained, and an
// empty report cannot exclude DMA, memcpy, host services or earlier writes.
class EeGifWriteProbe {
    bool active_{};
    uint64_t generation_{}, armedPacket_{}, totalAttempts_{};
    uint32_t address_{}, bytes_{}, dmaTag_{};
    std::array<EeGifWriteAttempt, EeGifProbeCapacity> attempts_{};

    static constexpr bool range(uint32_t address, uint32_t bytes) noexcept {
        return bytes && address < EeGifProbeRamBytes &&
            uint64_t(address) + bytes <= EeGifProbeRamBytes;
    }
    static constexpr bool storeWidth(uint32_t bytes) noexcept {
        return bytes == 1 || bytes == 2 || bytes == 4 || bytes == 8 || bytes == 16;
    }
    void copyReport(EeGifProbeReport& report, uint64_t consumedPacket, uint32_t consumedTag) const noexcept {
        report.generation = generation_;
        report.armedPacket = armedPacket_;
        report.consumedPacket = consumedPacket;
        report.totalAttempts = totalAttempts_;
        report.address = address_; report.bytes = bytes_;
        report.armedDmaTag = dmaTag_; report.consumedDmaTag = consumedTag;
        report.count = totalAttempts_ < EeGifProbeCapacity ? size_t(totalAttempts_) : EeGifProbeCapacity;
        report.droppedAttempts = totalAttempts_ - report.count;
        const uint64_t first = totalAttempts_ - report.count;
        for (size_t i = 0; i < report.count; ++i)
            report.attempts[i] = attempts_[(first + i) % attempts_.size()];
    }

public:
    bool active() const noexcept { return active_; }
    uint64_t generation() const noexcept { return generation_; }
    uint32_t address() const noexcept { return address_; }
    // Cheap enough for a macro hook even in the runner's /Od build. Call this
    // before extracting PC/RA if the caller wants to avoid disabled-path work.
    DRAKENGARD_GIF_PROBE_INLINE bool matches(uint32_t physicalAddress, uint32_t width) const noexcept {
        return active_ && uint64_t(physicalAddress) + width > address_ &&
            uint64_t(address_) + bytes_ > physicalAddress;
    }

    // Caller supplies an actually observed mapped RAM source, not a guessed alias.
    // An active interval cannot be silently retargeted or restarted by more draws.
    bool arm(uint32_t physicalAddress, uint32_t bytes, uint64_t packet, uint32_t dmaTag = 0) noexcept {
        if (active_ || !range(physicalAddress, bytes) || bytes > 16 || !packet || generation_ == UINT64_MAX)
            return false;
        ++generation_; active_ = true; address_ = physicalAddress; bytes_ = bytes;
        armedPacket_ = packet; dmaTag_ = dmaTag; totalAttempts_ = 0;
        return true;
    }

    // Call only in the real RAM-store branch, using its translated physical
    // address. Reject MMIO/scratchpad/virtual addresses rather than masking them.
    void observe(uint32_t physicalAddress, uint32_t width, uint64_t low, uint64_t high,
                 uint32_t pc, uint32_t ra, uint32_t sp = 0) noexcept {
        if (!active_) return;
        if (!storeWidth(width) || !range(physicalAddress, width) ||
            uint64_t(physicalAddress) + width <= address_ ||
            uint64_t(address_) + bytes_ <= physicalAddress) return;
        auto& attempt = attempts_[totalAttempts_ % attempts_.size()];
        attempt = {totalAttempts_ + 1, low, high, pc, ra, sp, physicalAddress, width};
        ++totalAttempts_;
    }

    // A later packet must consume a range covering the entire observed target.
    // Packet and DMA-tag IDs label physical-address reuse; they do not assert that
    // the buffer still belongs to the same logical object or has the same layout.
    bool consume(uint32_t physicalAddress, uint32_t bytes, uint64_t packet,
                 EeGifProbeReport& report, uint32_t dmaTag = 0) noexcept {
        if (!active_ || packet <= armedPacket_ || !range(physicalAddress, bytes) ||
            physicalAddress > address_ ||
            uint64_t(physicalAddress) + bytes < uint64_t(address_) + bytes_) return false;
        copyReport(report, packet, dmaTag);
        active_ = false;
        return true;
    }

    // Snapshot/cancel are also EE-owner-only. A snapshot does not establish a
    // completed producer/consumer interval; consumedPacket remains zero.
    bool snapshot(EeGifProbeReport& report) const noexcept {
        if (!active_) return false;
        copyReport(report, 0, 0);
        return true;
    }
    void cancel() noexcept { active_ = false; }
};

inline EeGifWriteProbe eeGifWriteProbeState;
inline EeGifWriteProbe& eeGifWriteProbe() noexcept { return eeGifWriteProbeState; }

DRAKENGARD_GIF_PROBE_INLINE void traceEeGifWriteAttempt(uint32_t physicalAddress, uint32_t width,
                                  uint64_t low, uint64_t high,
                                  uint32_t pc, uint32_t ra, uint32_t sp = 0) noexcept {
    if (eeGifWriteProbeState.matches(physicalAddress, width))
        eeGifWriteProbeState.observe(physicalAddress, width, low, high, pc, ra, sp);
}

inline void writeEeGifProbeJson(std::ostream& out, const EeGifProbeReport& report) {
    out << "{\"source\":\"cpu_store_attempts_only\",\"generation\":" << report.generation
        << ",\"armed_packet\":" << report.armedPacket << ",\"consumed_packet\":" << report.consumedPacket
        << ",\"address\":" << report.address << ",\"bytes\":" << report.bytes
        << ",\"armed_dma_tag\":" << report.armedDmaTag << ",\"consumed_dma_tag\":" << report.consumedDmaTag
        << ",\"total_attempts\":" << report.totalAttempts << ",\"dropped_attempts\":" << report.droppedAttempts
        << ",\"attempts\":[";
    for (size_t i = 0; i < report.count; ++i) {
        if (i) out << ',';
        const auto& a = report.attempts[i];
        out << "{\"sequence\":" << a.sequence << ",\"pc\":" << a.pc << ",\"ra\":" << a.ra
            << ",\"sp\":" << a.sp << ",\"address\":" << a.address << ",\"width\":" << a.width
            << ",\"low\":" << a.low << ",\"high\":" << a.high << '}';
    }
    out << "]}";
}
#undef DRAKENGARD_GIF_PROBE_INLINE
}
