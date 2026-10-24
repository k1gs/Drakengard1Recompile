# Included by overlay.cmake inside drakengard_configure_runner.
# Route reviewed EE register accesses through the strict IOP/firmware devices.
# Keep diagnostics and the original fallback for accesses those devices do not own.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/ps2_memory.cpp" memory_original)
set(empty_gif_anchor "            if (tryProcessScratchpadDma(channelBase, value))")
string(REPLACE "${empty_gif_anchor}"
    "            if (channelBase==0x1000a000u) {\n                if (auto empty=inspectEmptyGifChain({m_rdram,PS2_RAM_SIZE},value,m_ioRegisters[channelBase+0x30])) {\n                    m_ioRegisters[channelBase]=(value&0xffffu)|(empty->tag&0xffff0000u);\n                    m_ioRegisters[channelBase+0x10]=empty->madr;\n                    m_ioRegisters[channelBase+0x20]=0;\n                    m_ioRegisters[channelBase+0x30]=empty->tadr;\n                    completeDmacChannel(channelBase,2u);\n                    std::cerr << \"[empty-gif-chain-complete] tags=\" << empty->retired << \" tadr=0x\" << std::hex << empty->tadr << std::dec << '\\n';\n                    return true;\n                }\n            }\n${empty_gif_anchor}"
    memory_empty_gif "${memory_original}")
if(memory_empty_gif STREQUAL memory_original)
    message(FATAL_ERROR "Pinned graphics DMA start contract changed")
endif()
set(memory_original "${memory_empty_gif}")
include("${REPO_ROOT}/native/runner/cmake/ipu.cmake")
set(memory_entry "bool PS2Memory::writeIORegister(uint32_t address, uint32_t value)\n{")
string(REPLACE "${memory_entry}"
    "${memory_entry}\n    diagnoseEeSifRegisterWrite(address, value);\n    diagnoseEeIpuWrite(address, value);\n    if (drakengard::eeIpuWrite(address, value)) return true;\n    if (drakengard::eeIntcWrite(address, value)) return true;\n    if (address == 0x1000c400u && (value & 0x100u) && drakengard::strictIopActive()) { const auto control = readIORegister(0x1000e000u); std::cerr << \"[strict-ee-dmac] d_ctrl=0x\" << std::hex << control << std::dec << '\\n'; if (!(control & 1u)) throw drakengard::StrictDeviceError(\"EE SIF1 DMA requires actual guest DMAE enable\"); }\n    if (drakengard::firmwareDmaWrite(address, value)) { if (drakengard::takeFirmwareDmaCompletion()) completeDmacChannel(0x1000c400u, 6u); return true; }\n    if (drakengard::cooperativeSifWrite(address, value)) return true;"
    memory_traced "${memory_original}")
if(memory_traced STREQUAL memory_original)
    message(FATAL_ERROR "Pinned EE memory register-write entry changed")
endif()
set(memory_read "uint32_t PS2Memory::readIORegister(uint32_t address)\n{")
string(REPLACE "${memory_read}"
    "${memory_read}\n    if (auto value = drakengard::eeIpuRead(address)) return *value;\n    if (auto value = drakengard::eeIntcRead(address)) return *value;\n    if (auto value = drakengard::firmwareDmaRead(address)) return *value;\n    if (auto value = drakengard::cooperativeSifRead(address)) return *value;"
    memory_bridged "${memory_traced}")
if(memory_bridged STREQUAL memory_traced)
    message(FATAL_ERROR "Pinned EE memory register-read entry changed")
endif()
set(intc_read64_anchor "    // 64-bit IO read: compose from the two adjacent 32-bit IO register slots")
string(REPLACE "${intc_read64_anchor}"
    "    if (auto value = drakengard::eeIntcRead(physAddr)) return *value;\n\n${intc_read64_anchor}"
    memory_intc64 "${memory_bridged}")
if(memory_intc64 STREQUAL memory_bridged)
    message(FATAL_ERROR "Pinned EE memory 64-bit register-read entry changed")
endif()
set(memory_bridged "${memory_intc64}")
set(memory_file "${CMAKE_BINARY_DIR}/private_traced_Memory.cpp")
set(pending_entry "std::vector<uint32_t> PS2Memory::consumeCompletedDmacCauses()\n{")
string(REPLACE "${pending_entry}"
    "${pending_entry}\n    const auto ipuCauses=drakengard::takeEeIpuDmaCompletions();\n    if (ipuCauses & 8u) completeDmacChannel(0x1000b000u,3u);\n    if (ipuCauses & 16u) completeDmacChannel(0x1000b400u,4u);\n    if (drakengard::takeFirmwareSif0Completion()) { std::cerr << \"[strict-ee-sif0-complete] queue actual DMAC cause5\\n\"; completeDmacChannel(0x1000c000u, 5u); }"
    memory_pending "${memory_bridged}")
if(memory_pending STREQUAL memory_bridged)
    message(FATAL_ERROR "Pinned EE DMA completion-consumer entry changed")
endif()
set(dma_fake_read "                uint32_t channelStatus = m_ioRegisters[address] & ~0x100u;\n                m_ioRegisters[address] = channelStatus;\n                return channelStatus;")
string(REPLACE "${dma_fake_read}" "                return m_ioRegisters[address];"
    memory_real_status "${memory_pending}")
if(memory_real_status STREQUAL memory_pending)
    message(FATAL_ERROR "Pinned DMA STR read contract changed")
endif()
file(CONFIGURE OUTPUT "${memory_file}"
    CONTENT "#include \"strict_device_error.h\"\n#include \"sif_diagnostic.h\"\n#include \"ipu_diagnostic.h\"\n#include \"iop_bridge.h\"\n#include \"empty_gif_chain.h\"\n${memory_real_status}" @ONLY)
set_source_files_properties("${memory_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/ps2_memory[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${memory_file}")
