include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/pinned_patch.cmake")

function(drakengard_trace_caim_memory variable)
    set(text "${${variable}}")
    drakengard_replace_pinned(text
        "                    std::vector<uint8_t> chainBuf;"
        "                    const bool caimChain=drakengard::caimVifTrace.begin(channelBase,tagAddr,chcr,m_rdram+0x665bc0u);\n                    std::vector<uint8_t> chainBuf;"
        "Caim chain producer boundary")
    drakengard_replace_pinned(text
        "                        pt.chainData = std::move(chainBuf);"
        "                        pt.chainData = std::move(chainBuf);\n                        drakengard::caimVifTrace.queue(caimChain,pt.chainData.data(),static_cast<uint32_t>(pt.chainData.size()));\n                        drakengard::caimVifTrace.queueOutput(channelBase,pt.chainData.data(),static_cast<uint32_t>(pt.chainData.size()));"
        "Caim queued stream identity")
    drakengard_replace_pinned(text
        "            const uint32_t qwc = m_ioRegisters[channelBase + 0x20];"
        "            const uint32_t qwc = m_ioRegisters[channelBase + 0x20];\n            if(drakengard::caimVifTrace.following()) {\n                const auto diagnosticRegister=[&](uint32_t a) { const auto it=m_ioRegisters.find(a);return it==m_ioRegisters.end()?0u:it->second; };\n                drakengard::caimVifTrace.dma(channelBase,value,madr,qwc,diagnosticRegister(channelBase+0x30),diagnosticRegister(channelBase+0x80));\n            }"
        "Caim subsequent DMA boundary")
    drakengard_replace_pinned(text
        "            std::memcpy(m_rdram + mainOffset + copied, m_scratchpad + scratchOffset, chunk);"
        "            std::memcpy(m_rdram + mainOffset + copied, m_scratchpad + scratchOffset, chunk);\n            drakengard::caimVifTrace.output(mainOffset+copied,m_rdram+mainOffset+copied,chunk,scratchOffset);"
        "Caim actual scratchpad output copy")
    drakengard_replace_pinned(text
        "                            chainBuf.insert(chainBuf.end(), base2 + src, base2 + src + chunk);"
        "                            if(!scratch)drakengard::caimVifTrace.outputSource(channelBase,src,chunk,base2+src,static_cast<uint32_t>(chainBuf.size()));\n                            chainBuf.insert(chainBuf.end(), base2 + src, base2 + src + chunk);"
        "Caim exported range DMA consumption")
    set(${variable} "${text}" PARENT_SCOPE)
endfunction()

function(drakengard_trace_caim_vif variable)
    set(text "${${variable}}")
    foreach(vif RANGE 0 1)
        set(entry "void PS2Memory::processVIF${vif}Data(const uint8_t *data, uint32_t sizeBytes)\n{")
        drakengard_replace_pinned(text "${entry}"
            "${entry}\n    drakengard::CaimVifConsumerScope caimConsumer(data,sizeBytes,${vif});"
            "Caim VIF${vif} consumer identity")
    endforeach()
    drakengard_replace_pinned(text
        "    drakengard::CaimVifConsumerScope caimConsumer(data,sizeBytes,1);"
        "    drakengard::CaimVifConsumerScope caimConsumer(data,sizeBytes,1);\n    drakengard::CaimOutputVifScope caimOutput(data,sizeBytes);"
        "Caim exported VIF1 consumer identity")
    drakengard_replace_pinned(text
        "            if (m_vu1MscalCallback)\n                m_vu1MscalCallback(startPC, runTop, runItop);"
        "            if (m_vu1MscalCallback) {\n                drakengard::caimVifTrace.vu1(false,cmd,runTop,runItop,m_vu1Code,m_vu1Data);\n                m_vu1MscalCallback(startPC, runTop, runItop);\n                drakengard::caimVifTrace.vu1(true,cmd,runTop,runItop,m_vu1Code,m_vu1Data);\n            }"
        "Caim VU1 call boundary")
    drakengard_replace_pinned(text
        "            if (m_vu1MscntCallback)\n                m_vu1MscntCallback(runTop, runItop);"
        "            if (m_vu1MscntCallback) {\n                drakengard::caimVifTrace.vu1(false,cmd,runTop,runItop,m_vu1Code,m_vu1Data);\n                m_vu1MscntCallback(runTop, runItop);\n                drakengard::caimVifTrace.vu1(true,cmd,runTop,runItop,m_vu1Code,m_vu1Data);\n            }"
        "Caim VU1 resume boundary")
    # Work on each function independently: identical POS increments occur in both.
    string(FIND "${text}" "void PS2Memory::processVIF1Data(uint32_t" split)
    if(split LESS 0)
        message(FATAL_ERROR "Pinned VIF function split changed")
    endif()
    string(SUBSTRING "${text}" 0 ${split} zero)
    string(SUBSTRING "${text}" ${split} -1 one)
    foreach(vif RANGE 0 1)
        if(vif EQUAL 0)
            drakengard_replace_pinned(zero
                "drakengard::unpackVif0(cmd,vif0_regs,data,sizeBytes,pos,m_vu0Data,[](bool,uint32_t){})"
                "drakengard::unpackVif0(cmd,vif0_regs,data,sizeBytes,pos,m_vu0Data,[&](bool after,uint32_t bytes) { drakengard::caimVifTrace.unpack(after,cmd,vif0_regs,data+pos,bytes,m_vu0Data,PS2_VU0_DATA_SIZE,m_vu0Data && bytes<=sizeBytes-pos); })"
                "Caim VIF0 shared decompression observation")
            continue()
        else()
            set(body "${one}")
            set(anchor "            if (m_vu1Data && totalBytes > 0 && pos + totalBytes <= sizeBytes)")
        endif()
        set(args "cmd,vif${vif}_regs,data+pos,totalBytes,m_vu${vif}Data,PS2_VU${vif}_DATA_SIZE,m_vu${vif}Data && totalBytes<=sizeBytes-pos")
        drakengard_replace_pinned(body "${anchor}"
            "            drakengard::caimVifTrace.unpack(false,${args});\n${anchor}"
            "Caim VIF${vif} pre UNPACK")
        drakengard_replace_pinned(body "            pos += totalBytes;"
            "            drakengard::caimVifTrace.unpack(true,${args});\n            pos += totalBytes;"
            "Caim VIF${vif} post UNPACK")
        set(one "${body}")
    endforeach()
    set(${variable} "${zero}${one}" PARENT_SCOPE)
endfunction()

function(drakengard_trace_caim_micro variable)
    set(text "${${variable}}")
    drakengard_replace_pinned(text "    m_vu0.reset();\n    copyVu0ContextToState(ctx, m_vu0.state());"
        "    drakengard::caimVifTrace.micro(false,startPC,vu0Code,vu0Data,ctx->vi[1]);\n    m_vu0.reset();\n    copyVu0ContextToState(ctx, m_vu0.state());"
        "Caim VU0 execution entry")
    drakengard_replace_pinned(text "    copyVu0StateToContext(m_vu0.state(), ctx);"
        "    copyVu0StateToContext(m_vu0.state(), ctx);\n    drakengard::caimVifTrace.micro(true,startPC,vu0Code,vu0Data,ctx->vi[1]);"
        "Caim VU0 execution return")
    set(${variable} "${text}" PARENT_SCOPE)
endfunction()
