#pragma once
#include "core/iop_memory.h"
#include "core/iop_cpu.h"
#include "imports/iop_imports.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace iop_diagnostics {
using ps2x::iop::detail::IopMemory;
using ps2x::iop::detail::IopCpuState;

[[noreturn]] inline void unresolvedImport(const IopMemory& memory, const IopCpuState& cpu,
    const ps2x::iop::detail::IopImportCall& call, uint32_t codeBegin, uint32_t codeEnd) {
    std::cerr << "[unresolved-iop-import] library=" << call.library << " ordinal=" << call.ordinal
              << " version=0x" << std::hex << call.version << " pc=0x" << cpu.pc
              << " ra=0x" << cpu.gpr[31] << " a0=0x" << cpu.gpr[4] << std::dec << '\n';
    for (uint32_t address = codeBegin + 12; address + 8 < codeEnd; address += 4) {
        bool match = call.library.size() <= 8;
        for (unsigned index = 0; match && index < 8; ++index)
            match = memory.read8(address + index) ==
                (index < call.library.size() ? static_cast<uint8_t>(call.library[index]) : 0);
        if (!match) continue;
        const auto table = address - 12;
        if (uint64_t(table) + 24 + uint64_t(call.ordinal) * 4 > IopMemory::RamSize) continue;
        const auto target = memory.read32(table + 20 + call.ordinal * 4);
        std::cerr << "[guest-library-candidate] table=0x" << std::hex << table
                  << " link_magic=0x" << memory.read32(table) << " version=0x" << memory.read16(table + 8)
                  << " mode=0x" << memory.read16(table + 10) << " ordinal_target=0x" << target
                  << " owned_target=" << memory.ownsRamRange(target, 4) << std::dec << '\n';
    }
    throw std::runtime_error("Unresolved guest import " + call.library + ":" + std::to_string(call.ordinal));
}

[[noreturn]] inline void stoppedCpu(const IopMemory& memory, const IopCpuState& cpu,
    uint32_t steps, uint32_t previous, uint32_t instruction) {
    std::cerr << "[bootstrap-cpu-blocker] steps=" << steps << " pc=0x" << std::hex << previous
              << " instruction=0x" << instruction << " cause=0x" << cpu.cop0[13]
              << " badvaddr=0x" << cpu.cop0[8] << " target=0x" << cpu.pc
              << " status=0x" << cpu.cop0[12] << " epc=0x" << cpu.cop0[14] << std::dec << '\n';
    for (unsigned index = 0; index < cpu.gpr.size(); ++index)
        std::cerr << "[stopped-iop-reg] r" << index << "=0x" << std::hex << cpu.gpr[index] << std::dec << '\n';
    const auto physical = IopMemory::physicalAddress(previous);
    if (physical >= 32 && physical < IopMemory::RamSize - 64)
        for (uint32_t at = physical - 32; at < physical + 64; at += 4)
            std::cerr << "[stopped-iop-code] address=0x" << std::hex << at << " word=0x" << memory.read32(at)
                      << std::dec << '\n';
    const auto stack = IopMemory::physicalAddress(cpu.gpr[29]);
    if (memory.ownsRamRange(stack, 64))
        for (uint32_t n = 0; n < 64; n += 4)
            std::cerr << "[stopped-iop-stack] address=0x" << std::hex << stack + n << " word=0x"
                      << memory.read32(stack + n) << std::dec << '\n';
    if (cpu.exception)
        for (uint32_t at = 0x80; at < 0xa0; at += 4)
            std::cerr << "[exception-vector] address=0x" << std::hex << at << " word=0x"
                      << memory.read32(at) << " owned=" << memory.ownsRamRange(at, 4) << std::dec << '\n';
    throw std::runtime_error("IOP CPU stopped/raised an exception; no handler invented");
}

[[noreturn]] inline void exhaustedBudget(const IopMemory& memory, const IopCpuState& cpu,
    uint64_t isolatedStores) {
    std::cerr << "[bootstrap-budget] pc=0x" << std::hex << cpu.pc << " instruction=0x" << memory.read32(cpu.pc)
              << " v0=0x" << cpu.gpr[2] << " a0=0x" << cpu.gpr[4] << " a1=0x" << cpu.gpr[5]
              << " ra=0x" << cpu.gpr[31] << " sp=0x" << cpu.gpr[29] << " status=0x" << cpu.cop0[12]
              << std::dec << " isolated_cache_stores=" << isolatedStores << '\n';
    for (unsigned index = 0; index < cpu.gpr.size(); ++index)
        std::cerr << "[bootstrap-reg] r" << index << "=0x" << std::hex << cpu.gpr[index] << std::dec << '\n';
    const auto faultPhysical = IopMemory::physicalAddress(cpu.pc);
    if (faultPhysical >= 32 && faultPhysical < IopMemory::RamSize - 32) {
        for (uint32_t address = faultPhysical - 32; address < faultPhysical + 24; address += 4) {
            const auto instruction = memory.read32(address);
            std::cerr << "[bootstrap-code] address=0x" << std::hex << address << " word=0x" << instruction;
            const auto op = instruction >> 26;
            if (op == 0x23) {
                const auto operand = cpu.gpr[(instruction >> 21) & 31] +
                    static_cast<uint32_t>(static_cast<int16_t>(instruction));
                const auto operandPhysical = IopMemory::physicalAddress(operand);
                std::cerr << " load_address=0x" << operand;
                if (operandPhysical < IopMemory::RamSize - 3)
                    std::cerr << " value=0x" << memory.read32(operand);
            }
            std::cerr << std::dec << '\n';
        }
    }
    throw std::runtime_error("Bootstrap call exceeded the fixed instruction budget");
}
}
