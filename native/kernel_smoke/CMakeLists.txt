cmake_minimum_required(VERSION 3.21)
project(KernelDispatchSyntheticSmoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# Small interface fixtures test our sparse dispatch and byte gate, not upstream ABI.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixture/runtime")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fixture/ps2_runtime.h" [=[
#pragma once
#include <cstdint>
struct R5900Context { uint32_t pc; uint32_t result; };
struct PS2Runtime { using RecompiledFunction = void(*)(uint8_t*, R5900Context*, PS2Runtime*); };
]=])
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fixture/runtime/ps2_memory.h" [=[
#pragma once
#include <cstdint>
constexpr uint32_t PS2_RAM_SIZE = 32u * 1024u * 1024u;
]=])
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fixture/kernel_recompiled_functions.h" [=[
#pragma once
#include "ps2_runtime.h"
inline void syntheticKernel(uint8_t*, R5900Context* ctx, PS2Runtime*) { ctx->result = 42; }
]=])
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fixture/kernel_bindings.h" [=[
const KernelEntry kernelEntries[] = {{0x80001000u, syntheticKernel},
                                    {0x80001020u, syntheticKernel},
                                    {0x80002000u, syntheticKernel}};
const uint8_t syntheticCode[] = {0x2a, 0, 2, 0x24, 8, 0, 0xe0, 3};
const KernelCode kernelCode[] = {{0x80001000u, syntheticCode, sizeof(syntheticCode)},
                               {0x80002000u, syntheticCode, sizeof(syntheticCode)}};
const KernelRegion kernelRegions[] = {{0x80001000u, 0x80001008u}, {0x80002000u, 0x80002008u}};
]=])
add_executable(kernel_dispatch_smoke main.cpp "${REPO_ROOT}/native/runner/kernel_functions.cpp")
target_include_directories(kernel_dispatch_smoke PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/fixture" "${REPO_ROOT}/native/runner")
enable_testing()
add_test(NAME exact_kernel_dispatch COMMAND kernel_dispatch_smoke)
