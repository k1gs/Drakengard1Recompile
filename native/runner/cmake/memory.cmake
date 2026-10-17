# Included by overlay.cmake inside drakengard_configure_runner.
# Route reviewed EE register accesses through the strict IOP/firmware devices.
# Keep diagnostics and the original fallback for accesses those devices do not own.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/ps2_memory.cpp" memory_original)
set(memory_entry "bool PS2Memory::writeIORegister(uint32_t address, uint32_t value)\n{")
string(REPLACE "${memory_entry}"
    "${memory_entry}\n    diagnoseEeSifRegisterWrite(address, value);\n    if (drakengard::firmwareDmaWrite(address, value)) return true;\n    if (drakengard::cooperativeSifWrite(address, value)) return true;"
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
file(CONFIGURE OUTPUT "${memory_file}"
    CONTENT "#include \"sif_diagnostic.h\"\n#include \"iop_bridge.h\"\n${memory_bridged}" @ONLY)
set_source_files_properties("${memory_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/ps2_memory[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${memory_file}")
