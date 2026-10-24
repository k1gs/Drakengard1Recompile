# Included by memory.cmake before the common MMIO hooks are injected.
macro(replace_ipu_memory anchor replacement)
    string(REPLACE "${anchor}" "${replacement}" memory_ipu_next "${memory_original}")
    if(memory_ipu_next STREQUAL memory_original)
        message(FATAL_ERROR "Pinned IPU memory contract changed: ${anchor}")
    endif()
    set(memory_original "${memory_ipu_next}")
endmacro()
set(ipu_read64_anchor "    // 64-bit IO read: compose from the two adjacent 32-bit IO register slots")
replace_ipu_memory("${ipu_read64_anchor}"
    "    if (auto value = drakengard::eeIpuRead64(physAddr)) return *value;\n\n${ipu_read64_anchor}")
set(ipu_write64_anchor "        write32(address, (uint32_t)value);\n        write32(address + 4, (uint32_t)(value >> 32));")
replace_ipu_memory("${ipu_write64_anchor}"
    "        if (drakengard::eeIpuWrite64(physAddr,value)) return;\n${ipu_write64_anchor}")
set(ipu_input_anchor "    if (!scratch && physAddr == 0x10004000u) // VIF0_FIFO")
replace_ipu_memory("${ipu_input_anchor}"
    "    if (!scratch && physAddr == 0x10007010u) {\n        alignas(16) uint8_t bytes[16];\n        _mm_storeu_si128(reinterpret_cast<__m128i*>(bytes),value);\n        drakengard::writeEeIpuInput(std::span<const uint8_t,16>(bytes,16));\n        return;\n    }\n    if (!scratch && physAddr == 0x10007000u) throw drakengard::StrictDeviceError(\"Write to IPU output FIFO\");\n${ipu_input_anchor}")
set(ipu_output_anchor "    // 128-bit reads are primarily for quad-word loads in the EE, which are only valid for RAM areas")
replace_ipu_memory("${ipu_output_anchor}"
    "    if (!scratch && physAddr == 0x10007000u) {\n        alignas(16) uint8_t bytes[16];\n        drakengard::readEeIpuOutput(std::span<uint8_t,16>(bytes,16));\n        return _mm_loadu_si128(reinterpret_cast<const __m128i*>(bytes));\n    }\n    if (!scratch && physAddr > 0x10007000u && physAddr < 0x10007020u)\n        throw drakengard::StrictDeviceError(\"Unsupported IPU output FIFO subregister\");\n${ipu_output_anchor}")
