#pragma once
#include "bare_call.h"
#include "sif1_receiver.h"
#include "udnl_code_profile.h"
#include "services/iop_module_loader.h"
#include <cstring>
#include <span>

class BootstrapSession {
    using Memory = ps2x::iop::detail::IopMemory;
    using Loader = ps2x::iop::detail::IopModuleLoader;
    static constexpr uint32_t SourcePool = 0xf0000, StackGuard = Memory::RamSize - 8192;
    Memory memory;
    PassiveHardware hardware;
    CoherentCache cache;
    QuietInterrupts interrupts;
    QuiescentDma dma;
    CdvdDma cdvdDma;
    Sif1Receiver sif1;
    Sif0Sender sif0;
    PioWriteCapture pio;
    QuiescentSio2 sio2;
    Sio2Dma sio2Dma{sio2};
    SpdifSetup spdif;
    SpuVolumeRegisters volumes;
    SpuQuiescentControl spuControl;
    SpuIdleVoices voices;
    SpuReverbSetup reverb;
    SpuRamWritePort spuRam;
    SpuDmaWrite spuDma{spuRam,spuControl};
    std::optional<VerifiedRamCode> ramCode;
    OwnedExceptions exceptions;
    drakengard::IopTimers timers;
    ReadOnlyRom rom;
    drakengard::CdvdDevice cdvd;
    BareCallState state;
    std::vector<uint8_t> loadcoreImage;
    uint32_t start{}, count{}, entry{}, gp{}, argument{Memory::RamSize};
    bool inLoadcore{};
    static uint32_t word(std::span<const uint8_t> bytes, size_t at) {
        if (at > bytes.size() || bytes.size() - at < 4) throw std::runtime_error("Short bootstrap word");
        return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 |
            uint32_t(bytes[at + 2]) << 16 | uint32_t(bytes[at + 3]) << 24;
    }
public:
    SifRegisters sif;
    void enableVirtualPad() { sio2.enableVirtualPad(); dma.enableSio2(sio2Dma); }
    void configureRuntimeBudget(uint32_t limit) { state.budget.configureRuntimeLimit(limit); }
    void publishPadInput(const VirtualPadInput& input) { sio2.publishInput(input); }
    void videoEvent(bool start) {
        if (!state.budget.inRuntime()) return;
        interrupts.videoEvent(memory,start);
    }
    void enableSif1Receiver() { dma.enableSif1Receiver(); }
    void feedSif1(std::span<const uint8_t> quantum) { sif1.feed(memory, quantum); }
    void bindSif0(std::function<bool(std::span<const uint8_t>)> sink) {
        sif0.bind(std::move(sink)); dma.enableSif0Sender();
    }
    void pumpSif0() { sif0.pump(memory); }
    void enableEntryDevices() {
        pio.enable(); cache.enableRebootControls(); rom.enableIopBootCode(); rom.enableAbsentRom1();
        sio2.enable(); dma.enableCdvdReceiver();
        ramCode = makeUdnlCodeProfile(rom, start, SourcePool);
        state.budget.enableGuestReboot();
        spdif.enable(); volumes.enable(); spuRam.enable();
        voices.enable(timers.cycles(),&spuRam); reverb.enable(&spuRam); spuControl.enable(&reverb,true);
        reverb.enableProcessing();
        spuDma.bindSampleClock();
        dma.enableSpuWriter(spuDma);
    }
    void setClockSource(std::function<drakengard::CdvdCalendar()> source) { cdvd.setClockSource(std::move(source)); }
    BootstrapSession(std::span<const uint8_t> bundle, std::span<const uint8_t> seed,
                     std::vector<uint8_t> romBytes, drakengard::IsoMedia& media,
                     std::array<uint8_t, 4> mecha)
        : rom(std::move(romBytes)), cdvd(media, mecha) {
        if (bundle.size() < 16 || bundle.size() > 512 * 1024 || std::memcmp(bundle.data(), "D1IOP01\0", 8))
            throw std::runtime_error("Invalid bootstrap bundle");
        start = word(bundle, 8); count = word(bundle, 12);
        if (start > 0x100000 || start % 16 || count < 2 || count > 128)
            throw std::runtime_error("Invalid bootstrap layout");
        interrupts.reset(memory); dma.reset(memory); hardware.seed(seed, memory); cache.initialize(memory);
        const uint32_t pointers = SourcePool + 32;
        uint32_t source = (pointers + (count + 1) * 4 + 15) & ~15u;
        size_t position = 16;
        std::vector<uint8_t> sysmemImage;
        for (uint32_t index = 0; index < count; ++index) {
            if (position > bundle.size() || bundle.size() - position < 16)
                throw std::runtime_error("Short bootstrap record");
            const std::string name(reinterpret_cast<const char*>(bundle.data() + position), 10);
            if ((index == 0 && name != std::string("SYSMEM\0\0\0\0", 10)) ||
                (index == 1 && name != std::string("LOADCORE\0\0", 10)) || bundle[position + 10] || bundle[position + 11])
                throw std::runtime_error("Invalid bootstrap record identity/order");
            const auto bytes = word(bundle, position + 12); position += 16;
            if (bytes < 52 || bytes > bundle.size() - position || source >= StackGuard || bytes > StackGuard - source)
                throw std::runtime_error("Bootstrap source exceeds protected pool");
            const auto image = bundle.subspan(position, bytes);
            if (std::memcmp(image.data(), "\x7f" "ELF", 4) || !memory.writeRam(source, image.data(), image.size()))
                throw std::runtime_error("Invalid bootstrap ELF source");
            if (index == 0) sysmemImage.assign(image.begin(), image.end());
            if (index == 1) loadcoreImage.assign(image.begin(), image.end());
            memory.write32(pointers + index * 4, source);
            position += bytes; source = (source + bytes + 15) & ~15u;
        }
        if (position != bundle.size() || source + 256 >= StackGuard)
            throw std::runtime_error("Trailing/oversized bootstrap source pool");
        memory.write32(pointers + count * 4, 0);
        const std::array<uint32_t, 8> reset{2, 3, 0, start + 0x30, SourcePool, source - SourcePool + 256, count, pointers};
        for (unsigned i = 0; i < reset.size(); ++i) memory.write32(SourcePool + i * 4, reset[i]);
        const auto loaded = Loader::load(sysmemImage, memory, start + 0x30);
        if (!loaded || !loaded.relocationsComplete || loaded.base != start + 0x30 ||
            loaded.base + uint64_t(loaded.size) >= SourcePool) throw std::runtime_error("SYSMEM image refused");
        entry = loaded.entry; gp = loaded.gp;
    }
    void advance(uint32_t instructions) {
        pumpSif0();
        const IopDevices devices{
            .hardware = &hardware, .cache = &cache, .interrupts = &interrupts,
            .dma = &dma, .exceptions = &exceptions, .cdvd = &cdvd,
            .timers = &timers, .rom = &rom, .sif = &sif, .sif0 = &sif0, .pio = &pio, .sio2 = &sio2,
            .ramCode = ramCode ? &*ramCode : nullptr, .cdvdDma = ramCode ? &cdvdDma : nullptr,
            .spdif = &spdif, .volumes = &volumes, .spuControl = &spuControl, .voices = &voices, .reverb = &reverb, .spuRam = &spuRam};
        const auto result = bareCall(memory, entry, gp, argument, start, SourcePool,
                                    devices, &state, instructions);
        if (!result.completed) return;
        if (inLoadcore) throw std::runtime_error("LOADCORE returned without a supported scheduler handoff");
        std::cerr << "[cooperative-sysmem] steps=" << result.instructions << " free_start=0x"
                  << std::hex << result.value << std::dec << '\n';
        if (result.value % 16 || result.value >= SourcePool - 0x30)
            throw std::runtime_error("Invalid SYSMEM free boundary");
        const auto loaded = Loader::load(loadcoreImage, memory, result.value + 0x30);
        if (!loaded || !loaded.relocationsComplete || loaded.base != result.value + 0x30 ||
            loaded.base + uint64_t(loaded.size) >= SourcePool) throw std::runtime_error("LOADCORE image refused");
        entry = loaded.entry; gp = loaded.gp; argument = SourcePool; state = {}; inLoadcore = true;
        std::cerr << "[cooperative-loadcore] entry=0x" << std::hex << entry << std::dec << " modules=" << count << '\n';
    }
};
