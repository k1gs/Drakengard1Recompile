#pragma once
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>

struct EmptyGifChainResult { uint32_t madr,tadr,tag,retired; };

// A zero-length source chain still fetches real DMAtags and terminates DMA.
// Reviewed against PCSX2 v2.8.2 Hw.cpp and Gif.cpp. Nonempty payloads and the
// CALL/RET stack are left to the existing transfer engine, with no mutation here.
inline std::optional<EmptyGifChainResult> inspectEmptyGifChain(
    std::span<const uint8_t> ram,uint32_t control,uint32_t address) {
    if ((control&0x17f)!=0x105) return std::nullopt; // RAM->GIF chain, STR, no ASP/TTE.
    for (unsigned count=1;count<=256;++count) {
        if ((address&15) || address>ram.size() || ram.size()-address<16)
            throw std::runtime_error("Empty GIF chain tag outside aligned RAM");
        uint32_t words[2]; std::memcpy(words,ram.data()+address,8);
        if (words[0]&0xffff) return std::nullopt;
        const unsigned kind=(words[0]>>28)&7;
        if (kind==5 || kind==6) return std::nullopt;
        uint32_t next=address+16, source=words[1];
        bool done=kind==0 || kind==7;
        if (kind==1 || kind==2 || kind==7) source=address+16;
        if (kind==2) next=words[1];
        if (kind==7) next=address;
        done|=(control&0x80) && (words[0]&0x80000000);
        if (done) return EmptyGifChainResult{source,next,words[0],count};
        address=next;
    }
    throw std::runtime_error("Empty GIF chain did not reach a terminal tag within its bound");
}
