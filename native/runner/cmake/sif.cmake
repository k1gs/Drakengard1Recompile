# Included by overlay.cmake inside drakengard_configure_runner.
# Outgoing EE -> IOP DMA is not an incoming EE SIF0 interrupt. Raise it only
# when the channel has actually delivered an IOP -> EE command reply.
set(sif_original_path "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Stubs/SIF.cpp")
file(READ "${sif_original_path}" sif_original)
string(REPLACE "ps2_syscalls::dispatchDmacHandlersForCause(rdram, runtime, 5u);"
    "if (drakengard::takeSifReplyInterrupt(runtime)) ps2_syscalls::dispatchDmacHandlersForCause(rdram, runtime, 5u);"
    sif_directional "${sif_original}")
if(sif_directional STREQUAL sif_original)
    message(FATAL_ERROR "Pinned SIF DMA interrupt contract changed")
endif()
string(REPLACE "(void)rdram;\n        (void)runtime;\n        setReturnS32(ctx, 0);"
    "(void)rdram; (void)ctx; drakengard::startSifReceive(runtime);"
    sif_started "${sif_directional}")
if(sif_started STREQUAL sif_directional)
    message(FATAL_ERROR "Pinned SIF receive-chain entry contract changed")
endif()
set(sif_directional "${sif_started}")
foreach(reg_entry IN ITEMS
    "void sceSifGetReg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)"
    "void sceSifSetReg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)")
    if(reg_entry MATCHES "sceSifGetReg")
        set(reg_route "drakengard::cooperativeSifGetReg(getRegU32(ctx, 4))")
    else()
        set(reg_route "drakengard::cooperativeSifSetReg(getRegU32(ctx, 4), getRegU32(ctx, 5))")
    endif()
    string(REPLACE "${reg_entry}\n    {"
        "${reg_entry}\n    {\n        if (auto value = ${reg_route}) { setReturnU32(ctx, *value); return; }"
        sif_reg_guarded "${sif_directional}")
    if(sif_reg_guarded STREQUAL sif_directional)
        message(FATAL_ERROR "Pinned HLE SIF register entry changed")
    endif()
    set(sif_directional "${sif_reg_guarded}")
endforeach()
set(sif_file "${CMAKE_BINARY_DIR}/private_directional_SIF.cpp")
file(CONFIGURE OUTPUT "${sif_file}"
    CONTENT "#include \"sif_transport.h\"\n#include \"iop_bridge.h\"\n${sif_directional}" @ONLY)
set_source_files_properties("${sif_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Stubs")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "Stubs/SIF[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${sif_file}")
