# Build-local macro shadow for the opt-in EE write watch. The pinned macro
# source is copied and only the existing trace calls are instrumented.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/include/ps2_runtime_macros.h" write_watch_source)
set(write_watch_header "#pragma once\n#include \"${REPO_ROOT}/native/runner/ee_write_watch.h\"\n")
foreach(write_watch_pair IN ITEMS
    "ps2TraceGuestWrite(rdram, _addr, 1u, (uint8_t)(val), 0u, \"WRITE8\", ctx);@@drakengard::traceEeWatchedWrite(rdram, _addr, 1u, (uint8_t)(val), 0u, \"WRITE8\", ctx); ps2TraceGuestWrite(rdram, _addr, 1u, (uint8_t)(val), 0u, \"WRITE8\", ctx);"
    "ps2TraceGuestWrite(rdram, _addr, 2u, (uint16_t)(val), 0u, \"WRITE16\", ctx);@@drakengard::traceEeWatchedWrite(rdram, _addr, 2u, (uint16_t)(val), 0u, \"WRITE16\", ctx); ps2TraceGuestWrite(rdram, _addr, 2u, (uint16_t)(val), 0u, \"WRITE16\", ctx);"
    "ps2TraceGuestWrite(rdram, _addr, 4u, (uint32_t)(val), 0u, \"WRITE32\", ctx);@@drakengard::traceEeWatchedWrite(rdram, _addr, 4u, (uint32_t)(val), 0u, \"WRITE32\", ctx); ps2TraceGuestWrite(rdram, _addr, 4u, (uint32_t)(val), 0u, \"WRITE32\", ctx);"
    "ps2TraceGuestWrite(rdram, _addr, 8u, (uint64_t)(val), 0u, \"WRITE64\", ctx);@@drakengard::traceEeWatchedWrite(rdram, _addr, 8u, (uint64_t)(val), 0u, \"WRITE64\", ctx); ps2TraceGuestWrite(rdram, _addr, 8u, (uint64_t)(val), 0u, \"WRITE64\", ctx);"
    "ps2TraceGuestWrite(rdram, _addr, 16u, _lo, _hi, \"WRITE128\", ctx);@@drakengard::traceEeWatchedWrite(rdram, _addr, 16u, _lo, _hi, \"WRITE128\", ctx); ps2TraceGuestWrite(rdram, _addr, 16u, _lo, _hi, \"WRITE128\", ctx);")
    string(FIND "${write_watch_pair}" "@@" write_watch_separator)
    string(SUBSTRING "${write_watch_pair}" 0 ${write_watch_separator} write_watch_old)
    math(EXPR write_watch_new_start "${write_watch_separator}+2")
    string(SUBSTRING "${write_watch_pair}" ${write_watch_new_start} -1 write_watch_new)
    string(FIND "${write_watch_source}" "${write_watch_old}" write_watch_at)
    if(write_watch_at LESS 0)
        message(FATAL_ERROR "Pinned write macro trace anchor changed")
    endif()
    string(REPLACE "${write_watch_old}" "${write_watch_new}" write_watch_source "${write_watch_source}")
endforeach()
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/private_headers")
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/private_headers/ps2_runtime_macros.h"
    CONTENT "${write_watch_header}${write_watch_source}" @ONLY)
target_include_directories(ps2EntryRunner BEFORE PRIVATE "${CMAKE_BINARY_DIR}/private_headers")
