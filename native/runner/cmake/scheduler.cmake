# Keep the pinned checkout unchanged. Only the EE executor advances strict
# devices and delivers real completion events, including during guest spin loops.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/EeScheduler.cpp" scheduler_source)
macro(replace_scheduler anchor replacement)
    string(REPLACE "${anchor}" "${replacement}" scheduler_next "${scheduler_source}")
    if(scheduler_next STREQUAL scheduler_source)
        message(FATAL_ERROR "Pinned EE scheduler contract changed: ${anchor}")
    endif()
    set(scheduler_source "${scheduler_next}")
endmacro()
set(scheduler_wait "                waitForEvent();\n                continue;")
replace_scheduler("${scheduler_wait}"
    "                if (drakengard::advanceFirmwareIopWhileEeIdle()) {\n                    for (uint32_t cause : m_runtime.memory().consumeCompletedDmacCauses()) dispatchIrq(true, cause);\n                    if (!m_pendingInvocations.empty()) continue;\n                }\n${scheduler_wait}")
set(scheduler_run_entry "    m_running.store(true, std::memory_order_release);")
replace_scheduler("${scheduler_run_entry}" "${scheduler_run_entry}\n    EeDeviceSafePoint devices;")
replace_scheduler("        processPendingEvents();"
    "        devices.advance(*this,m_runtime,m_eeCycle,(m_enabledIntcMask & 1u)!=0,thread(kMainThreadId) ? &thread(kMainThreadId)->context : nullptr);\n        processPendingEvents();")
replace_scheduler("            function(m_rdram, &context, &m_runtime);"
    "            const uint32_t entryPc = context.pc;\n            const uint32_t entryRa = getRegU32(&context, 31);\n            const uint32_t entrySp = getRegU32(&context, 29);\n            function(m_rdram, &context, &m_runtime);\n            drakengard::diagnoseEeTransfer(\"scheduler\", entryPc, entryPc, entryRa, entrySp, m_rdram, context);")
replace_scheduler("    handlers.emplace(id,"
    "    logEeIrqRegistration(dmac,cause,handler,argument,gp);\n    handlers.emplace(id,")
replace_scheduler("    case EeEventType::VBlankStart:\n        ++m_vsyncTick;"
    "    case EeEventType::VBlankStart:\n        drakengard::firmwareIopVideoEvent(true);\n        ++m_vsyncTick;")
replace_scheduler("    case EeEventType::VBlankEnd:\n        dispatchIrq(false, 3u);"
    "    case EeEventType::VBlankEnd:\n        drakengard::firmwareIopVideoEvent(false);\n        dispatchIrq(false, 3u);")
replace_scheduler("    std::sort(matching.begin(), matching.end(),"
    "    logEeIrqDelivery(dmac,cause,matching.size());\n    std::sort(matching.begin(), matching.end(),")
replace_scheduler("void EeScheduler::dispatchIrq(bool dmac, uint32_t cause)\n{\n    assertExecutor();"
    "void EeScheduler::dispatchIrq(bool dmac, uint32_t cause)\n{\n    assertExecutor();\n    if (!dmac) drakengard::latchEeIntcCause(cause);")
replace_scheduler("    std::memcpy(m_rdram + physical, &value, sizeof(value));"
    "    auto* writer = currentThread();\n    drakengard::traceEeWatchedWrite(m_rdram, physical, sizeof(value), value, 0u, \"SCHEDULER32\", writer ? &writer->activeContext() : nullptr);\n    std::memcpy(m_rdram + physical, &value, sizeof(value));")
set(scheduler_file "${CMAKE_BINARY_DIR}/private_cooperative_Scheduler.cpp")
file(CONFIGURE OUTPUT "${scheduler_file}"
    CONTENT "#include \"ee_device_safe_point.h\"\n#include \"ee_transfer_diagnostic.h\"\n${scheduler_source}" @ONLY)
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/EeScheduler[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${scheduler_file}")
