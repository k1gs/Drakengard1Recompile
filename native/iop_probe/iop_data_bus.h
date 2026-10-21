#pragma once
#include "core/iop_memory.h"
#include "core/iop_cpu.h"
#include "passive_hardware.h"
#include "coherent_cache.h"
#include "quiet_interrupts.h"
#include "quiescent_dma.h"
#include "owned_exceptions.h"
#include "cdvd_device.h"
#include "iop_timers.h"
#include "read_only_rom.h"
#include "sif_registers.h"
#include "sif0_sender.h"
#include "pio_write_capture.h"
#include "quiescent_sio2.h"
#include "verified_ram_code.h"
#include "spdif_setup.h"
#include "spu_volume.h"
#include "spu_quiescent_control.h"
#include "spu_idle_voices.h"
#include "spu_reverb_setup.h"
#include "spu_ram_write_port.h"
#include <iostream>
#include <stdexcept>
#include <optional>
#include <utility>

// Optional devices are named at call sites; absent devices remain unsupported.
struct IopDevices {
    const PassiveHardware* hardware{};
    CoherentCache* cache{};
    QuietInterrupts* interrupts{};
    QuiescentDma* dma{};
    OwnedExceptions* exceptions{};
    drakengard::CdvdDevice* cdvd{};
    drakengard::IopTimers* timers{};
    const ReadOnlyRom* rom{};
    SifRegisters* sif{};
    Sif0Sender* sif0{};
    PioWriteCapture* pio{};
    QuiescentSio2* sio2{};
    VerifiedRamCode* ramCode{};
    CdvdDma* cdvdDma{};
    SpdifSetup* spdif{};
    SpuVolumeRegisters* volumes{};
    SpuQuiescentControl* spuControl{};
    SpuIdleVoices* voices{};
    SpuReverbSetup* reverb{};
    SpuRamWritePort* spuRam{};
};

