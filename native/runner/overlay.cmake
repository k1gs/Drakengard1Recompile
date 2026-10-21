# Inject original host glue and locally generated guest code without changing upstream.
function(drakengard_configure_runner)
    if(NOT TARGET ps2EntryRunner OR NOT TARGET ps2_runtime)
        message(FATAL_ERROR "Enable pinned PS2Recomp runtime targets first")
    endif()
    get_filename_component(REPO_ROOT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
    set(GENERATED_DIR "${REPO_ROOT}/generated/lift-007")
    if(NOT EXISTS "${GENERATED_DIR}/register_functions.cpp")
        message(FATAL_ERROR "Run the verified local lift first")
    endif()
    get_target_property(runner_sources ps2EntryRunner SOURCES)
    list(FILTER runner_sources EXCLUDE REGEX "main[.]cpp$")
    # Replace the default empty registration TU, keeping unity grouping stable.
    list(TRANSFORM runner_sources REPLACE ".*register_functions[.]cpp$"
        "${REPO_ROOT}/native/runner/verify_input.cpp")
    set_property(TARGET ps2EntryRunner PROPERTY SOURCES "${runner_sources}")
    file(GLOB guest_sources CONFIGURE_DEPENDS "${GENERATED_DIR}/*.cpp")
    target_sources(ps2EntryRunner PRIVATE
        "${REPO_ROOT}/native/runner/main.cpp" ${guest_sources})
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${REPO_ROOT}/scripts/prepare_kernel.py"
        "${REPO_ROOT}/scripts/prepare_firmware.py"
        "${REPO_ROOT}/scripts/reference_state.py")
    set(kernel_units "${CMAKE_BINARY_DIR}/kernel_translation")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/prepare_kernel.py"
        "${kernel_units}" RESULT_VARIABLE kernel_result)
    if(NOT kernel_result EQUAL 0)
        message(FATAL_ERROR "Prepare verified kernel image and lift first")
    endif()
    file(GLOB kernel_sources CONFIGURE_DEPENDS "${kernel_units}/*.cpp")
    target_sources(ps2EntryRunner PRIVATE ${kernel_sources}
        "${REPO_ROOT}/native/runner/kernel_functions.cpp"
        "${REPO_ROOT}/native/runner/sif_channel.cpp"
        "${REPO_ROOT}/native/runner/fileio_rpc.cpp"
        "${REPO_ROOT}/native/runner/iso_media.cpp"
        "${REPO_ROOT}/native/runner/cdvd_rpc.cpp"
        "${REPO_ROOT}/native/runner/sif_transport.cpp")
    set(IOP_ROOT "${CMAKE_SOURCE_DIR}/ps2xIOP")
    include("${REPO_ROOT}/native/iop_probe/primitives.cmake")
    drakengard_add_iop_primitives(drakengard_strict_iop)
    # Separate namespace prevents strict CPU/cache semantics from replacing the
    # upstream pre-reset HLE subsystem through duplicate linker symbols.
    target_compile_definitions(drakengard_strict_iop PRIVATE ps2x=drakengard_strict_iop)
    add_library(drakengard_iop_engine STATIC "${REPO_ROOT}/native/runner/iop_boot.cpp")
    target_compile_definitions(drakengard_iop_engine PRIVATE ps2x=drakengard_strict_iop)
    target_include_directories(drakengard_iop_engine PRIVATE "${REPO_ROOT}/native/iop_probe")
    target_link_libraries(drakengard_iop_engine PRIVATE drakengard_strict_iop)
    target_sources(ps2EntryRunner PRIVATE "${REPO_ROOT}/native/runner/iop_bridge.cpp")
    target_link_libraries(ps2EntryRunner PRIVATE drakengard_iop_engine)
    set(firmware_host "${REPO_ROOT}/native/runner/firmware_functions.cpp")
    target_sources(ps2EntryRunner PRIVATE "${firmware_host}")
    if(EXISTS "${REPO_ROOT}/generated/firmware-005/register_functions.cpp")
        set(firmware_units "${CMAKE_BINARY_DIR}/firmware_translation")
        execute_process(COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/prepare_firmware.py"
            --input-complete "${firmware_units}" RESULT_VARIABLE firmware_result)
        if(NOT firmware_result EQUAL 0)
            message(FATAL_ERROR "Verified firmware preparation failed")
        endif()
        file(GLOB firmware_sources CONFIGURE_DEPENDS "${firmware_units}/*.cpp")
        target_sources(ps2EntryRunner PRIVATE ${firmware_sources})
        set_source_files_properties(${firmware_sources} "${firmware_host}"
            TARGET_DIRECTORY ps2EntryRunner PROPERTIES INCLUDE_DIRECTORIES "${firmware_units}")
        set_source_files_properties("${firmware_host}" TARGET_DIRECTORY ps2EntryRunner PROPERTIES
            COMPILE_DEFINITIONS DRAKENGARD_HAVE_FIRMWARE SKIP_UNITY_BUILD_INCLUSION ON)
    endif()
    set_source_files_properties("${REPO_ROOT}/native/runner/sif_transport.cpp"
        TARGET_DIRECTORY ps2EntryRunner PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
    target_include_directories(ps2EntryRunner PRIVATE "${kernel_units}")
    target_include_directories(ps2EntryRunner PRIVATE "${GENERATED_DIR}")
    set_target_properties(ps2EntryRunner PROPERTIES
        UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16 INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
    if(MSVC)
        get_target_property(runner_link_options ps2EntryRunner LINK_OPTIONS)
        list(FILTER runner_link_options EXCLUDE REGEX "^/FORCE(:.*)?$")
        set_property(TARGET ps2EntryRunner PROPERTY LINK_OPTIONS "${runner_link_options}")
        target_compile_options(ps2EntryRunner PRIVATE /Od /bigobj /MP4 /arch:AVX2 /fp:precise)
        target_link_options(ps2EntryRunner PRIVATE /STACK:8388608)
        target_compile_options(ps2_runtime PRIVATE /arch:AVX2 /fp:precise)
    endif()
    if(WIN32)
        target_link_libraries(ps2EntryRunner PRIVATE bcrypt)
    endif()

    # Reviewed upstream adaptations, grouped by the runtime contract they change.
    include("${REPO_ROOT}/native/runner/cmake/syscalls.cmake")
    include("${REPO_ROOT}/native/runner/cmake/dispatch.cmake")
    include("${REPO_ROOT}/native/runner/cmake/sif.cmake")
    include("${REPO_ROOT}/native/runner/cmake/memory.cmake")
    include("${REPO_ROOT}/native/runner/cmake/scheduler.cmake")
    include("${REPO_ROOT}/native/runner/cmake/graphics.cmake")
endfunction()
cmake_language(DEFER CALL drakengard_configure_runner)
