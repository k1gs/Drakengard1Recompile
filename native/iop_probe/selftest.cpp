#include "bare_call.h"
#include "sif1_receiver.h"
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
        {
            constexpr uint32_t base = 0x110000;
            memory.write32(base, 0x2402002a); memory.write32(base + 4, 0x03e00008); memory.write32(base + 8, 0);
            std::array<uint8_t, 12> text{}; memory.readRam(base, text.data(), text.size());
            VerifiedRamCode module(base, base, text);
            rejects([&] { bareCall(memory, base, 0, 0, 0x1000, 0x1100); }, "owned bootstrap code RAM");
            result = bareCall(memory, base, 0, 0, 0x1000, 0x1100, {.ramCode = &module});
            if (result.value != 42 || result.instructions != 3 || memory.interruptStatus())
                throw std::runtime_error("Verified RAM module did not execute its actual CPU return/delay slot");
            memory.write32(base + 4, 0);
            rejects([&] { bareCall(memory, base, 0, 0, 0x1000, 0x1100, {.ramCode = &module}); }, "verified relocated reference");
            if (memory.read32(base + 4) != 0)
                throw std::runtime_error("RAM code verification repaired guest code");
        }
        memory.reset(); memory.write32(0x800, 42);
        code(memory, {0x8c820000, 0, 0x03e00008, 0x24420001}); // lw; nop; jr ra; addiu v0,1.
        BareCallState sliced;
        const auto step = [&] {
            return bareCall(memory, 0x1000, 0, 0x800, 0x1000, 0x1100,
                {}, &sliced, 1);
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
            {}, &sliced, 1); }, "call contract");
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
            {}, nullptr, 1); }, "instruction slice");
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
        result = bareCall(memory, 0x1000, 0, 0xbf801450, 0x1000, 0x1030,
            {.hardware = &hardware});
        if (result.value != 0x12345678) throw std::runtime_error("Passive hardware read lost actual state");
        code(memory, {0xac800000, 0x8c820000, 0, 0x03e00008, 0}); // sw zero,0(a0); lw v0,0(a0)
        result = bareCall(memory, 0x1000, 0, 0xbf801578, 0x1000, 0x1030,
            {.hardware = &hardware});
        if (result.value != 0 || memory.read32(0xbf801578) != 0)
            throw std::runtime_error("DMA-global gate storage did not change");
        code(memory, {0x24020002, 0xac820000, 0x03e00008, 0}); // Unreviewed gate bits.
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf801578, 0x1000, 0x1030,
            {.hardware = &hardware}); }, "hardware contract");
        put(20, 0x1f801450);
        rejects([&] { hardware.seed(seed, memory); }, "duplicate passive hardware register");
        if (memory.read32(0xbf801578) != 0) throw std::runtime_error("Rejected seed changed hardware");
        code(memory, {0x3c020016, 0x3442244f, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        for (uint32_t address : {0x1f801010u, 0x9f801010u, 0xbf801010u}) {
            result = bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100, {.hardware = &hardware});
            if (result.value != 0x0016244f || memory.interruptStatus() || memory.read32(0x1f801014))
                throw std::runtime_error("ROM delay store/read lost value, changed neighbor or invented IRQ");
        }
        code(memory, {0x24020001, 0xac820000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf801010, 0x1000, 0x1100,
            {.hardware = &hardware}); }, "hardware contract");
        if (memory.read32(0x1f801010) != 0x0016244f || hardware.allows(0x1f801010, 0x29, 0) ||
            hardware.allows(0x1f801014, 0x2b, 0x0016244f))
            throw std::runtime_error("Unreviewed SSBUS register/bits/width accepted");
        code(memory, {0x24021225, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801020, 0x1000, 0x1100, {.hardware = &hardware});
        if (result.value != 0x1225 || memory.read32(0x1f801010) != 0x0016244f || memory.interruptStatus() ||
            hardware.allows(0x1f801020, 0x2b, 0x1224) || hardware.allows(0x1f801020, 0x25, 0))
            throw std::runtime_error("SSBUS common-delay storage/guard changed another device or invented IRQ");
        code(memory, {0x240200ff, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf80101c, 0x1000, 0x1100, {.hardware = &hardware});
        if (result.value != 0xff || memory.interruptStatus() ||
            hardware.allows(0x1f80101c, 0x2b, 0x100) || hardware.allows(0x1f80101c, 0x28, 0))
            throw std::runtime_error("SSBUS PIO-delay storage/width/profile guard failed");
        code(memory, {0x3c021f80, 0x34422000, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801004, 0x1000, 0x1100, {.hardware = &hardware});
        if (result.value != 0x1f802000 || memory.interruptStatus() ||
            hardware.allows(0x1f801004, 0x2b, 0x1f803000) || hardware.allows(0x1f802070, 0x2b, 0xfc))
            throw std::runtime_error("PIO base profile opened arbitrary remapping or unreviewed device ports");
        code(memory, {0x3c02000d, 0x34422077, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf80101c, 0x1000, 0x1100, {.hardware = &hardware});
        if (result.value != 0x000d2077 || memory.interruptStatus())
            throw std::runtime_error("PIO second delay profile lost actual guest state");
        PioWriteCapture pio;
        code(memory, {0x240200fc, 0xac820000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf802070, 0x1000, 0x1100,
            {.pio = &pio}); }, "hardware contract");
        pio.enable(); memory.write32(0x1f802074, 0xdeadbeef);
        for (uint32_t address : {0x1f802070u, 0x9f802070u, 0xbf802070u})
            bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100, {.pio = &pio});
        memory.write32(0x1f802070, 0xdeadbefc); // Byte stores must preserve the other three bytes.
        code(memory, {0x3c021234, 0x344256a5, 0xa0820000, 0x03e00008, 0});
        bareCall(memory, 0x1000, 0, 0xbf802070, 0x1000, 0x1100, {.pio = &pio});
        if (pio.writes() != 4 || pio.value() != 0xa5 || pio.width() != 1 || memory.interruptStatus() ||
            memory.read32(0x1f802074) != 0xdeadbeef || memory.read32(0x1f802070) != 0xdeadbea5)
            throw std::runtime_error("Actual PIO stores lost width/data, changed neighbor or invented IRQ");
        code(memory, {0x8c820000, 0, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf802070, 0x1000, 0x1100,
            {.pio = &pio}); }, "hardware contract");
        QuiescentSio2 sio2;
        code(memory, {0x240203bc, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf808268, 0x1000, 0x1100,
            {.sio2 = &sio2}); }, "hardware contract");
        sio2.enable(); memory.write32(0x1f80826c, 0xdeadbeef);
        for (auto address : {0x1f808268u, 0x9f808268u, 0xbf808268u}) {
            result = bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100, {.sio2 = &sio2});
            if (result.value != 0x3bc || memory.interruptStatus() || memory.read32(0x1f80826c) != 0xdeadbeef)
                throw std::runtime_error("SIO2 control lost actual CPU state, changed neighbor or invented IRQ");
        }
        code(memory, {0x240203bd, 0xac820000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbf808268, 0x1000, 0x1100,
            {.sio2 = &sio2}); }, "hardware contract");
        if (memory.read32(0x1f808268) != 0x3bc || memory.interruptStatus())
            throw std::runtime_error("Rejected SIO2 transfer changed control or raised IRQ");
        // Exercise real CPU stores/load delays for every reviewed configuration,
        // including transitions, aliases and denied remapping. No device is run.
        PassiveHardware unseeded;
        for (const auto& setting : ssbus::settings) {
            for (uint32_t value : setting.writes) {
                if (!value) continue;
                for (uint32_t alias : {0u, 0x80000000u, 0xa0000000u}) {
                    const auto address = setting.address | alias;
                    code(memory, {0x3c020000 | (value >> 16), 0x34420000 | (value & 0xffff),
                        0xac820000, 0x8c820000, 0, 0x03e00008, 0});
                    result = bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100, {.hardware = &hardware});
                    if (result.value != value || memory.read32(setting.address) != value || memory.interruptStatus())
                        throw std::runtime_error("SSBUS CPU configuration storage or alias failed");
                }
            }
            const auto previous = memory.read32(setting.address);
            code(memory, {0x24020000, 0xac820000, 0x03e00008, 0});
            rejects([&] { bareCall(memory, 0x1000, 0, setting.address, 0x1000, 0x1100,
                {.hardware = &hardware}); }, "hardware contract");
            if (memory.read32(setting.address) != previous ||
                unseeded.allows(setting.address, 0x23, 0) ||
                hardware.allows(setting.address, 0x29, setting.writes[0]) ||
                hardware.allows(setting.address, 0x25, 0))
                throw std::runtime_error("SSBUS rejected write changed state or accepted width/unseeded profile");
        }
        for (uint32_t port : {0x1f801da8u, 0x1f402000u, 0x1e000000u, 0x1f400010u, 0x14000000u,
                              0x1f801424u, 0xfffe0140u, 0xfffe0144u})
            if (hardware.allows(port, 0x23, 0) || hardware.allows(port, 0x2b, 0xff))
                throw std::runtime_error("SSBUS configuration opened an unrelated device/CPU port");
        memory.reset(); memory.write32(0x800, 0xdeadbeef);
        code(memory, {0x3c08fffe, 0x35080130, 0x24090c04, 0xad090000, 0x3c0a0001,
                      0x408a6000, 0x240b0800, 0xad600000, 0x40806000, 0xad000000,
                      0x2402002a, 0x03e00008, 0});
        CoherentCache cache; cache.initialize(memory);
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
            {.cache = &cache});
        if (result.value != 42 || cache.isolatedStores != 1 || memory.read32(0x800) != 0xdeadbeef ||
            memory.read32(0xfffe0130) != 0)
            throw std::runtime_error("Cache isolation corrupted backing RAM or BIU restore");
        CoherentCache uninitializedCache;
        rejects([&] { uninitializedCache.enableRebootControls(); }, "initialized coherent memory");
        if (cache.allows(0xfffe0130, 0x2b, 0x1edd8))
            throw std::runtime_error("Reboot BIU configuration enabled before explicit entry profile");
        code(memory, {0x3c021f80, 0x34420400, 0xac820000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xfffe0140, 0x1000, 0x1100,
            {.cache = &cache}); }, "hardware contract");
        cache.enableRebootControls();
        rejects([&] { cache.enableRebootControls(); }, "single activation");
        for (auto address : {0xfffe0140u, 0xfffe0144u}) {
            memory.write32(address, 0xfeedface); memory.write32(address + 8, 0xabcdef01);
            const uint32_t value = address == 0xfffe0140 ? 0x1f800400 : 0x1f800000;
            code(memory, {0x3c021f80, 0x34420000 | (value & 0xffff), 0xac820000, 0x03e00008, 0});
            bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100, {.cache = &cache});
            if (memory.read32(address) != 0xfeedface || memory.read32(address + 8) != 0xabcdef01 ||
                memory.read32(0x800) != 0xdeadbeef || memory.interruptStatus())
                throw std::runtime_error("Reference CPU control polluted a masked slot, neighbor, RAM or IRQ");
            for (uint32_t opcode : {0x20u, 0x21u, 0x23u, 0x25u, 0x28u, 0x29u, 0x2eu})
                if (cache.allows(address, opcode, value))
                    throw std::runtime_error("Reference CPU control accepted an unreviewed width/read");
            for (auto alias : {address & 0x1fffffffu, (address & 0x1fffffffu) | 0x80000000u,
                               (address & 0x1fffffffu) | 0xa0000000u, address + 1})
                if (cache.allows(alias, 0x2b, value))
                    throw std::runtime_error("Reference CPU control opened an alias or unaligned access");
            code(memory, {0x24020001, 0xac820000, 0x03e00008, 0});
            rejects([&] { bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100,
                {.cache = &cache}); }, "hardware contract");
            if (memory.read32(address) != 0xfeedface)
                throw std::runtime_error("Rejected CPU control write changed state");
        }
        if (cache.controlWrites != 2 || cache.allows(0xfffe0148, 0x2b, 0x1f800000))
            throw std::runtime_error("Reference CPU control count/adjacent register guard failed");
        const auto failedWrite = cache.stageControl(memory, 0xfffe0140, 0x2b, 0x1f800400, 0x1000);
        memory.write32(0xfffe0140, 0x1f800400);
        cache.completeControl(memory, failedWrite, true);
        if (cache.controlWrites != 2 || memory.read32(0xfffe0140) != 0xfeedface)
            throw std::runtime_error("Failed CPU control instruction retained side effects or capture");
        code(memory, {0x3c020001, 0x3442edd8, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xfffe0130, 0x1000, 0x1100, {.cache = &cache});
        if (result.value != 0x1edd8 || memory.interruptStatus() || cache.allows(0xfffe0130, 0x2b, 0x1edd9))
            throw std::runtime_error("Reboot BIU shadow lost configuration or accepted unknown bits");
        rejects([&] { cache.isolatedAccess(memory, 0, 0x2b, 0); }, "Unsupported isolated cache access");
        memory.write32(0xfffe0130, 0);
        {
            std::vector<uint8_t> romBytes(0x400000);
            const auto instruction = [&](uint32_t offset, uint32_t word) {
                for (unsigned n = 0; n < 4; ++n) romBytes[0x4a000 + offset + n] = uint8_t(word >> (n * 8));
            };
            // Fetch and data load target the same word: nested staging must
            // restore the instruction, then the original backing slot.
            instruction(0, 0x8c820000); instruction(4, 0);
            instruction(8, 0x03e00008); instruction(12, 0);
            ReadOnlyRom codeRom(romBytes);
            rejects([&] { bareCall(memory, 0xbfc4a000, 0, 0xbfc4a000, 0, 0x200000,
                {.rom = &codeRom}); }, "owned bootstrap code RAM");
            codeRom.enableIopBootCode();
            rejects([&] { codeRom.enableIopBootCode(); }, "enabled twice");
            for (auto address : {0xbfc49ffcu, 0xbfc4b120u, 0xbfc4a001u, 0x1fc4a000u, 0x9fc4a000u, 0xffc4a000u})
                if (codeRom.ownsCode(address)) throw std::runtime_error("ROM execution window escaped its reviewed profile");
            memory.write32(0xbfc4a000, 0xfeedface); memory.write32(0xbfc4a004, 0xabcdef01);
            BareCallState romState;
            for (unsigned n = 0; n < 4; ++n) {
                result = bareCall(memory, 0xbfc4a000, 0, 0xbfc4a000, 0, 0x200000,
                    {.rom = &codeRom}, &romState, 1);
                if (memory.read32(0xbfc4a000) != 0xfeedface || memory.read32(0xbfc4a004) != 0xabcdef01 ||
                    memory.ownsRamRange(0x1fc4a000, 4))
                    throw std::runtime_error("ROM fetch leaked staging state or marked code as RAM");
            }
            if (!result.completed || result.value != 0x8c820000 || memory.interruptStatus())
                throw std::runtime_error("Sliced ROM instruction/data load or delay slot failed");
            instruction(0, 0xac800000); // Store to ROM must fail before modifying the owned ROM bytes.
            ReadOnlyRom storeRom(romBytes); storeRom.enableIopBootCode();
            rejects([&] { bareCall(memory, 0xbfc4a000, 0, 0xbfc4a000, 0, 0x200000,
                {.rom = &storeRom}); }, "hardware contract");
            if (memory.read32(0xbfc4a000) != 0xfeedface)
                throw std::runtime_error("Failed ROM instruction leaked its temporary fetch slot");
            instruction(0, 0x08000000 | ((0xbfc4b120 >> 2) & 0x03ffffff));
            ReadOnlyRom escapedRom(romBytes); escapedRom.enableIopBootCode();
            rejects([&] { bareCall(memory, 0xbfc4a000, 0, 0, 0, 0x200000,
                {.rom = &escapedRom}); }, "owned bootstrap code RAM");
            if (memory.read32(0xbfc4a000) != 0xfeedface)
                throw std::runtime_error("Out-of-window ROM branch leaked fetch state");
            ReadOnlyRom absentRom(romBytes);
            code(memory, {0x8c820000, 0, 0x03e00008, 0});
            rejects([&] { bareCall(memory, 0x1000, 0, 0xbe000000, 0x1000, 0x1100,
                {.rom = &absentRom}); }, "hardware contract");
            absentRom.enableAbsentRom1();
            for (auto address : {0x1e000000u, 0x9e000000u, 0xbe000000u, 0xbe3ffffcu}) {
                memory.write32(address, 0xdeadbeef);
                result = bareCall(memory, 0x1000, 0, address, 0x1000, 0x1100, {.rom = &absentRom});
                if (result.value || memory.read32(address) != 0xdeadbeef || memory.interruptStatus())
                    throw std::runtime_error("Absent ROM1 load did not use zero-backed data or leaked staging/IRQ");
            }
            code(memory, {0xac800000, 0x03e00008, 0});
            rejects([&] { bareCall(memory, 0x1000, 0, 0xbe000000, 0x1000, 0x1100,
                {.rom = &absentRom}); }, "hardware contract");
            rejects([&] { bareCall(memory, 0xbe000000, 0, 0, 0, 0x200000,
                {.rom = &absentRom}); }, "owned bootstrap code RAM");
            if (memory.read32(0xbe000000) != 0xdeadbeef)
                throw std::runtime_error("Rejected ROM1 store/fetch modified its backing slot");
        }
        code(memory, {0x3c0a0001, 0x408a6000, 0xad600000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030); }, "explicit coherent-cache mode");
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1030,
            {.cache = &cache}); }, "Unsupported isolated cache access");
        memory.reset(); QuietInterrupts interrupts; interrupts.reset(memory);
        code(memory, {0x24020001, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801078, 0x1000, 0x1030,
            {.interrupts = &interrupts});
        if (result.value != 1 || memory.interruptControl() != 0)
            throw std::runtime_error("I_CTRL read must return prior value and disable interrupts");
        memory.setInterruptStatus(0x42); memory.setInterruptMask(0x40); memory.setInterruptControl(1);
        interrupts.checkDelivery(memory, false); // CP0 global masking must preserve a latched pending source.
        rejects([&] { interrupts.checkDelivery(memory); }, "requires actual exception delivery");
        memory.setInterruptControl(0);
        code(memory, {0xac800000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801070, 0x1000, 0x1030,
            {.interrupts = &interrupts});
        if (result.value != 0 || memory.interruptStatus() != 0)
            throw std::runtime_error("I_STAT acknowledge did not clear actual pending bits");
        memory.reset(); QuiescentDma dma; dma.reset(memory);
        memory.write32(0x1f801534, 0xabcd0000);
        code(memory, {0x24020020, 0xa4820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801534, 0x1000, 0x1100,
            {.dma = &dma});
        if (result.value != 0xabcd0020 || memory.interruptStatus())
            throw std::runtime_error("DMA halfword BCR write corrupted upper half or fabricated an IRQ");
        code(memory, {0x94820002, 0, 0x03e00008, 0}); // lhu v0,2(a0)
        result = bareCall(memory, 0x1000, 0, 0xbf801534, 0x1000, 0x1100,
            {.dma = &dma});
        if (result.value != 0xabcd || dma.allows(0x1f801538, 0x29, 0))
            throw std::runtime_error("DMA halfword BCR read or CHCR width guard failed");
        memory.write32(0x1f801534, 0x20); memory.write32(0x800, 0xdeadbeef);
        dma.enableSif1Receiver();
        code(memory, {0x3c024100, 0x34420300, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801538, 0x1000, 0x1100,
            {.dma = &dma});
        if (result.value != 0x41000300 || memory.read32(0x1f801538) != 0x41000300 ||
            memory.read32(0x1f801534) != 0x20 || memory.read32(0x800) != 0xdeadbeef || memory.interruptStatus())
            throw std::runtime_error("Empty SIF1 receiver fabricated bytes/completion/IRQ or lost STR");
        rejects([&] { dma.allows(0x1f801538, 0x2b, 0x41000300, &memory); }, "active rearm");
        memory.write32(0x1f801538, 0); memory.write32(0x1f801534, 0x21);
        rejects([&] { dma.allows(0x1f801538, 0x2b, 0x41000300, &memory); }, "receiver configuration");
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
        memory.reset(); dma.reset(memory); dma.enableSif0Sender();
        memory.write32(0x1f801524, 0x20); memory.write32(0x1f801578, 1);
        memory.write32(0x1f801570, 0x800); memory.write32(0x1f80152c, 0x3004);
        memory.write32(0x3004, 0x80004000); memory.write32(0x3008, 1);
        memory.write32(0x300c, 0x90000001); memory.write32(0x3010, 0x800);
        memory.write32(0x3014, 0x13579bdf); memory.write32(0x3018, 0x2468ace0);
        memory.write32(0x4000, 0x11223344);
        Sif0Sender sif0; unsigned quanta = 0;
        sif0.bind([&](auto quantum) {
            if (memory.read32(0x1f801528) != 0x01000701 || memory.interruptStatus())
                throw std::runtime_error("SIF0 transfer ran before the actual CHCR store or invented early IRQ");
            if (quanta == 1 && (quantum[0] != 0x44 || quantum[5] != 8 || quantum[8] != 0xdf))
                throw std::runtime_error("IOP SIF0 payload/history differs from actual RAM");
            ++quanta;
            return true;
        });
        code(memory, {0x3c020100, 0x34420701, 0xac820000, 0x8c820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbf801528, 0x1000, 0x1100,
            {.dma = &dma, .sif0 = &sif0});
        if (quanta != 2 || result.value != 0x701 || memory.interruptStatus() != 8 ||
            memory.read32(0x1f801520) != 0x4004 || memory.read32(0x1f801574) != 0x04000000)
            throw std::runtime_error("Actual IOP CHCR store did not deliver SIF0 and latch completion");
        memory.reset(); Sif1Receiver receiver;
        memory.write32(0x1f801538, 0x41000300); memory.write32(0x1f801534, 0x20);
        memory.write32(0x1f801570, 0x8000); memory.write32(0x1f801578, 1);
        memory.write32(0x7fc, 0xdeadbeef); memory.write32(0x820, 0xcafebabe);
        std::array<uint8_t, 16> tag{0, 8, 0, 0xc0, 8}, first{}, second{};
        for (unsigned n = 0; n < 16; ++n) { first[n] = uint8_t(n + 1); second[n] = uint8_t(n + 17); }
        receiver.feed(memory, tag);
        if (receiver.transferredBytes() || receiver.completedChains() || memory.interruptStatus())
            throw std::runtime_error("SIF1 tag alone fabricated a payload/completion/IRQ");
        receiver.feed(memory, first);
        if (receiver.transferredBytes() != 16 || receiver.completedChains() || memory.interruptStatus() ||
            memory.read32(0x1f801538) != 0x41000300 || memory.read32(0x800) != 0x04030201)
            throw std::runtime_error("SIF1 partial payload did not preserve STR and actual bytes");
        receiver.feed(memory, second);
        if (receiver.transferredBytes() != 32 || receiver.completedChains() != 1 ||
            memory.read32(0x81c) != 0x201f1e1d || memory.read32(0x7fc) != 0xdeadbeef ||
            memory.read32(0x820) != 0xcafebabe || memory.read32(0x1f801530) != 0x820 ||
            memory.read32(0x1f801538) != 0x40000300 || memory.read32(0x1f801574) != 0x08000000 ||
            memory.interruptStatus() != 8 || memory.read32(0x1f801534) != 0x20)
            throw std::runtime_error("SIF1 completion/RAM bounds/mandatory channel IRQ failed");
        rejects([&] { receiver.feed(memory, first); }, "ready, enabled");
        if (!dma.allows(0x1f801538, 0x2b, 0x41000300, &memory))
            throw std::runtime_error("A completed SIF1 receiver cannot rearm with its actual previous MADR");
        memory.setInterruptStatus(0);
        dma.afterWrite(memory, 0x1f801574, 0x08000000, 0); // Mask cannot suppress latched SIF1.
        if (memory.interruptStatus() != 8) throw std::runtime_error("SIF1 pending IRQ was incorrectly masked");
        dma.afterWrite(memory, 0x1f801574, memory.read32(0x1f801574), 0x08000000);
        memory.setInterruptStatus(0); memory.write32(0x1f801538, 0x41000300);
        tag[4] = 3;
        rejects([&] { receiver.feed(memory, tag); }, "destination tag");
        if (memory.interruptStatus() || memory.read32(0x1f801574) || receiver.transferredBytes() != 32)
            throw std::runtime_error("Rejected SIF1 tag changed completion or payload state");
        memory.reset(); code(memory, {0x0000000c, 0x2402002a, 0x03e00008, 0});
        OwnedExceptions exceptions;
        rejects([&] { bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
            {.exceptions = &exceptions}); },
                "raised an exception");
        if (!memory.zeroRam(0x80, 0x80)) throw std::runtime_error("Vector fixture range failed");
        const uint32_t handler[] = {0x401a7000, 0, 0x275a0004, 0x03400008, 0x42000010};
        for (unsigned index = 0; index < 5; ++index) memory.write32(0x80 + index * 4, handler[index]);
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
            {.exceptions = &exceptions});
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
        result = bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
            {.interrupts = &interrupts, .exceptions = &irqExceptions});
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
            {.sif = &sif});
        if (result.value != 0x50000) throw std::runtime_error("SIF set flags were overwritten by CPU SW backing storage");
        memory.reset(); SifRegisters peerBus; BareCallState waiting;
        code(memory, {0x3c08bd00, 0x8d020020, 0, 0x1040fffd, 0, 0x03e00008, 0});
        const auto poll = [&] {
            return bareCall(memory, 0x1000, 0, 0, 0x1000, 0x1100,
                {.sif = &peerBus}, &waiting, 16);
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
                {.rom = &rom});
            if (result.value != 0x92345678 || memory.read32(base) != 0)
                throw std::runtime_error("Read-only ROM alias/load delay or staging restoration failed");
        }
        code(memory, {0x90820000, 0, 0x03e00008, 0});
        result = bareCall(memory, 0x1000, 0, 0xbfffffffu, 0x1000, 0x1100,
            {.rom = &rom});
        if (result.value != 0xa5 || rom.allows(0xc0000000, 0x24) || rom.allows(0xfffe0130, 0x23) ||
            rom.allows(0x7fc00000, 0x23) || rom.allows(0xbfc00000, 0x2b))
            throw std::runtime_error("ROM bounds, write protection or KSEG3 exclusion failed");
        code(memory, {0xac800000, 0x03e00008, 0});
        rejects([&] { bareCall(memory, 0x1000, 0, 0xbfc00000, 0x1000, 0x1100,
            {.rom = &rom}); }, "hardware contract");
        rejects([&] { bareCall(memory, 0xbfc00000, 0, 0, 0x1000, 0x1100,
            {.rom = &rom}); }, "owned bootstrap code");
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
