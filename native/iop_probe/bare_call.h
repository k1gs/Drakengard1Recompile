#pragma once
#include "core/iop_memory.h"
#include "core/iop_cpu.h"
#include "imports/iop_imports.h"
#include "iop_data_bus.h"
#include "iop_diagnostics.h"

struct BareCallResult { uint32_t value{}, instructions{}; bool completed{true}; };
struct BareCallState {
    ps2x::iop::detail::IopCpuState cpu;
    const ps2x::iop::detail::IopMemory* memory{};
    uint32_t instructions{}, codeBegin{}, codeEnd{}, entry{}, gp{}, argument{};
};

// Execute only owned RAM code with explicitly mapped data. Unknown imports, hardware,
// exceptions and the instruction budget are explicit blockers, never zero returns.
inline BareCallResult bareCall(ps2x::iop::detail::IopMemory& memory, uint32_t entry,
                              uint32_t gp, uint32_t argument, uint32_t codeBegin, uint32_t codeEnd,
                              const IopDevices& devices = {}, BareCallState* resumed = nullptr,
                              uint32_t sliceInstructions = 8000000) {
    using namespace ps2x::iop::detail;
    IopCpuCore core(memory);
    IopImportRegistry imports(memory);
    IopDataBus bus(memory, devices);
    auto* interrupts = devices.interrupts;
    auto* exceptions = devices.exceptions;
    if (!sliceInstructions || sliceInstructions > 8000000 || (!resumed && sliceInstructions != 8000000))
        throw std::runtime_error("Invalid bounded IOP instruction slice");
    BareCallState local;
    auto& state = resumed ? *resumed : local;
    auto& cpu = state.cpu;
    constexpr uint32_t ReturnBoundary = 0xfffffffcu;
    if (!state.memory) {
        state.memory = &memory; state.codeBegin = codeBegin; state.codeEnd = codeEnd;
        state.entry = entry; state.gp = gp; state.argument = argument;
        cpu.pc = entry; cpu.gpr[4] = argument; cpu.gpr[28] = gp;
        cpu.cop0[15] = 0x1f; // Immutable IOP R3000A revision, pinned PCSX2 reset contract.
        cpu.gpr[29] = IopMemory::RamSize - 4096;
        cpu.gpr[31] = ReturnBoundary;
    } else if (state.memory != &memory || state.codeBegin != codeBegin || state.codeEnd != codeEnd ||
               state.entry != entry || state.gp != gp || state.argument != argument) {
        throw std::runtime_error("IOP resume context changed its memory or call contract");
    }
    const uint32_t sliceBegin = state.instructions;
    // A legitimate 2-MiB word fill alone uses ~2.62M instructions (five per word).
    // Keep a finite bound that also permits the following module bootstrap work.
    for (auto& steps = state.instructions; steps < 8000000 && steps - sliceBegin < sliceInstructions; ++steps) {
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
            iop_diagnostics::unresolvedImport(memory, cpu, *call, codeBegin, codeEnd);
        }
        const uint32_t instruction = memory.read32(cpu.pc);
        const auto effects = bus.prepare(cpu, instruction, steps);
        const uint32_t previous = cpu.pc;
        const bool running = core.executeInstruction(cpu);
        bus.complete(cpu, effects);
        if (interrupts) {
            if (interrupts->pending(memory)) cpu.cop0[13] |= 0x400u;
            else cpu.cop0[13] &= ~0x400u;
            const bool enabled = (cpu.cop0[12] & 0x401u) == 0x401u;
            if (!exceptions) interrupts->checkDelivery(memory, enabled);
        }
        const bool ownedSyscall = cpu.exception && exceptions && exceptions->enterSyscall(memory, cpu, instruction);
        if (!running || (cpu.exception && !ownedSyscall) || cpu.stopped || cpu.yielded) {
            iop_diagnostics::stoppedCpu(memory, cpu, steps, previous, instruction);
        }
    }
    if (cpu.pc == ReturnBoundary) return {cpu.gpr[2], state.instructions};
    if (state.instructions < 8000000) return {0, state.instructions, false};
    iop_diagnostics::exhaustedBudget(memory, cpu, devices.cache ? devices.cache->isolatedStores : 0);
}
