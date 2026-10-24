#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace drakengard {
struct EeToSprChainState {
    uint32_t control{},address{},count{},tag{},scratch{};
};
struct EeToSprChainResult {
    bool completed{};
    uint32_t tags{},quadwords{};
};

// Functional, synchronous RAM -> SPR source-chain transport. The caller owns
// DMAC arbitration and queues cause9 only on the returned completion edge.
// CALL/RET (stack), VU/SPR sources and stall control are not implemented.
inline EeToSprChainResult transferEeToSprChain(EeToSprChainState& state,
        std::span<const uint8_t> ram,std::span<uint8_t> scratchpad,bool enabled) {
    EeToSprChainResult result;
    if (!(state.control&0x100u) || !enabled) return result;
    if ((state.control&0x3fu)!=5u)
        throw std::runtime_error("Unsupported toSPR chain direction/mode/stack");
    if (scratchpad.size()!=0x4000u)
        throw std::runtime_error("toSPR chain requires actual 16-KiB scratchpad");
    // A fresh QWC=0 chain fetches a tag; it is not a 65536-QW normal transfer.
    // In-flight payload restarts require separate end-tag state and are refused.
    if (state.count)
        throw std::runtime_error("Unsupported toSPR chain restart with nonzero QWC");
    const auto requireRam=[&](uint32_t address,uint32_t bytes) {
        if ((address&0x8000000fu) || address>ram.size() || bytes>ram.size()-address)
            throw std::runtime_error("toSPR chain tag/payload outside aligned EE RAM");
    };
    const auto word=[&](uint32_t address) {
        return uint32_t(ram[address])|(uint32_t(ram[address+1])<<8)|
               (uint32_t(ram[address+2])<<16)|(uint32_t(ram[address+3])<<24);
    };
    state.scratch&=0x3ff0u;
    const auto copy=[&](uint32_t source,uint32_t quadwords) {
        while (quadwords) {
            const uint32_t chunk=std::min(quadwords,(0x4000u-state.scratch)/16u);
            std::memcpy(scratchpad.data()+state.scratch,ram.data()+source,chunk*16u);
            source+=chunk*16u;
            state.scratch=(state.scratch+chunk*16u)&0x3ff0u;
            quadwords-=chunk;
        }
    };
    for (unsigned retired=0;retired<65536;++retired) {
        const uint32_t tagAddress=state.tag;
        requireRam(tagAddress,16);
        const uint32_t tag0=word(tagAddress),tag1=word(tagAddress+4);
        const unsigned id=(tag0>>28)&7u;
        if (id==4 || id==5 || id==6)
            throw std::runtime_error("Unsupported toSPR source-chain REFS/CALL/RET tag");
        const uint32_t qwc=tag0&0xffffu;
        const bool inlinePayload=id==1 || id==2 || id==7;
        const uint32_t source=inlinePayload?tagAddress+16u:tag1;
        requireRam(source,qwc*16u);
        // NEXT's ADDR selects the next tag, not the inline payload source.
        const uint32_t next=id==2?tag1:id==1?source+qwc*16u:
                            id==7?tagAddress:tagAddress+16u;
        if (id==2) requireRam(next,16);
        state.control=(state.control&0xffffu)|(tag0&0xffff0000u);
        state.address=source;
        state.count=qwc;
        state.tag=next;
        if (state.control&0x40u) copy(tagAddress,1); // TTE transfers the whole tag to SPR.
        copy(source,qwc);
        state.address+=qwc*16u;
        state.count=0;
        ++result.tags;
        result.quadwords+=qwc+((state.control&0x40u)?1u:0u);
        if (id==0 || id==7 || ((state.control&0x80u) && (tag0&0x80000000u))) {
            state.control&=~0x100u;
            result.completed=true;
            return result;
        }
    }
    throw std::runtime_error("toSPR source chain exceeds bounded 65536-tag traversal");
}
}
