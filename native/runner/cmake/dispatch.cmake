# Included by overlay.cmake inside drakengard_configure_runner.
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
    "diagnoseSifTransfer(rdram, transfer);\n    if (transfer.phase == ps2x::iop::SifTransferPhase::BeforeCopy) drakengard::processSifTransfer(this, rdram, transfer);\n    m_iopSubsystem->onSifTransfer(transfer);\n    if (transfer.phase != ps2x::iop::SifTransferPhase::BeforeCopy) drakengard::processSifTransfer(this, rdram, transfer);"
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
include("${REPO_ROOT}/native/runner/cmake/thread_failure.cmake")
string(REPLACE "    const uint32_t entryPc = ctx->pc;\n    targetFn(rdram, ctx, this);"
    "    const uint32_t entryPc = ctx->pc;\n    const uint32_t entryRa = getRegU32(ctx, 31);\n    const uint32_t entrySp = getRegU32(ctx, 29);\n    targetFn(rdram, ctx, this);\n    drakengard::diagnoseEeTransfer(\"dispatch\", sourcePc, entryPc, entryRa, entrySp, rdram, *ctx);"
    runtime_diagnostic "${runtime_fatal_devices}")
if(runtime_diagnostic STREQUAL runtime_fatal_devices)
    message(FATAL_ERROR "Pinned guest dispatch target invocation changed")
endif()
set(runtime_file "${CMAKE_BINARY_DIR}/private_extended_Runtime.cpp")
file(CONFIGURE OUTPUT "${runtime_file}"
    CONTENT "#include <exception>\n#include \"kernel_functions.h\"\n#include \"firmware_functions.h\"\n#include \"strict_device_error.h\"\n#include \"sif_diagnostic.h\"\n#include \"sif_transport.h\"\n#include \"ee_transfer_diagnostic.h\"\n${runtime_diagnostic}" @ONLY)
set_source_files_properties("${runtime_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/ps2_runtime[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${runtime_file}")
