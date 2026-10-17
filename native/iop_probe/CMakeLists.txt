cmake_minimum_required(VERSION 3.21)
project(IopImageProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(IOP_ROOT "${REPO_ROOT}/tools/ps2recomp/ps2xIOP")
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
add_subdirectory("${IOP_ROOT}" "${CMAKE_BINARY_DIR}/ps2_iop")
add_executable(iop_image_probe main.cpp)
target_include_directories(iop_image_probe PRIVATE "${IOP_ROOT}/src/emulator")
target_link_libraries(iop_image_probe PRIVATE ps2_iop)

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
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/private_isolated_cpu.cpp"
    CONTENT "${isolated_cpu}" @ONLY)
add_library(iop_bootstrap_primitives STATIC
    "${IOP_ROOT}/src/ps2_path.cpp"
    "${IOP_ROOT}/src/emulator/core/iop_memory.cpp"
    "${CMAKE_BINARY_DIR}/private_isolated_cpu.cpp"
    "${IOP_ROOT}/src/emulator/imports/iop_imports.cpp"
    "${CMAKE_BINARY_DIR}/private_bootstrap_loader.cpp")
target_include_directories(iop_bootstrap_primitives PUBLIC
    "${IOP_ROOT}/include" "${IOP_ROOT}/src/emulator"
    "${IOP_ROOT}/src/emulator/services" "${IOP_ROOT}/src/emulator/core")
add_executable(iop_sysmem_probe sysmem.cpp)
target_link_libraries(iop_sysmem_probe PRIVATE iop_bootstrap_primitives)
add_executable(iop_bootstrap_probe bootstrap.cpp)
target_link_libraries(iop_bootstrap_probe PRIVATE iop_bootstrap_primitives)
add_executable(iop_bootstrap_checks selftest.cpp)
target_link_libraries(iop_bootstrap_checks PRIVATE iop_bootstrap_primitives)
enable_testing()
add_test(NAME strict_bare_iop_calls COMMAND iop_bootstrap_checks)
