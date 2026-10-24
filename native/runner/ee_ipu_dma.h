#pragma once
#include "ee_ipu.h"
#include <array>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

namespace drakengard {
class EeIpuDma {
    static constexpr uint32_t ScratchpadAlias = 0x70000000u;
    static constexpr uint32_t ScratchpadSize = 0x4000u;
    struct Channel {
        uint32_t control{},address{},count{},tag{},return0{},return1{};
        bool completion{};
    };
    std::array<Channel,2> channels{}; // from-IPU channel3, to-IPU channel4.
    static unsigned index(uint32_t address) { return address>=0x1000b400u?1u:0u; }
    static uint32_t offset(uint32_t address) { return address&0xffu; }
    static void complete(Channel& channel) { channel.control&=~0x100u; channel.completion=true; }
    static std::optional<uint32_t> scratchOffset(uint32_t address) {
        // EE DMA uses SPR (MADR bit 31) for the local scratchpad.  The
        // physical 0x70000000 window is also accepted by the reviewed runtime
        // address helper; do not feed either form through the RDRAM alias mask.
        if (address & 0x80000000u) {
            const auto offset=address&0x7fffffffu;
            return offset<ScratchpadSize ? std::optional<uint32_t>(offset) : std::nullopt;
        }
        if (address>=ScratchpadAlias && address<ScratchpadAlias+ScratchpadSize)
            return address-ScratchpadAlias;
        return std::nullopt;
    }
    static uint32_t mainOffset(uint32_t address) {
        // Normal EE DMA accepts the direct KSEG/physical forms, but a
        // scratchpad alias is never silently treated as RDRAM.
        if (address & 0x80000000u)
            throw std::runtime_error("IPU DMA unsupported scratchpad MADR");
        if (scratchOffset(address)) throw std::runtime_error("IPU DMA scratchpad address used as EE RAM");
        if ((address & 0xf0000000u)==0x70000000u)
            throw std::runtime_error("IPU DMA unsupported scratchpad alias");
        return address&0x1fffffffu;
    }
public:
    static bool owns(uint32_t address) {
        return (address>=0x1000b000u && address<0x1000b060u) ||
               (address>=0x1000b400u && address<0x1000b460u);
    }
    uint32_t read(uint32_t address) const {
        const auto& channel=channels[index(address)];
        switch(offset(address)) {
        case 0: return channel.control;
        case 0x10: return channel.address;
        case 0x20: return channel.count;
        case 0x30: return channel.tag;
        case 0x40: return channel.return0;
        case 0x50: return channel.return1;
        default: throw std::runtime_error("Unsupported IPU DMA register read");
        }
    }
    void write(uint32_t address,uint32_t value) {
        auto& channel=channels[index(address)];
        if (offset(address)==0) {
            if (value&0x100u) {
                if (channel.control&0x100u)
                    throw std::runtime_error("IPU DMA start replaced an active transfer");
                if ((value&0xdu)!=(index(address)==1?1u:0u) || (value&0x40u))
                    throw std::runtime_error("Unsupported IPU DMA direction/chain/tag-transfer mode");
                // EE normal-mode zero QWC denotes 65536 quadwords, not an
                // empty completed transfer. The finite FIFO supplies backpressure.
                if (!channel.count) channel.count=0x10000u;
            }
            channel.control=value; return;
        }
        if (channel.control&0x100u) throw std::runtime_error("IPU DMA registers changed during an active transfer");
        switch(offset(address)) {
        case 0x10:
            if (value&15u) throw std::runtime_error("Unaligned IPU DMA address");
            channel.address=value; break;
        case 0x20: channel.count=value&0xffffu; break;
        case 0x30: channel.tag=value; break;
        case 0x40: channel.return0=value; break;
        case 0x50: channel.return1=value; break;
        default: throw std::runtime_error("Unsupported IPU DMA register write");
        }
    }
    void pump(EeIpu& ipu,std::span<uint8_t> ram,std::span<uint8_t> scratchpad,bool enabled) {
        if (!enabled) return;
        auto& from=channels[0];
        auto& to=channels[1];
        // A push may make decoder output available and a pop may make input
        // space available. Alternate one quadword from each side until both
        // sides stall, so neither channel is starved by the other.
        for (;;) {
            bool progress=false;
            if (from.control&0x100u) {
                if (from.count && ipu.outputCount()) {
                    const auto destination=scratchOffset(from.address);
                    auto& target=destination ? scratchpad : ram;
                    const auto offset=destination ? *destination : mainOffset(from.address);
                    if (offset>target.size() || target.size()-offset<16)
                        throw std::runtime_error(destination ? "IPU DMA destination outside bound scratchpad"
                                                              : "IPU DMA destination outside bound EE RAM");
                    std::array<uint8_t,16> quad{};
                    if (!ipu.pop(std::span<uint8_t,16>(quad)))
                        throw std::runtime_error("IPU output count changed before pop");
                    std::memcpy(target.data()+offset,quad.data(),quad.size());
                    from.address+=16; --from.count; progress=true;
                }
                if (!from.count) complete(from);
            }
            if (to.control&0x100u) {
                if (to.count && ipu.canAcceptInput()) {
                    if (scratchOffset(to.address)) throw std::runtime_error("Unsupported IPU DMA scratchpad source");
                    const auto offset=mainOffset(to.address);
                    if (offset>ram.size() || ram.size()-offset<16)
                        throw std::runtime_error("IPU DMA source outside bound EE RAM");
                    if (ipu.push(std::span<const uint8_t,16>(ram.data()+offset,16))) {
                        to.address+=16; --to.count; progress=true;
                    }
                }
                if (!to.count) complete(to);
            }
            if (!progress) break;
        }
    }
    void pump(EeIpu& ipu,std::span<uint8_t> ram,bool enabled) {
        std::span<uint8_t> noScratch;
        pump(ipu,ram,noScratch,enabled);
    }
    uint32_t takeCompletions() {
        uint32_t causes=0;
        for (unsigned i=0;i<channels.size();++i)
            if (std::exchange(channels[i].completion,false)) causes|=1u<<(i+3);
        return causes;
    }
};
}