// Adapts explicit device contracts to the pinned CPU's backing memory.
// prepare() runs before the instruction; complete() preserves the existing
// post-instruction order, load delays, acknowledgements and one-cycle timer tick.
class IopDataBus {
    using Memory = ps2x::iop::detail::IopMemory;
    using Cpu = ps2x::iop::detail::IopCpuState;
    Memory& memory;
    const IopDevices devices;
public:
    struct PioWrite { uint32_t opcode{}, value{}, pc{}; };
    struct Effects {
        uint32_t interruptRead{}, dmaWrite{}, dmaPrevious{}, dmaValue{}, sifWrite{};
        std::optional<std::pair<uint32_t, uint32_t>> romRestore;
        bool sif0Start{};
        std::optional<PioWrite> pioWrite;
        std::optional<CoherentCache::ControlWrite> cpuControl;
        bool sio2Write{};
        bool bootEndRise{};
        std::optional<std::pair<uint32_t, uint32_t>> spdifWrite;
        std::optional<SpuVolumeRegisters::Access> volumeAccess;
        std::optional<SpuQuiescentControl::Access> controlAccess;
        std::optional<SpuIdleVoices::Access> voiceAccess;
        std::optional<SpuReverbSetup::Access> reverbAccess;
        std::optional<SpuRamWritePort::Access> ramAccess;
    };
    IopDataBus(Memory& memory, const IopDevices& devices) : memory(memory), devices(devices) {}
    Effects prepare(const Cpu& cpu, uint32_t instruction, uint32_t steps) {
        using ps2x::iop::detail::IopMemory;
        const auto& [hardware, cache, interrupts, dma, exceptions, cdvd, timers, rom, sif, sif0, pio, sio2, ramCode, cdvdDma, spdif, volumes, spuControl, voices, reverb, spuRam] = devices;
        const uint32_t opcode = instruction >> 26;
        Effects effects;
        auto& [interruptRead, dmaWrite, dmaPrevious, dmaValue, sifWrite, romRestore, sif0Start, pioWrite, cpuControl, sio2Write, bootEndRise, spdifWrite, volumeAccess, controlAccess, voiceAccess, reverbAccess, ramAccess] = effects;
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
            const bool pioAccess = pio && pio->allows(memory, target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            const bool sio2Access = sio2 && sio2->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            const bool spdifAccess = spdif && spdif->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            const bool volumeSupported = volumes && volumes->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            const bool controlSupported = spuControl && spuControl->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            const bool voiceSupported = voices && voices->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            const bool reverbSupported = reverb && reverb->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            const bool ramSupported = spuRam && spuRam->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            if (dma && opcode == 0x2b && dma->channelRegister(target) && (target & 15) == 8 &&
                (cpu.gpr[(instruction >> 16) & 31] & 0x01000000)) {
                const auto base = target & ~15u;
                std::cerr << "[strict-iop-dma-start] pc=0x" << std::hex << cpu.pc << " register=0x" << target
                          << " chcr=0x" << cpu.gpr[(instruction >> 16) & 31] << " madr=0x" << memory.read32(base)
                          << " bcr=0x" << memory.read32(base + 4) << std::dec << " steps=" << steps << '\n';
                if (base == 0x1f801520) {
                    const auto tadr = memory.read32(base + 12);
                    std::cerr << "[strict-sif0-source] tadr=0x" << std::hex << tadr;
                    if (!(tadr & 3) && tadr < IopMemory::RamSize && IopMemory::RamSize - tadr >= 16)
                        for (unsigned n = 0; n < 4; ++n)
                            std::cerr << " tag" << n << "=0x" << memory.read32(tadr + n * 4);
                    std::cerr << std::dec << '\n';
                }
            }
            if ((target >= IopMemory::RamSize || width > IopMemory::RamSize - target) &&
                !(hardware && hardware->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31])) &&
                !(cache && cache->allows(address, opcode, cpu.gpr[(instruction >> 16) & 31])) &&
                !(interrupts && interrupts->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31])) &&
                !(dma && dma->allows(target, opcode, cpu.gpr[(instruction >> 16) & 31], &memory)) &&
                !cdvdAccess && !timerAccess && !romAccess && !sifAccess && !pioAccess && !sio2Access && !spdifAccess && !volumeSupported && !controlSupported && !voiceSupported && !reverbSupported && !ramSupported) {
                std::cerr << "[bootstrap-mmio-blocker] steps=" << steps << " pc=0x" << std::hex << cpu.pc
                          << " instruction=0x" << instruction << " address=0x" << address << std::dec
                          << " rt_value=0x" << std::hex << cpu.gpr[(instruction >> 16) & 31] << std::dec
                          << " ra=0x" << std::hex << cpu.gpr[31] << " a0=0x" << cpu.gpr[4]
                          << " a1=0x" << cpu.gpr[5] << std::dec
                          << " isolated_cache_stores=" << (cache ? cache->isolatedStores : 0) << '\n';
                throw std::runtime_error("Non-RAM data access requires an explicit hardware contract");
            }
            if (ramSupported) ramAccess = spuRam->stage(memory, address, target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            if (reverbSupported) reverbAccess = reverb->stage(memory, address, target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            if (voiceSupported) voiceAccess = voices->stage(memory, address, target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            if (controlSupported) controlAccess = spuControl->stage(memory, address, target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            if (volumeSupported) volumeAccess = volumes->stage(memory, address, target, opcode, cpu.gpr[(instruction >> 16) & 31]);
            if (romAccess) romRestore = rom->stage(memory, address, opcode);
            if (cache && cache->rebootWrite(address, opcode, cpu.gpr[(instruction >> 16) & 31]))
                cpuControl = cache->stageControl(memory, address, opcode, cpu.gpr[(instruction >> 16) & 31], cpu.pc);
            if (pioAccess) pioWrite = PioWrite{opcode, cpu.gpr[(instruction >> 16) & 31], cpu.pc};
            sio2Write = sio2Access && opcode == 0x2b;
            if (spdifAccess) {
                if (opcode==0x29) spdifWrite = std::pair{target, cpu.gpr[(instruction >> 16) & 31]};
                else memory.write16(address,spdif->read(target));
            }
            if (sifAccess) {
                if (opcode == 0x2b) {
                    bootEndRise = target == 0x1d000030 && (cpu.gpr[(instruction >> 16) & 31] & 0x40000u) &&
                        !(sif->read(target) & 0x40000u);
                    sif->write(target, cpu.gpr[(instruction >> 16) & 31]);
                    sifWrite = target;
                }
                memory.write32(address, sif->read(target));
            }
            if (cdvdAccess) {
                const auto key = static_cast<uint8_t>(target);
                try {
                    if (opcode == 0x28) cdvd->writeRegister(key, static_cast<uint8_t>(cpu.gpr[(instruction >> 16) & 31]));
                    else memory.write8(address, cdvd->readRegister(key));
                } catch (const std::exception&) {
                    std::cerr << "[iop-cdvd-blocker] pc=0x" << std::hex << cpu.pc << " register=0x" << target
                              << " opcode=0x" << opcode << " value=0x" << cpu.gpr[(instruction >> 16) & 31]
                              << std::dec << " steps=" << steps << '\n';
                    throw;
                }
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
            sif0Start = sif0 && opcode == 0x2b && target == 0x1f801528 &&
                (cpu.gpr[(instruction >> 16) & 31] & 0x01000000u);
        }
        return effects;
    }
    void complete(const Cpu& cpu, const Effects& effects) {
        const auto& [hardware, cache, interrupts, dma, exceptions, cdvd, timers, rom, sif, sif0, pio, sio2, ramCode, cdvdDma, spdif, volumes, spuControl, voices, reverb, spuRam] = devices;
        const auto& [interruptRead, dmaWrite, dmaPrevious, dmaValue, sifWrite, romRestore, sif0Start, pioWrite, cpuControl, sio2Write, bootEndRise, spdifWrite, volumeAccess, controlAccess, voiceAccess, reverbAccess, ramAccess] = effects;
        // Temporary staging preserves the upstream core's load-delay behavior
        // without leaving ROM words in the masked hardware map (BIU aliases).
        if (ramAccess) spuRam->complete(memory, *ramAccess, cpu.exception);
        if (reverbAccess) reverb->complete(memory, *reverbAccess, cpu.exception);
        if (voiceAccess) voices->complete(memory, *voiceAccess, cpu.exception);
        if (controlAccess) spuControl->complete(memory, *controlAccess, cpu.exception);
        if (volumeAccess) volumes->complete(memory, *volumeAccess, cpu.exception);
        if (romRestore) memory.write32(romRestore->first, romRestore->second);
        if (cpuControl) cache->completeControl(memory, *cpuControl, cpu.exception);
        if (sifWrite) memory.write32(sifWrite, sif->read(sifWrite));
        if (!cpu.exception && bootEndRise) sif->observeRetiredBootEnd();
        if (!cpu.exception && spdifWrite) spdif->retire(spdifWrite->first, spdifWrite->second);
        if (!cpu.exception && interruptRead) interrupts->afterRead(memory, interruptRead);
        if (!cpu.exception && dmaWrite) dma->afterWrite(memory, dmaWrite, dmaPrevious, dmaValue);
        if (!cpu.exception && sif0Start) sif0->start(memory);
        if (!cpu.exception && pioWrite) pio->afterWrite(memory, pioWrite->opcode, pioWrite->value, pioWrite->pc);
        if (!cpu.exception && sio2Write) sio2->afterWrite(memory);
        if (cdvdDma && cdvd) cdvdDma->pump(memory, *cdvd);
        if (cdvd && cdvd->takeInterruptEdge()) memory.setInterruptStatus(memory.interruptStatus() | 4u);
        if (timers) memory.setInterruptStatus(memory.interruptStatus() | timers->advance(1));
        if (dma) dma->pumpSpu(memory);
        if (voices && timers) voices->advanceTo(timers->cycles());
        if (reverb && voices) reverb->advanceSilentInputs(voices->sampleCount());
    }
};
