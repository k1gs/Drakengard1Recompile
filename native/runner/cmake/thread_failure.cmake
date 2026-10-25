# Included by dispatch.cmake in the runner configuration scope. Preserve a fatal
# device failure across the game thread; rethrow only after join and GUI cleanup.
set(thread_state "std::atomic<bool> gameThreadFinished{false};")
string(REPLACE "${thread_state}" "${thread_state}\n    std::exception_ptr strictDeviceFailure;"
    thread_guarded "${runtime_fatal_devices}")
if(thread_guarded STREQUAL runtime_fatal_devices)
    message(FATAL_ERROR "Pinned game-thread completion state contract changed")
endif()
set(thread_catch "catch (const std::exception &e)\n        {\n            std::cerr << \"Error during program execution: ")
string(REPLACE "${thread_catch}"
    "catch (const drakengard::StrictDeviceError&) { strictDeviceFailure = std::current_exception(); }\n        ${thread_catch}"
    thread_captured "${thread_guarded}")
if(thread_captured STREQUAL thread_guarded)
    message(FATAL_ERROR "Pinned game-thread execution catch contract changed")
endif()
# A host/UI exception must stop and join before std::thread's destructor runs.
# Keep the normal explicit join and the existing fatal-device propagation.
set(thread_finish "gameThreadFinished.store(true, std::memory_order_release); });")
string(REPLACE "${thread_finish}"
    "${thread_finish}\n    drakengard::GameThreadJoinGuard gameThreadGuard(gameThread, [this]() noexcept { requestStop(); });"
    thread_owned "${thread_captured}")
if(thread_owned STREQUAL thread_captured)
    message(FATAL_ERROR "Pinned game-thread ownership boundary changed")
endif()
set(thread_exit "RUNTIME_LOG(\"[run] exiting loop\");")
string(REPLACE "${thread_exit}"
    "${thread_exit}\n    if (strictDeviceFailure) std::rethrow_exception(strictDeviceFailure);"
    thread_propagated "${thread_owned}")
if(thread_propagated STREQUAL thread_owned)
    message(FATAL_ERROR "Pinned game-thread cleanup boundary changed")
endif()
set(runtime_fatal_devices "${thread_propagated}")
