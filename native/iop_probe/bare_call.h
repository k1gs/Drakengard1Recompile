#pragma once
#include "core/iop_memory.h"
#include "core/iop_cpu.h"
#include "imports/iop_imports.h"
#include "passive_hardware.h"
#include "coherent_cache.h"
#include "quiet_interrupts.h"
#include "quiescent_dma.h"
#include "owned_exceptions.h"
#include "cdvd_device.h"
#include "iop_timers.h"
#include "read_only_rom.h"
#include "sif_registers.h"
#include <iostream>
#include <stdexcept>
#include <string>

struct BareCallResult { uint32_t value{}, instructions{}; };

// Execute only owned RAM code with explicitly mapped data. Unknown imports, hardware,
// exceptions and the instruction budget are explicit blockers, never zero returns.
inline BareCallResult bareCall(ps2x::iop::detail::IopMemory& memory, uint32_t entry,
                              uint32_t gp, uint32_t argument, uint32_t codeBegin, uint32_t codeEnd,
                              const PassiveHardware* hardware = nullptr, CoherentCache* cache = nullptr,
                              QuietInterrupts* interrupts = nullptr, QuiescentDma* dma = nullptr,
                              OwnedExceptions* exceptions = nullptr, drakengard::CdvdDevice* cdvd = nullptr,
                              drakengard::IopTimers* timers = nullptr, const ReadOnlyRom* rom = nullptr,
                              SifRegisters* sif = nullptr) {
    using namespace ps2x::iop::detail;
    IopCpuCore core(memory);
    IopImportRegistry imports(memory);
    IopCpuState cpu;
    constexpr uint32_t ReturnBoundary = 0xfffffffcu;
    cpu.pc = entry; cpu.gpr[4] = argument; cpu.gpr[28] = gp;
    cpu.cop0[15] = 0x1f; // Immutable IOP R3000A revision, pinned PCSX2 reset contract.
    cpu.gpr[29] = IopMemory::RamSize - 4096;
    cpu.gpr[31] = ReturnBoundary;
    // A legitimate 2-MiB word fill alone uses ~2.62M instructions (five per word).
    // Keep a finite bound that also permits the following module bootstrap work.
    for (uint32_t steps = 0; steps < 8000000; ++steps) {
        if (cpu.pc == ReturnBoundary) return {cpu.gpr[2], steps};
        if (interrupts) {
            const bool pending = interrupts->pending(memory);
            if (pending) cpu.cop0[13] |= 0x400u;
            else cpu.cop0[13] &= ~0x400u;
            // Poll at retired instruction boundaries, after any pending load has committed.
            // Branch-delay IRQ entry uses the core's actual BD/EPC handling.
            if (pending && (cpu.cop0[12] & 0x401u) == 0x401u && !cpu.pendingLoad) {
                if (!exceptions || !exceptions->enterInterrupt(memory, cpu, core))
                    interrupts->checkDelivery(memory);
            }
        }
        const uint32_t physical = IopMemory::physicalAddress(cpu.pc);
        const bool vector = exceptions && exceptions->vectorOwns(memory, physical);
        if (((physical < codeBegin || uint64_t(physical) + 4 > codeEnd) && !vector) || (cpu.pc & 3) ||
            !memory.ownsRamRange(physical, 4))
            throw std::runtime_error("Execution left owned bootstrap code RAM");
        if (const auto call = imports.decode(cpu.pc)) {
            std::cerr << "[unresolved-iop-import] library=" << call->library << " ordinal=" << call->ordinal
                      << " version=0x" << std::hex << call->version << " pc=0x" << cpu.pc
                      << " ra=0x" << cpu.gpr[31] << " a0=0x" << cpu.gpr[4] << std::dec << '\n';
            for (uint32_t address = codeBegin + 12; address + 8 < codeEnd; address += 4) {
                bool match = call->library.size() <= 8;
                for (unsigned index = 0; match && index < 8; ++index)
                    match = memory.read8(address + index) ==
                        (index < call->library.size() ? static_cast<uint8_t>(call->library[index]) : 0);
                if (!match) continue;
                const auto table = address - 12;
                if (uint64_t(table) + 24 + uint64_t(call->ordinal) * 4 > IopMemory::RamSize) continue;
                const auto target = memory.read32(table + 20 + call->ordinal * 4);
                std::cerr << "[guest-library-candidate] table=0x" << std::hex << table
                          << " link_magic=0x" << memory.read32(table) << " version=0x" << memory.read16(table + 8)
                          << " mode=0x" << memory.read16(table + 10) << " ordinal_target=0x" << target
                          << " owned_target=" << memory.ownsRamRange(target, 4) << std::dec << '\n';
            }
            throw std::runtime_error("Unresolved guest import " + call->library + ":" + std::to_string(call->ordinal));
        }
        const uint32_t instruction = memory.read32(cpu.pc);
        const uint32_t opcode = instruction >> 26;
        uint32_t interruptRead = 0;
        uint32_t dmaWrite = 0, dmaPrevious = 0, dmaValue = 0;
        uint32_t sifWrite = 0;
        std::optional<std::pair<uint32_t, uint32_t>> romRestore;
        if ((opcode >= 0x20 && opcode <= 0x26) || (opcode >= 0x28 && opcode <= 0x2b) || opcode == 0x2e) {
            const uint32_t address = cpu.gpr[(instruction >> 21) & 31] +
                static_cast<uint32_t>(static_cast<int16_t>(instruction));
            const auto target = IopMemory::physicalAddress(address);
            const unsigned width = opcode == 0x20 || opcode == 0x24 || opcode == 0x28 ? 1 :
                opcode == 0x21 || opcode == 0x25 || opcode == 0x29 ? 2 : 4;
            if (cpu.cop0[12] & 0x10000u) {
                if (!cache) throw std::runtime_error("Cache isolation requires explicit coherent-cache mode");
                cache->isolatedAccess(memory, address, opcode, cpu.gpr[(instruction >> 16) & 31]);
            }
            const bool cdvdAccess = cdvd && target >= 0x1f402000 && target < 0x1f402100 &&
                (opcode == 0x20 || opcode == 0x24 || opcode == 0x28);
            const bool timerWrite = opcode == 0x29 || opcode == 0x2b;
            const bool timerAccess = timers && (timerWrite || opcode == 0x21 || opcode == 0x25 || opcode == 0x23) &&
                timers->supports(target, width, timerWrite);
            const bool romAccess = rom && rom->allows(address, opcode);
            const bool sifAccess = sif && sif->allows(target, opcode);
            if ((target >= IopMemory::RamSize || width > IopMemory::RamSize - target) &&
                !(hardware && hardware->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31])) &&
                !(cache && cache->allows(address, opcode, cpu.gpr[(instruction >> 16) & 31])) &&
                !(interrupts && interrupts->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31])) &&
                !(dma && dma->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31])) &&
                !cdvdAccess && !timerAccess && !romAccess && !sifAccess) {
                std::cerr << "[bootstrap-mmio-blocker] steps=" << steps << " pc=0x" << std::hex << cpu.pc
                          << " instruction=0x" << instruction << " address=0x" << address << std::dec
                          << " rt_value=0x" << std::hex << cpu.gpr[(instruction >> 16) & 31] << std::dec
                          << " isolated_cache_stores=" << (cache ? cache->isolatedStores : 0) << '\n';
                throw std::runtime_error("Non-RAM data access requires an explicit hardware contract");
            }
            if (romAccess) romRestore = rom->stage(memory, address, opcode);
            if (sifAccess) {
                if (opcode == 0x2b) {
                    sif->write(target, cpu.gpr[(instruction >> 16) & 31]);
                    sifWrite = target;
                }
                memory.write32(address, sif->read(target));
            }
            if (cdvdAccess) {
                const auto key = static_cast<uint8_t>(target);
                if (opcode == 0x28) cdvd->writeRegister(key, static_cast<uint8_t>(cpu.gpr[(instruction >> 16) & 31]));
                else memory.write8(address, cdvd->readRegister(key));
            }
            if (timerAccess) {
                if (timerWrite) timers->write(target, width, cpu.gpr[(instruction >> 16) & 31]);
                else if (width == 2) memory.write16(address, static_cast<uint16_t>(timers->read(target, width)));
                else memory.write32(address, timers->read(target, width));
            }
            if (interrupts && opcode == 0x23 && interrupts->allows(target, opcode, 0)) interruptRead = target;
            if (dma && opcode == 0x2b && dma->interruptRegister(target)) {
                dmaWrite = target; dmaPrevious = memory.read32(target); dmaValue = cpu.gpr[(instruction >> 16) & 31];
            }
        }
        const uint32_t previous = cpu.pc;
        const bool running = core.executeInstruction(cpu);
        // Temporary staging preserves the upstream core's load-delay behavior
        // without leaving ROM words in the masked hardware map (BIU aliases).
        if (romRestore) memory.write32(romRestore->first, romRestore->second);
        if (sifWrite) memory.write32(sifWrite, sif->read(sifWrite));
        if (!cpu.exception && interruptRead) interrupts->afterRead(memory, interruptRead);
        if (!cpu.exception && dmaWrite) dma->afterWrite(memory, dmaWrite, dmaPrevious, dmaValue);
        if (cdvd && cdvd->takeInterruptEdge()) memory.setInterruptStatus(memory.interruptStatus() | 4u);
        if (timers) memory.setInterruptStatus(memory.interruptStatus() | timers->advance(1));
        if (interrupts) {
            if (interrupts->pending(memory)) cpu.cop0[13] |= 0x400u;
            else cpu.cop0[13] &= ~0x400u;
            const bool enabled = (cpu.cop0[12] & 0x401u) == 0x401u;
            if (!exceptions) interrupts->checkDelivery(memory, enabled);
        }
        const bool ownedSyscall = cpu.exception && exceptions && exceptions->enterSyscall(memory, cpu, instruction);
        if (!running || (cpu.exception && !ownedSyscall) || cpu.stopped || cpu.yielded) {
            std::cerr << "[bootstrap-cpu-blocker] steps=" << steps << " pc=0x" << std::hex << previous
                      << " instruction=0x" << instruction << " cause=0x" << cpu.cop0[13]
                      << " badvaddr=0x" << cpu.cop0[8] << " target=0x" << cpu.pc
                      << " status=0x" << cpu.cop0[12] << " epc=0x" << cpu.cop0[14] << std::dec << '\n';
            if (cpu.exception)
                for (uint32_t at = 0x80; at < 0xa0; at += 4)
                    std::cerr << "[exception-vector] address=0x" << std::hex << at << " word=0x"
                              << memory.read32(at) << " owned=" << memory.ownsRamRange(at, 4) << std::dec << '\n';
            throw std::runtime_error("IOP CPU stopped/raised an exception; no handler invented");
        }
    }
    std::cerr << "[bootstrap-budget] pc=0x" << std::hex << cpu.pc << " instruction=0x" << memory.read32(cpu.pc)
              << " v0=0x" << cpu.gpr[2] << " a0=0x" << cpu.gpr[4] << " a1=0x" << cpu.gpr[5]
              << " ra=0x" << cpu.gpr[31] << " sp=0x" << cpu.gpr[29] << " status=0x" << cpu.cop0[12]
              << std::dec << " isolated_cache_stores=" << (cache ? cache->isolatedStores : 0) << '\n';
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
