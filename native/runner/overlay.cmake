# Inject original host glue and locally generated guest code without changing upstream.
function(drakengard_configure_runner)
    if(NOT TARGET ps2EntryRunner OR NOT TARGET ps2_runtime)
        message(FATAL_ERROR "Enable pinned PS2Recomp runtime targets first")
    endif()
    get_filename_component(REPO_ROOT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(GENERATED_DIR "${REPO_ROOT}/generated/lift-002")
    if(NOT EXISTS "${GENERATED_DIR}/register_functions.cpp")
        message(FATAL_ERROR "Run the verified local lift first")
    endif()
    get_target_property(runner_sources ps2EntryRunner SOURCES)
    list(FILTER runner_sources EXCLUDE REGEX "main[.]cpp$")
    set_property(TARGET ps2EntryRunner PROPERTY SOURCES "${runner_sources}")
    file(GLOB guest_sources CONFIGURE_DEPENDS "${GENERATED_DIR}/*.cpp")
    target_sources(ps2EntryRunner PRIVATE
        "${REPO_ROOT}/native/runner/main.cpp" ${guest_sources})
    target_include_directories(ps2EntryRunner PRIVATE "${GENERATED_DIR}")
    set_target_properties(ps2EntryRunner PROPERTIES
        UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16 INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    if(MSVC)
        target_compile_options(ps2EntryRunner PRIVATE /Od /bigobj /MP4 /arch:AVX2 /fp:precise)
        target_link_options(ps2EntryRunner PRIVATE /STACK:8388608)
        target_compile_options(ps2_runtime PRIVATE /arch:AVX2 /fp:precise)
    endif()

    # Upstream's unknown syscall fallback returns success. Replace only that path
    # in a private build-tree translation unit with an explicit fatal blocker.
    set(system_file "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Syscalls/System.cpp")
    file(READ "${system_file}" original_system)
    string(REPLACE "\r\n" "\n" original_system "${original_system}")
    set(old "// Bootstrap default: avoid hard-failing loops that probe syscall availability.\n        setReturnS32(ctx, 0);")
    string(REPLACE "${old}"
        "throw std::runtime_error(\"Unsupported EE syscall; see PC and arguments above\");"
        strict_system "${original_system}")
    if(strict_system STREQUAL original_system)
        message(FATAL_ERROR "Pinned unknown-syscall source contract changed; review first")
    endif()
    set(strict_file "${CMAKE_BINARY_DIR}/private_strict_System.cpp")
    file(WRITE "${strict_file}" "#include <stdexcept>\n${strict_system}")
    get_target_property(runtime_sources ps2_runtime SOURCES)
    list(FILTER runtime_sources EXCLUDE REGEX "Syscalls/System[.]cpp$")
    set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
    target_sources(ps2_runtime PRIVATE "${strict_file}")
    target_include_directories(ps2_runtime PRIVATE
        "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Syscalls")
endfunction()
cmake_language(DEFER CALL drakengard_configure_runner)
