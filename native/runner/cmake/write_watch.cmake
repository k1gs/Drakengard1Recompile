# Shared build-local runtime macro shadow. Keep the pinned source read-only;
# instrument write traces and correct reviewed COP1 compare/conversion results.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/include/ps2_runtime_macros.h" write_watch_source)
set(write_watch_header "#pragma once\n#include \"${REPO_ROOT}/native/runner/ee_write_watch.h\"\n#include \"${REPO_ROOT}/native/runner/fpu/ee_fpu_compare.h\"\n#include \"${REPO_ROOT}/native/runner/fpu/ee_fpu_convert.h\"\n")
include("${CMAKE_CURRENT_LIST_DIR}/pinned_patch.cmake")
drakengard_replace_pinned(write_watch_source
    "#define FPU_C_EQ_S(a, b) ((float)(a) == (float)(b))"
    "#define FPU_C_EQ_S(a, b) drakengard::eeFpuEqual((a), (b))" "scalar FPU macro")
drakengard_replace_pinned(write_watch_source
    "#define FPU_C_OLT_S(a, b) ((float)(a) < (float)(b))"
    "#define FPU_C_OLT_S(a, b) drakengard::eeFpuLess((a), (b))" "scalar FPU macro")
drakengard_replace_pinned(write_watch_source
    "#define FPU_C_OLE_S(a, b) ((float)(a) <= (float)(b))"
    "#define FPU_C_OLE_S(a, b) drakengard::eeFpuLessEqual((a), (b))" "scalar FPU macro")
drakengard_replace_pinned(write_watch_source
    "#define FPU_CVT_W_S(a) ((int32_t)nearbyintf((float)(a)))"
    "#define FPU_CVT_W_S(a) drakengard::eeFpuConvertWord((a))" "scalar FPU macro")

foreach(write_watch_width IN ITEMS 1 2 4 8 16)
    math(EXPR write_watch_bits "${write_watch_width} * 8")
    if(write_watch_width EQUAL 16)
        set(write_watch_values "_lo, _hi")
    else()
        set(write_watch_values "(uint${write_watch_bits}_t)(val), 0u")
    endif()
    set(write_watch_arguments "rdram, _addr, ${write_watch_width}u, ${write_watch_values}, \"WRITE${write_watch_bits}\", ctx")
    set(write_watch_original_trace "ps2TraceGuestWrite(${write_watch_arguments});")
    drakengard_replace_pinned(write_watch_source "${write_watch_original_trace}"
        "drakengard::traceEeWatchedWrite(${write_watch_arguments}); ${write_watch_original_trace}"
        "write macro trace anchor")
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
