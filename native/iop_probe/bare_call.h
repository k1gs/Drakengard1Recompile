#pragma once
#include "core/iop_memory.h"
#include "core/iop_cpu.h"
#include "imports/iop_imports.h"
#include "passive_hardware.h"
#include <iostream>
#include <stdexcept>
#include <string>

struct BareCallResult { uint32_t value{}, instructions{}; };

// Execute only owned RAM code with RAM data accesses. Unknown imports, hardware,
// exceptions and the instruction budget are explicit blockers, never zero returns.
inline BareCallResult bareCall(ps2x::iop::detail::IopMemory& memory, uint32_t entry,
                              uint32_t gp, uint32_t argument, uint32_t codeBegin, uint32_t codeEnd,
                              const PassiveHardware* hardware = nullptr) {
    using namespace ps2x::iop::detail;
    IopCpuCore core(memory);
    IopImportRegistry imports(memory);
    IopCpuState cpu;
    constexpr uint32_t ReturnBoundary = 0xfffffffcu;
    cpu.pc = entry; cpu.gpr[4] = argument; cpu.gpr[28] = gp;
    cpu.gpr[29] = IopMemory::RamSize - 4096;
    cpu.gpr[31] = ReturnBoundary;
    for (uint32_t steps = 0; steps < 2000000; ++steps) {
        if (cpu.pc == ReturnBoundary) return {cpu.gpr[2], steps};
        const uint32_t physical = IopMemory::physicalAddress(cpu.pc);
        if (physical < codeBegin || uint64_t(physical) + 4 > codeEnd || (cpu.pc & 3) ||
            !memory.ownsRamRange(physical, 4))
            throw std::runtime_error("Execution left owned bootstrap code RAM");
        if (const auto call = imports.decode(cpu.pc))
            throw std::runtime_error("Unresolved guest import " + call->library + ":" + std::to_string(call->ordinal));
        const uint32_t instruction = memory.read32(cpu.pc);
        const uint32_t opcode = instruction >> 26;
        if ((opcode >= 0x20 && opcode <= 0x26) || (opcode >= 0x28 && opcode <= 0x2b) || opcode == 0x2e) {
            const uint32_t address = cpu.gpr[(instruction >> 21) & 31] +
                static_cast<uint32_t>(static_cast<int16_t>(instruction));
            const auto target = IopMemory::physicalAddress(address);
            const unsigned width = opcode == 0x20 || opcode == 0x24 || opcode == 0x28 ? 1 :
                opcode == 0x21 || opcode == 0x25 || opcode == 0x29 ? 2 : 4;
            if ((target >= IopMemory::RamSize || width > IopMemory::RamSize - target) &&
                !(hardware && hardware->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]))) {
                std::cerr << "[bootstrap-mmio-blocker] steps=" << steps << " pc=0x" << std::hex << cpu.pc
                          << " instruction=0x" << instruction << " address=0x" << address << std::dec << '\n';
                throw std::runtime_error("Non-RAM data access requires an explicit hardware contract");
            }
        }
        const uint32_t previous = cpu.pc;
        const bool running = core.executeInstruction(cpu);
        if (!running || cpu.exception || cpu.stopped || cpu.yielded) {
            std::cerr << "[bootstrap-cpu-blocker] steps=" << steps << " pc=0x" << std::hex << previous
                      << " instruction=0x" << instruction << " cause=0x" << cpu.cop0[13]
                      << " badvaddr=0x" << cpu.cop0[8] << std::dec << '\n';
            throw std::runtime_error("IOP CPU stopped/raised an exception; no handler invented");
        }
    }
    throw std::runtime_error("Bootstrap call exceeded the fixed instruction budget");
}
