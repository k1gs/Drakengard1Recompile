# Included by overlay.cmake inside drakengard_configure_runner.
# Route reviewed EE register accesses through the strict IOP/firmware devices.
# Keep diagnostics and the original fallback for accesses those devices do not own.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/ps2_memory.cpp" memory_original)
set(memory_entry "bool PS2Memory::writeIORegister(uint32_t address, uint32_t value)\n{")
string(REPLACE "${memory_entry}"
    "${memory_entry}\n    diagnoseEeSifRegisterWrite(address, value);\n    if (address == 0x1000c400u && (value & 0x100u) && drakengard::strictIopActive()) { const auto control = readIORegister(0x1000e000u); std::cerr << \"[strict-ee-dmac] d_ctrl=0x\" << std::hex << control << std::dec << '\\n'; if (!(control & 1u)) throw drakengard::StrictDeviceError(\"EE SIF1 DMA requires actual guest DMAE enable\"); }\n    if (drakengard::firmwareDmaWrite(address, value)) { if (drakengard::takeFirmwareDmaCompletion()) completeDmacChannel(0x1000c400u, 6u); return true; }\n    if (drakengard::cooperativeSifWrite(address, value)) return true;"
    memory_traced "${memory_original}")
if(memory_traced STREQUAL memory_original)
    message(FATAL_ERROR "Pinned EE memory register-write entry changed")
endif()
set(memory_read "uint32_t PS2Memory::readIORegister(uint32_t address)\n{")
string(REPLACE "${memory_read}"
    "${memory_read}\n    if (auto value = drakengard::firmwareDmaRead(address)) return *value;\n    if (auto value = drakengard::cooperativeSifRead(address)) return *value;"
    memory_bridged "${memory_traced}")
if(memory_bridged STREQUAL memory_traced)
    message(FATAL_ERROR "Pinned EE memory register-read entry changed")
endif()
set(memory_file "${CMAKE_BINARY_DIR}/private_traced_Memory.cpp")
set(pending_entry "std::vector<uint32_t> PS2Memory::consumeCompletedDmacCauses()\n{")
string(REPLACE "${pending_entry}"
    "${pending_entry}\n    if (drakengard::takeFirmwareSif0Completion()) { std::cerr << \"[strict-ee-sif0-complete] queue actual DMAC cause5\\n\"; completeDmacChannel(0x1000c000u, 5u); }"
    memory_pending "${memory_bridged}")
if(memory_pending STREQUAL memory_bridged)
    message(FATAL_ERROR "Pinned EE DMA completion-consumer entry changed")
endif()
file(CONFIGURE OUTPUT "${memory_file}"
    CONTENT "#include \"strict_device_error.h\"\n#include \"sif_diagnostic.h\"\n#include \"iop_bridge.h\"\n${memory_pending}" @ONLY)
set_source_files_properties("${memory_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/ps2_memory[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${memory_file}")
