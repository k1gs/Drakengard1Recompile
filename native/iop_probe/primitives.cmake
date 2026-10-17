function(drakengard_add_iop_primitives target)
execute_process(COMMAND git -C "${IOP_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE status)
if(NOT status EQUAL 0 OR NOT revision STREQUAL "eb8688b2fd191129f103a39543002c005b33409c")
    message(FATAL_ERROR "Install the pinned PS2Recomp revision before using this optional probe")
endif()
execute_process(COMMAND git -C "${IOP_ROOT}" status --porcelain
    OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE status)
if(NOT status EQUAL 0 OR NOT changes STREQUAL "")
    message(FATAL_ERROR "The optional IOP probe requires a clean pinned checkout")
endif()
# Isolate the bare bootstrap experiment from upstream HLE services. UDNL places
# SYSMEM at start-address + 0x30, which is only 16-byte aligned. Preserve every
# other loader operation and stop configuration if the pinned context changes.
file(READ "${IOP_ROOT}/src/emulator/services/iop_module_loader.cpp" loader_source)
string(REPLACE "base = alignUp(moduleCursor, 0x100u);" "base = alignUp(moduleCursor, 0x10u);"
    bootstrap_loader "${loader_source}")
if(bootstrap_loader STREQUAL loader_source)
    message(FATAL_ERROR "Pinned IOP loader alignment contract changed")
endif()
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/private_bootstrap_loader.cpp"
    CONTENT "${bootstrap_loader}" @ONLY)
# Match functional reference cache isolation. Bare-call validation permits only
# reviewed zero stores into cache lines; these must never alter backing RAM.
file(READ "${IOP_ROOT}/src/emulator/core/iop_cpu.cpp" cpu_source)
string(REPLACE "m_memory.write32(address, cpu.gpr[rt]);"
    "if ((cpu.cop0[12] & 0x10000u) == 0u) m_memory.write32(address, cpu.gpr[rt]);"
    isolated_cpu "${cpu_source}")
if(isolated_cpu STREQUAL cpu_source)
    message(FATAL_ERROR "Pinned IOP word-store isolation contract changed")
endif()
string(REPLACE "cpu.cop0[rd] = cpu.gpr[rt];"
    "{ if (rd != 15u) cpu.cop0[rd] = cpu.gpr[rt]; }"
    identified_cpu "${isolated_cpu}")
if(identified_cpu STREQUAL isolated_cpu)
    message(FATAL_ERROR "Pinned IOP read-only CP0.PRId contract changed")
endif()
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/private_isolated_cpu.cpp"
    CONTENT "${identified_cpu}" @ONLY)
add_library(${target} STATIC
    "${IOP_ROOT}/src/ps2_path.cpp"
    "${IOP_ROOT}/src/emulator/core/iop_memory.cpp"
    "${CMAKE_BINARY_DIR}/private_isolated_cpu.cpp"
    "${IOP_ROOT}/src/emulator/imports/iop_imports.cpp"
    "${REPO_ROOT}/native/runner/iso_media.cpp"
    "${REPO_ROOT}/native/runner/cdvd_device.cpp"
    "${REPO_ROOT}/native/runner/iop_timers.cpp"
    "${CMAKE_BINARY_DIR}/private_bootstrap_loader.cpp")
target_include_directories(${target} PUBLIC
    "${IOP_ROOT}/include" "${IOP_ROOT}/src/emulator"
    "${IOP_ROOT}/src/emulator/services" "${IOP_ROOT}/src/emulator/core" "${REPO_ROOT}/native/runner")
target_compile_features(${target} PUBLIC cxx_std_20)

endfunction()
