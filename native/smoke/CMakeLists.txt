cmake_minimum_required(VERSION 3.21)
project(DrakengardSyntheticSmoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_executable(recomp_smoke
    main.cpp
    "${REPO_ROOT}/generated/smoke-001/return42_0x1000.cpp")
target_include_directories(recomp_smoke PRIVATE
    "${REPO_ROOT}/generated/smoke-001"
    "${REPO_ROOT}/tools/ps2recomp/ps2xRuntime/include"
    "${REPO_ROOT}/tools/ps2recomp/ps2xRuntime/src/lib/Kernel"
    "${REPO_ROOT}/tools/ps2recomp/ps2xIOP/include")
if(MSVC)
    target_compile_options(recomp_smoke PRIVATE /arch:AVX2)
endif()
