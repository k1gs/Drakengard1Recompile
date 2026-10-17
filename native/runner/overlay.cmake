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
    set(install_call "runtime->setEeSyscallOverride(rdram, syscallIndex, handler);")
    string(REPLACE "${install_call}"
        "if (handler != 0u && !runtime->hasFunction(handler)) { std::cerr << \"Missing syscall handler: syscall=0x\" << std::hex << syscallIndex << \" handler=0x\" << handler << \" pc=0x\" << ctx->pc << std::dec << std::endl; throw std::runtime_error(\"No generated code for installed syscall handler\"); }\n        ${install_call}"
        guarded_system "${strict_system}")
    if(guarded_system STREQUAL strict_system)
        message(FATAL_ERROR "Pinned SetSyscall contract changed; review first")
    endif()
    set(strict_system "${guarded_system}")
    string(REPLACE "if (syscallId == 0x04u)"
        "if (syscallId == 0x6bu) { drakengard::stopSifReceive(runtime); return; }\n        if (syscallId == 0x04u)"
        stopped_system "${strict_system}")
    if(stopped_system STREQUAL strict_system)
        message(FATAL_ERROR "Pinned syscall fallback dispatch contract changed")
    endif()
    set(strict_system "${stopped_system}")
    set(strict_file "${CMAKE_BINARY_DIR}/private_strict_System.cpp")
    string(REPLACE "throw std::runtime_error(\"No generated code for installed syscall handler\");"
        "diagnoseMissingHandler(rdram, PS2_RAM_SIZE, handler); throw std::runtime_error(\"No generated code for installed syscall handler\");"
        strict_system "${strict_system}")
    file(CONFIGURE OUTPUT "${strict_file}"
        CONTENT "#include <stdexcept>\n#include \"sif_transport.h\"\n#include \"diagnostic.h\"\n${strict_system}" @ONLY)
    get_target_property(runtime_sources ps2_runtime SOURCES)
    list(FILTER runtime_sources EXCLUDE REGEX "Syscalls/System[.]cpp$")
    set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
    target_sources(ps2_runtime PRIVATE "${strict_file}")
    target_include_directories(ps2_runtime PRIVATE
        "${REPO_ROOT}/native/runner"
        "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Syscalls")

    # Add a separate exact-address kernel table instead of expanding the dense
    # application table across the unused 2GB guest virtual-address interval.
    file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/ps2_runtime.cpp" runtime_original)
    string(REPLACE "return generatedFunctionTableSlot(address, slot) && g_ps2RecompiledFunctionTable[slot] != nullptr;"
        "return lookupKernelFunction(address) != nullptr || (generatedFunctionTableSlot(address, slot) && g_ps2RecompiledFunctionTable[slot] != nullptr);"
        runtime_guarded "${runtime_original}")
    if(runtime_guarded STREQUAL runtime_original)
        message(FATAL_ERROR "Pinned hasFunction contract changed")
    endif()
    string(REPLACE "pushDispatchPc(address);"
        "pushDispatchPc(address);\n    if (auto kernel = lookupKernelFunction(address)) return kernel;"
        runtime_extended "${runtime_guarded}")
    if(runtime_extended STREQUAL runtime_guarded)
        message(FATAL_ERROR "Pinned lookupFunction contract changed")
    endif()
    string(REPLACE "m_iopSubsystem->onSifTransfer(transfer);"
        "diagnoseSifTransfer(rdram, transfer);\n    m_iopSubsystem->onSifTransfer(transfer);\n    drakengard::processSifTransfer(this, rdram, transfer);"
        runtime_traced "${runtime_extended}")
    if(runtime_traced STREQUAL runtime_extended)
        message(FATAL_ERROR "Pinned SIF transfer contract changed")
    endif()
    string(REPLACE "m_iopSubsystem->reset();"
        "drakengard::resetSifChannel(this);\n    m_iopSubsystem->reset();"
        runtime_reset "${runtime_traced}")
    if(runtime_reset STREQUAL runtime_traced)
        message(FATAL_ERROR "Pinned IOP reset contract changed")
    endif()
    set(runtime_file "${CMAKE_BINARY_DIR}/private_extended_Runtime.cpp")
    file(CONFIGURE OUTPUT "${runtime_file}"
        CONTENT "#include \"kernel_functions.h\"\n#include \"sif_diagnostic.h\"\n#include \"sif_transport.h\"\n${runtime_reset}" @ONLY)
    set_source_files_properties("${runtime_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
    get_target_property(runtime_sources ps2_runtime SOURCES)
    list(FILTER runtime_sources EXCLUDE REGEX "/ps2_runtime[.]cpp$")
    set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
    target_sources(ps2_runtime PRIVATE "${runtime_file}")

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
    set(sif_file "${CMAKE_BINARY_DIR}/private_directional_SIF.cpp")
    file(CONFIGURE OUTPUT "${sif_file}"
        CONTENT "#include \"sif_transport.h\"\n${sif_directional}" @ONLY)
    set_source_files_properties("${sif_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Stubs")
    get_target_property(runtime_sources ps2_runtime SOURCES)
    list(FILTER runtime_sources EXCLUDE REGEX "Stubs/SIF[.]cpp$")
    set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
    target_sources(ps2_runtime PRIVATE "${sif_file}")
endfunction()
cmake_language(DEFER CALL drakengard_configure_runner)
