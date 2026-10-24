# Original toSPR chain implementation; keep the pinned upstream checkout read-only.
set(spr_mode_anchor "    const uint32_t mode = (chcr >> 2u) & 0x3u;\n    if (mode != 0u)\n        return false;")
set(spr_mode_replacement [=[    const uint32_t mode = (chcr >> 2u) & 0x3u;
    if (mode == 1u && channelBase == kSprToChannel) {
        const auto get=[&](uint32_t address) {
            const auto found=m_ioRegisters.find(address);
            return found==m_ioRegisters.end()?0u:found->second;
        };
        drakengard::EeToSprChainState state{chcr,get(channelBase+0x10),get(channelBase+0x20),
                                           get(channelBase+0x30),get(channelBase+0x80)};
        const uint32_t firstTag=state.tag,firstScratch=state.scratch&0x3ff0u;
        const bool enabled=(get(0x1000e000u)&1u) &&
            !(drakengard::firmwareDmaRead(0x1000f520u).value_or(get(0x1000f520u))&0x10000u);
        const auto publish=[&] {
            m_ioRegisters[channelBase]=state.control;
            m_ioRegisters[channelBase+0x10]=state.address;
            m_ioRegisters[channelBase+0x20]=state.count;
            m_ioRegisters[channelBase+0x30]=state.tag;
            m_ioRegisters[channelBase+0x80]=state.scratch;
        };
        drakengard::EeToSprChainResult result;
        try {
            result=drakengard::transferEeToSprChain(state,{m_rdram,PS2_RAM_SIZE},
                                                    {m_scratchpad,PS2_SCRATCHPAD_SIZE},enabled);
        } catch (...) { publish(); throw; }
        publish();
        if (result.completed) {
            completeDmacChannel(channelBase,9u);
            static uint64_t completions{};
            const auto count=++completions;
            if (count<=16 || (count&(count-1))==0) {
                uint64_t hash=14695981039346656037ull;
                for (unsigned i=0;i<PS2_SCRATCHPAD_SIZE;++i) {
                    hash^=m_scratchpad[i]; hash*=1099511628211ull;
                }
                std::cerr << "[ee-spr-chain-complete] first_tag=0x" << std::hex << firstTag
                          << " madr=0x" << state.address << " tadr=0x" << state.tag
                          << " first_sadr=0x" << firstScratch << " sadr=0x" << state.scratch
                          << " chcr=0x" << state.control << " qwc=0x" << state.count
                          << " tags=0x" << result.tags << " transferred_qw=0x" << result.quadwords
                          << " scratch_fnv1a64=0x" << hash << std::dec << " count=" << count << '\n';
            }
        }
        return true;
    }
    if (mode != 0u)
        throw std::runtime_error("Unsupported scratchpad DMA mode/direction");]=])
string(REPLACE "${spr_mode_anchor}" "${spr_mode_replacement}" memory_spr_chain "${memory_original}")
if(memory_spr_chain STREQUAL memory_original)
    message(FATAL_ERROR "Pinned scratchpad DMA mode contract changed")
endif()
set(memory_original "#include \"ee_spr_chain.h\"\n${memory_spr_chain}")

# A start blocked by DMAE/suspend retains STR. Service it when the existing
# scheduler consumes completions; never mutate STR merely because CHCR is read.
set(spr_service_anchor "std::vector<uint32_t> PS2Memory::consumeCompletedDmacCauses()\n{")
string(REPLACE "${spr_service_anchor}"
    "${spr_service_anchor}\n    if (const auto active=m_ioRegisters.find(0x1000d400u); active!=m_ioRegisters.end() && (active->second&0x10cu)==0x104u)\n        tryProcessScratchpadDma(0x1000d400u,active->second);"
    memory_spr_service "${memory_original}")
if(memory_spr_service STREQUAL memory_original)
    message(FATAL_ERROR "Pinned scratchpad DMA service contract changed")
endif()
set(memory_original "${memory_spr_service}")
