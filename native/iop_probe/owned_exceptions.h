#pragma once
#include "core/iop_memory.h"
#include "core/iop_cpu.h"
#include <iostream>

// Let an actual installed guest exception vector execute. No host syscall return
// value or invented handler: the guest CPU core already saved EPC/Cause/Status.
class OwnedExceptions {
public:
    bool vectorOwns(const ps2x::iop::detail::IopMemory& memory, uint32_t address) const {
        return address >= 0x80 && address < 0x100 && memory.ownsRamRange(address, 4) &&
            memory.ownsRamRange(0x80, 4) && memory.read32(0x80) != 0;
    }
    bool enterSyscall(const ps2x::iop::detail::IopMemory& memory,
                      const ps2x::iop::detail::IopCpuState& cpu, uint32_t instruction) {
        if ((instruction & 0xfc00003fu) != 0xcu || ((cpu.cop0[13] >> 2) & 31u) != 8 ||
            cpu.pc != 0x80000080u || !vectorOwns(memory, 0x80)) return false;
        ++syscalls;
        if (syscalls <= 16 || syscalls % 1024 == 0)
            std::cerr << "[owned-iop-syscall] count=" << syscalls << " epc=0x" << std::hex << cpu.cop0[14]
                      << " v0=0x" << cpu.gpr[2] << " a0=0x" << cpu.gpr[4] << std::dec << '\n';
        return true;
    }
    uint64_t syscalls{};
};
