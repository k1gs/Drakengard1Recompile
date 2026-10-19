# Included by overlay.cmake inside drakengard_configure_runner.
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
set(unknown_warning "        std::cerr << \"Warning: Unimplemented PS2 syscall called. PC=0x\"")
string(REPLACE "${unknown_warning}"
    "        if (syscallId == 0x6bu) { if (invokeFirmwareSifSyscall(rdram, ctx, runtime, 0x6bu)) return; drakengard::stopSifReceive(runtime); return; }\n${unknown_warning}"
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
    CONTENT "#include <stdexcept>\n#include \"firmware_functions.h\"\n#include \"sif_transport.h\"\n#include \"diagnostic.h\"\n${strict_system}" @ONLY)
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "Syscalls/System[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${strict_file}")
target_include_directories(ps2_runtime PRIVATE
    "${REPO_ROOT}/native/runner"
    "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/Kernel/Syscalls")
