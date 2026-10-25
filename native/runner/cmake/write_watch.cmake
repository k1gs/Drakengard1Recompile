# Shared build-local runtime macro shadow. Keep the pinned source read-only;
# instrument write traces and correct reviewed COP1 compare/conversion results.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/include/ps2_runtime_macros.h" write_watch_source)
set(write_watch_header "#pragma once\n#include \"${REPO_ROOT}/native/runner/ee_write_watch.h\"\n#include \"${REPO_ROOT}/native/runner/ee_fpu_compare.h\"\n#include \"${REPO_ROOT}/native/runner/ee_fpu_convert.h\"\n")
foreach(compare_pair IN ITEMS
    "#define FPU_C_EQ_S(a, b) ((float)(a) == (float)(b))@@#define FPU_C_EQ_S(a, b) drakengard::eeFpuEqual((a), (b))"
    "#define FPU_C_OLT_S(a, b) ((float)(a) < (float)(b))@@#define FPU_C_OLT_S(a, b) drakengard::eeFpuLess((a), (b))"
    "#define FPU_C_OLE_S(a, b) ((float)(a) <= (float)(b))@@#define FPU_C_OLE_S(a, b) drakengard::eeFpuLessEqual((a), (b))"
    "#define FPU_CVT_W_S(a) ((int32_t)nearbyintf((float)(a)))@@#define FPU_CVT_W_S(a) drakengard::eeFpuConvertWord((a))")
    string(FIND "${compare_pair}" "@@" compare_separator)
    string(SUBSTRING "${compare_pair}" 0 ${compare_separator} compare_old)
    math(EXPR compare_new_start "${compare_separator}+2")
    string(SUBSTRING "${compare_pair}" ${compare_new_start} -1 compare_new)
    string(FIND "${write_watch_source}" "${compare_old}" compare_at)
    if(compare_at LESS 0)
        message(FATAL_ERROR "Pinned scalar FPU macro changed")
    endif()
    string(REPLACE "${compare_old}" "${compare_new}" write_watch_source "${write_watch_source}")
endforeach()
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
# Quoted includes in generated code cannot override the absolute macro header
# already loaded by the pinned runner's PCH. Shadow that same PCH entry too.
get_target_property(write_watch_pch ps2EntryRunner PRECOMPILE_HEADERS)
if(write_watch_pch)
    list(FIND write_watch_pch "${CMAKE_SOURCE_DIR}/ps2xRuntime/include/ps2_runtime_macros.h" write_watch_pch_index)
    if(write_watch_pch_index LESS 0)
        message(FATAL_ERROR "Pinned runner macro PCH contract changed")
    endif()
    list(REMOVE_AT write_watch_pch ${write_watch_pch_index})
    list(INSERT write_watch_pch ${write_watch_pch_index}
        "${CMAKE_BINARY_DIR}/private_headers/ps2_runtime_macros.h")
    set_property(TARGET ps2EntryRunner PROPERTY PRECOMPILE_HEADERS "${write_watch_pch}")
endif()
