#include "bare_call.h"
#include <cstring>
#include <initializer_list>

namespace {
using ps2x::iop::detail::IopMemory;
void code(IopMemory& memory, std::initializer_list<uint32_t> words) {
    uint32_t address = 0x1000;
    for (uint32_t word : words) { memory.write32(address, word); address += 4; }
}
template<class Action> void rejects(Action action, const std::string& reason) {
    try { action(); }
    catch (const std::runtime_error& error) {
        if (std::string(error.what()).find(reason) != std::string::npos) return;
        throw;
    }
    throw std::runtime_error("Strict IOP guard failed to reject " + reason);
}
}

int main() {
    try {
        IopMemory memory;
        code(memory, {0x2402002a, 0x03e00008, 0});
        auto result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030);
        if (result.value != 42 || result.instructions != 3)
            throw std::runtime_error("Bare IOP return/delay-slot contract failed");
        memory.reset(); memory.write32(0x800, 42);
        code(memory, {0x8c820000, 0, 0x03e00008, 0x24420001}); // lw; nop; jr ra; addiu v0,1.
        BareCallState sliced;
        const auto step = [&] {
            return bareCall(memory, 0x1000, 0, 0x800, 0x1000, 0x1100,
                            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &sliced, 1);
        };
        result = step();
        if (result.completed || !sliced.cpu.pendingLoad || sliced.cpu.gpr[2] != 0)
            throw std::runtime_error("IOP slice lost its pending load or claimed completion");
        result = step();
        if (result.completed || sliced.cpu.pendingLoad || sliced.cpu.gpr[2] != 42)
            throw std::runtime_error("IOP slice failed to commit a delayed load");
        result = step();
        if (result.completed || !sliced.cpu.branchPending)
            throw std::runtime_error("IOP slice lost the branch delay slot");
        result = step();
        if (!result.completed || result.value != 43 || result.instructions != 4)
            throw std::runtime_error("IOP sliced return differs from actual delay-slot semantics");
        rejects([&] { bareCall(memory, 0x1000, 0, 0x801, 0x1000, 0x1100,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &sliced, 1); }, "call contract");
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 1); }, "instruction slice");
        code(memory, {0x40807800, 0x40027800, 0, 0x03e00008, 0}); // mtc0 zero,PRId; mfc0 v0,PRId.
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030);
        if (result.value != 0x1f) throw std::runtime_error("IOP revision identity must be correct and read-only");
        memory.reset(); code(memory, {0x8c820000, 0x03e00008, 0}); // lw v0,0(a0)
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf801450, 0x1000, 0x1030); }, "hardware contract");
        std::array<uint8_t, 28> seed{'D', '1', 'H', 'W', '0', '0', '1', 0};
        const auto put = [&](size_t at, uint32_t value) {
            for (unsigned n = 0; n < 4; ++n) seed[at + n] = uint8_t(value >> (8 * n));
        };
        put(8, 2); put(12, 0x1f801450); put(16, 0x12345678); put(20, 0x1f801578); put(24, 1);
        PassiveHardware hardware; hardware.seed(seed, memory);
        result = bareCall(memory, 0x1000, 0, 0xbf801450, 0x1000, 0x1030, &hardware);
        if (result.value != 0x12345678) throw std::runtime_error("Passive hardware read lost actual state");
        code(memory, {0xac800000, 0x8c820000, 0, 0x03e00008, 0}); // sw zero,0(a0); lw v0,0(a0)
        result = bareCall(memory, 0x1000, 0, 0xbf801578, 0x1000, 0x1030, &hardware);
        if (result.value != 0 || memory.read32(0xbf801578) != 0)
            throw std::runtime_error("DMA-global gate storage did not change");
        code(memory, {0x24020002, 0xac820000, 0x03e00008, 0}); // Unreviewed gate bits.
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf801578, 0x1000, 0x1030, &hardware); }, "hardware contract");
        put(20, 0x1f801450);
        rejects([&] { hardware.seed(seed, memory); }, "duplicate passive hardware register");
        if (memory.read32(0xbf801578) != 0) throw std::runtime_error("Rejected seed changed hardware");
        memory.reset(); memory.write32(0x800, 0xdeadbeef);
        code(memory, {0x3c08fffe, 0x35080130, 0x24090c04, 0xad090000, 0x3c0a0001,
                      0x408a6000, 0x240b0800, 0xad600000, 0x40806000, 0xad000000,
                      0x2402002a, 0x03e00008, 0});
        CoherentCache cache; cache.initialize(memory);
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100, nullptr, &cache);
        if (result.value != 42 || cache.isolatedStores != 1 || memory.read32(0x800) != 0xdeadbeef ||
            memory.read32(0xfffe0130) != 0)
            throw std::runtime_error("Cache isolation corrupted backing RAM or BIU restore");
        code(memory, {0x3c0a0001, 0x408a6000, 0xad600000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "explicit coherent-cache mode");
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030, nullptr, &cache); }, "Unsupported isolated cache access");
        memory.reset(); QuietInterrupts interrupts; interrupts.reset(memory);
        code(memory, {0x24020001, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801078, 0x1000, 0x1030, nullptr, nullptr, &interrupts);
        if (result.value != 1 || memory.interruptControl() != 0)
            throw std::runtime_error("I_CTRL read must return prior value and disable interrupts");
        memory.setInterruptStatus(0x42); memory.setInterruptMask(0x40); memory.setInterruptControl(1);
        interrupts.checkDelivery(memory, false); // CP0 global masking must preserve a latched pending source.
        rejects([&] { interrupts.checkDelivery(memory); }, "requires actual exception delivery");
        memory.setInterruptControl(0);
        code(memory, {0xac800000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801070, 0x1000, 0x1030, nullptr, nullptr, &interrupts);
        if (result.value != 0 || memory.interruptStatus() != 0)
            throw std::runtime_error("I_STAT acknowledge did not clear actual pending bits");
        memory.reset(); QuiescentDma dma; dma.reset(memory);
        memory.write32(0x1f8010f4, 0x03000000); // Two actual flags in the synthetic device state.
        dma.afterWrite(memory, 0x1f8010f4, 0x03000000, 0x01810000); // Ack one; enable remaining.
        if (memory.read32(0x1f8010f4) != 0x02810000 || memory.interruptStatus() != 0)
            throw std::runtime_error("DMA W1C acknowledge changed wrong flags");
        dma.afterWrite(memory, 0x1f8010f4, 0x02810000, 0x00820000);
        if (memory.read32(0x1f8010f4) != 0x82820000 || memory.interruptStatus() != 8)
            throw std::runtime_error("DMA enabled flag did not assert actual IRQ state");
        rejects([&] { dma.allows(0x1f801528, 0x2b, 0x01000000); }, "requires an actual transfer engine");
        if (!dma.allows(0x1f801528, 0x2b, 0) || dma.allows(0x1f80153c, 0x2b, 0))
            throw std::runtime_error("DMA idle control/reserved register boundary failed");
        memory.reset(); code(memory, {0x0000000c, 0x2402002a, 0x03e00008, 0});
        OwnedExceptions exceptions;
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100, nullptr, nullptr, nullptr, nullptr, &exceptions); },
                "raised an exception");
        if (!memory.zeroRam(0x80, 0x80)) throw std::runtime_error("Vector fixture range failed");
        const uint32_t handler[] = {0x401a7000, 0, 0x275a0004, 0x03400008, 0x42000010};
        for (unsigned index = 0; index < 5; ++index) memory.write32(0x80 + index * 4, handler[index]);
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100, nullptr, nullptr, nullptr, nullptr, &exceptions);
        if (result.value != 42 || exceptions.syscalls != 1)
            throw std::runtime_error("Actual guest syscall vector did not return through EPC/RFE");
        memory.reset(); interrupts.reset(memory); memory.setInterruptStatus(0x10);
        code(memory, {0x3c08bf80, 0x35081074, 0x24090010, 0xad090000, 0x24090001,
                      0xad090004, 0x340a0401, 0x408a6000, 0x2402002a, 0x03e00008, 0});
        if (!memory.zeroRam(0x80, 0x80)) throw std::runtime_error("IRQ vector fixture range failed");
        const uint32_t irqHandler[] = {0x401a7000, 0, 0x3c1bbf80, 0x377b1070,
                                       0xaf600000, 0x03400008, 0x42000010};
        for (unsigned index = 0; index < 7; ++index) memory.write32(0x80 + index * 4, irqHandler[index]);
        OwnedExceptions irqExceptions;
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100, nullptr, nullptr, &interrupts, nullptr, &irqExceptions);
        if (result.value != 42 || irqExceptions.interrupts != 1 || memory.interruptStatus() != 0)
            throw std::runtime_error("Actual guest IRQ vector/acknowledge/EPC return failed");
        ps2x::iop::detail::IopCpuState branchCpu;
        branchCpu.pc = 0x1004; branchCpu.branchPending = true; branchCpu.cop0[12] = 0x401; branchCpu.cop0[13] = 0x400;
        ps2x::iop::detail::IopCpuCore core(memory);
        if (!irqExceptions.enterInterrupt(memory, branchCpu, core) || branchCpu.cop0[14] != 0x1000 ||
            !(branchCpu.cop0[13] & 0x80000000u)) throw std::runtime_error("IRQ branch-delay EPC/BD contract failed");
        memory.reset(); code(memory, {0xfc000000}); // Unsupported instruction must raise an exception.
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "raised an exception");
        memory.reset();
        code(memory, {0x41e00000, 0, 0x100, 0, 0, 0x03e00008, 0x24000007, 0, 0});
        const char library[8] = "missing";
        if (!memory.writeRam(0x100c, library, sizeof(library))) throw std::runtime_error("Fixture write failed");
        rejects([&] { bareCall(memory, 0x1014, 0, 0, 0x1000, 0x1030); }, "Unresolved guest import missing:7");
        memory.reset();
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "owned bootstrap code");
        std::vector<uint8_t> romBytes(0x400000);
        SifRegisters sif;
        sif.write(0x1d000000, 0xffffffff); // Peer-owned address remains quiescent.
        sif.write(0x1d000010, 0x12345678);
        sif.write(0x1d000030, 0x10000); sif.write(0x1d000030, 0x40000);
        sif.write(0x1d000020, 0xffffffff); // Clear cannot invent peer bits.
        sif.write(0x1d000040, 0x20);
        if (sif.read(0x1d000000) || sif.read(0x1d000010) != 0x12345678 ||
            sif.read(0x1d000030) != 0x50000 || sif.read(0x1d000020) ||
            sif.read(0x1d000040) != 0xf0002022 || sif.read(0x1d000060))
            throw std::runtime_error("SIF direction, flags, control or unused register contract failed");
        sif.write(0x1d000040, 0x20);
        if (sif.read(0x1d000040) != 0xf0002002 || sif.allows(0x1d000050, 0x23) || sif.allows(0x1d000000, 0x25))
            throw std::runtime_error("SIF toggle, reserved register or width guard failed");
        memory.reset(); code(memory, {0xac800000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbd000030, 0x1000, 0x1100,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &sif);
        if (result.value != 0x50000) throw std::runtime_error("SIF set flags were overwritten by CPU SW backing storage");
        memory.reset(); SifRegisters peerBus; BareCallState waiting;
        code(memory, {0x3c08bd00, 0x8d020020, 0, 0x1040fffd, 0, 0x03e00008, 0});
        const auto poll = [&] {
            return bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &peerBus, &waiting, 16);
        };
        result = poll();
        if (result.completed || result.instructions != 16 || peerBus.read(0x1d000020))
            throw std::runtime_error("Waiting IOP fabricated a peer flag/completion");
        peerBus.writeEe(0x1000f220, 0x10000); // Explicit synthetic peer event between CPU slices.
        result = poll();
        if (!result.completed || result.value != 0x10000)
            throw std::runtime_error("Persistent IOP did not observe the actual peer write");
        romBytes[0] = 0x78; romBytes[1] = 0x56; romBytes[2] = 0x34; romBytes[3] = 0x92;
        romBytes.back() = 0xa5;
        ReadOnlyRom rom(std::move(romBytes));
        for (uint32_t base : {0x1fc00000u, 0x9fc00000u, 0xbfc00000u}) {
            memory.reset(); code(memory, {0x8c820000, 0, 0x03e00008, 0});
            result = bareCall(memory, 0x1000, 0, base, 0x1000, 0x1100,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &rom);
            if (result.value != 0x92345678 || memory.read32(base) != 0)
                throw std::runtime_error("Read-only ROM alias/load delay or staging restoration failed");
        }
        code(memory, {0x90820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbfffffffu, 0x1000, 0x1100,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &rom);
        if (result.value != 0xa5 || rom.allows(0xc0000000, 0x24) || rom.allows(0xfffe0130, 0x23) ||
            rom.allows(0x7fc00000, 0x23) || rom.allows(0xbfc00000, 0x2b))
            throw std::runtime_error("ROM bounds, write protection or KSEG3 exclusion failed");
        code(memory, {0xac800000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbfc00000, 0x1000, 0x1100,
                               nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &rom); }, "hardware contract");
        rejects([&] { bareCall(memory, 0xbfc00000, 0, 0, 0x1000, 0x1100,
                               nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &rom); }, "owned bootstrap code");
        code(memory, {0x3c040001, 0x3c05001f, 0xac800000, 0x24840004, 0x0085102b,
                      0x1440fffc, 0, 0x2402002a, 0x03e00008, 0});
        memory.write32(0x1efffc, 0xdeadbeef); memory.write32(0x1f0000, 0xcafebabe);
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100);
        if (result.instructions <= 2000000 || result.value != 42 || memory.read32(0x1efffc) != 0 ||
            memory.read32(0x1f0000) != 0xcafebabe)
            throw std::runtime_error("Finite RAM clear was cut short or crossed its end boundary");
        code(memory, {0x1000ffff, 0}); // Deliberate bounded loop.
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "instruction budget");
        std::cout << "Bare IOP return, exception, MMIO, unresolved import, ownership and budget checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
