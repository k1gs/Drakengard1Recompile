# Included by overlay.cmake. The EE executor owns both CPUs: only an actual
# no-ready-thread wait advances this extra bounded IOP slice. No host/UI worker
# touches guest RAM, and no IRQ is dispatched without a completed DMA event.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/EeScheduler.cpp" scheduler_original)
set(scheduler_wait "                waitForEvent();\n                continue;")
string(REPLACE "${scheduler_wait}"
    "                if (drakengard::advanceFirmwareIopWhileEeIdle()) {\n                    for (uint32_t cause : m_runtime.memory().consumeCompletedDmacCauses()) dispatchIrq(true, cause);\n                    if (!m_pendingInvocations.empty()) continue;\n                }\n${scheduler_wait}"
    scheduler_cooperative "${scheduler_original}")
if(scheduler_cooperative STREQUAL scheduler_original)
    message(FATAL_ERROR "Pinned EE no-ready-thread wait contract changed")
endif()
set(scheduler_file "${CMAKE_BINARY_DIR}/private_cooperative_Scheduler.cpp")
file(CONFIGURE OUTPUT "${scheduler_file}"
    CONTENT "#include \"iop_bridge.h\"\n${scheduler_cooperative}" @ONLY)
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/EeScheduler[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${scheduler_file}")
