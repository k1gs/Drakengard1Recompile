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
    if(EXISTS "${REPO_ROOT}/generated/firmware-003/register_functions.cpp")
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
        "return lookupFirmwareFunction(address) != nullptr || lookupKernelFunction(address) != nullptr || (generatedFunctionTableSlot(address, slot) && g_ps2RecompiledFunctionTable[slot] != nullptr);"
        runtime_guarded "${runtime_original}")
    if(runtime_guarded STREQUAL runtime_original)
        message(FATAL_ERROR "Pinned hasFunction contract changed")
    endif()
    string(REPLACE "pushDispatchPc(address);"
        "pushDispatchPc(address);\n    if (auto firmware = lookupFirmwareFunction(address)) return firmware;\n    if (auto kernel = lookupKernelFunction(address)) return kernel;"
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
    string(REPLACE "catch (const std::exception &)\n    {"
        "catch (const drakengard::StrictDeviceError&) { throw; }\n    catch (const std::exception &)\n    {"
        runtime_fatal_devices "${runtime_reset}")
    if(runtime_fatal_devices STREQUAL runtime_reset)
        message(FATAL_ERROR "Pinned runtime memory exception contract changed")
    endif()
    set(runtime_file "${CMAKE_BINARY_DIR}/private_extended_Runtime.cpp")
    file(CONFIGURE OUTPUT "${runtime_file}"
        CONTENT "#include \"kernel_functions.h\"\n#include \"firmware_functions.h\"\n#include \"strict_device_error.h\"\n#include \"sif_diagnostic.h\"\n#include \"sif_transport.h\"\n${runtime_fatal_devices}" @ONLY)
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
    foreach(reg_entry IN ITEMS
        "void sceSifGetReg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)"
        "void sceSifSetReg(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)")
        if(reg_entry MATCHES "sceSifGetReg")
            set(reg_route "drakengard::cooperativeSifGetReg(getRegU32(ctx, 4))")
        else()
            set(reg_route "drakengard::cooperativeSifSetReg(getRegU32(ctx, 4), getRegU32(ctx, 5))")
        endif()
        string(REPLACE "${reg_entry}\n    {"
            "${reg_entry}\n    {\n        if (auto value = ${reg_route}) { setReturnU32(ctx, *value); return; }"
            sif_reg_guarded "${sif_directional}")
        if(sif_reg_guarded STREQUAL sif_directional)
            message(FATAL_ERROR "Pinned HLE SIF register entry changed")
        endif()
        set(sif_directional "${sif_reg_guarded}")
    endforeach()
    set(sif_file "${CMAKE_BINARY_DIR}/private_directional_SIF.cpp")
    file(CONFIGURE OUTPUT "${sif_file}"
        CONTENT "#include \"sif_transport.h\"\n#include \"iop_bridge.h\"\n${sif_directional}" @ONLY)
    set_source_files_properties("${sif_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Stubs")
    get_target_property(runtime_sources ps2_runtime SOURCES)
    list(FILTER runtime_sources EXCLUDE REGEX "Stubs/SIF[.]cpp$")
    set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
    target_sources(ps2_runtime PRIVATE "${sif_file}")

    # Observe actual EE bus writes before wiring a separate IOP bootstrap peer.
    # This diagnostic copy changes no register value or completion behavior.
    file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/ps2_memory.cpp" memory_original)
    set(memory_entry "bool PS2Memory::writeIORegister(uint32_t address, uint32_t value)\n{")
    string(REPLACE "${memory_entry}"
        "${memory_entry}\n    diagnoseEeSifRegisterWrite(address, value);\n    if (drakengard::firmwareDmaWrite(address, value)) return true;\n    if (drakengard::cooperativeSifWrite(address, value)) return true;"
        memory_traced "${memory_original}")
    if(memory_traced STREQUAL memory_original)
        message(FATAL_ERROR "Pinned EE memory register-write entry changed")
    endif()
    set(memory_read "uint32_t PS2Memory::readIORegister(uint32_t address)\n{")
    string(REPLACE "${memory_read}"
        "${memory_read}\n    if (auto value = drakengard::firmwareDmaRead(address)) return *value;\n    if (auto value = drakengard::cooperativeSifRead(address)) return *value;"
        memory_bridged "${memory_traced}")
    if(memory_bridged STREQUAL memory_traced)
        message(FATAL_ERROR "Pinned EE memory register-read entry changed")
    endif()
    set(memory_file "${CMAKE_BINARY_DIR}/private_traced_Memory.cpp")
    file(CONFIGURE OUTPUT "${memory_file}"
        CONTENT "#include \"sif_diagnostic.h\"\n#include \"iop_bridge.h\"\n${memory_bridged}" @ONLY)
    set_source_files_properties("${memory_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
    get_target_property(runtime_sources ps2_runtime SOURCES)
    list(FILTER runtime_sources EXCLUDE REGEX "/ps2_memory[.]cpp$")
    set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
    target_sources(ps2_runtime PRIVATE "${memory_file}")
endfunction()
cmake_language(DEFER CALL drakengard_configure_runner)
