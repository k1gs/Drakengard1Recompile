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
    "    const uint32_t entryPc = ctx->pc;\n    const uint32_t entryRa = getRegU32(ctx, 31);\n    const uint32_t entrySp = getRegU32(ctx, 29);\n    drakengard::diagnoseEeListEntry(entryPc, rdram, *ctx);\n    targetFn(rdram, ctx, this);\n    drakengard::diagnoseEeTransfer(\"dispatch\", sourcePc, entryPc, entryRa, entrySp, rdram, *ctx);"
    runtime_diagnostic "${runtime_fatal_devices}")
if(runtime_diagnostic STREQUAL runtime_fatal_devices)
    message(FATAL_ERROR "Pinned guest dispatch target invocation changed")
endif()
string(REPLACE "    const uint64_t currentTick = rt->eeScheduler().currentVSyncTick();"
    "    const uint64_t currentTick = rt->eeScheduler().currentVSyncTick();\n    drakengard::observeGsUiTick(currentTick);"
    runtime_tick_metric "${runtime_diagnostic}")
if(runtime_tick_metric STREQUAL runtime_diagnostic)
    message(FATAL_ERROR "Pinned UploadFrame guest-tick metric point changed")
endif()
set(presentation_metric_anchor "    PS2_IF_AGRESSIVE_LOGS({\n        static uint32_t s_uploadDebugCount = 0u;")
string(REPLACE "${presentation_metric_anchor}"
    "    drakengard::observeGsRgb(s_scratch, width, height);\n${presentation_metric_anchor}"
    runtime_presentation_metric "${runtime_tick_metric}")
if(runtime_presentation_metric STREQUAL runtime_tick_metric)
    message(FATAL_ERROR "Pinned valid GS readback metric point changed")
endif()
set(runtime_file "${CMAKE_BINARY_DIR}/private_extended_Runtime.cpp")
include("${CMAKE_CURRENT_LIST_DIR}/pinned_patch.cmake")
drakengard_replace_pinned(runtime_presentation_metric
    "void PS2Runtime::Store32(uint8_t *rdram, R5900Context *ctx, uint32_t vaddr, uint32_t value)\n{"
    "void PS2Runtime::Store32(uint8_t *rdram, R5900Context *ctx, uint32_t vaddr, uint32_t value)\n{\n    drakengard::GsDmaCpuScope gifCpuScope((vaddr&0x1fffffffu)==0x1000a000u && (value&0x100u),ctx->pc,getRegU32(ctx,31),getRegU32(ctx,29),rdram,PS2_RAM_SIZE);"
    "GIF CPU store scope")
drakengard_replace_pinned(runtime_presentation_metric
    "                                         uint32_t chcr)\n{"
    "                                         uint32_t chcr)\n{\n    drakengard::GsDmaCpuScope gifCpuScope(true,ctx->pc,getRegU32(ctx,31),getRegU32(ctx,29),rdram,PS2_RAM_SIZE);"
    "GIF CPU chain scope")
file(CONFIGURE OUTPUT "${runtime_file}"
    CONTENT "#include \"gs_production_metrics.h\"\n#include <exception>\n#include \"game_thread_guard.h\"\n#include \"kernel_functions.h\"\n#include \"firmware_functions.h\"\n#include \"strict_device_error.h\"\n#include \"sif_diagnostic.h\"\n#include \"sif_transport.h\"\n#include \"ee_transfer_diagnostic.h\"\n${runtime_presentation_metric}" @ONLY)
set_source_files_properties("${runtime_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/ps2_runtime[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${runtime_file}")
