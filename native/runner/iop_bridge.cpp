#include "iop_bridge.h"
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <array>
#include <cstdlib>
#include "../iop_probe/sif_registers.h"
#include "../iop_probe/ee_sif_dma.h"
#include "../iop_probe/sif0_fifo.h"
#include "strict_device_error.h"
#include "../iop_probe/cooperative_clock.h"
#include "ee_write_watch.h"
#include "ee_intc_status.h"
#include "ee_ipu_dma.h"

namespace drakengard {
namespace {
// One owner for the verified IOP, its lifecycle and the EE-facing bus state.
// Public hooks below serialize access; internal calls stay within this owner.
class CooperativeIopBridge {
    std::unique_ptr<IopBoot> boot;
    enum class Phase { Prepared, Reboot, Firmware, FirmwareEntry };
    Phase phase{Phase::Prepared};
    bool active() const { return phase != Phase::Prepared; }
    bool firmwareExperiment() const { return phase == Phase::Firmware || phase == Phase::FirmwareEntry; }
    EeSifDma firmwareDma;
    EeIntcStatus intcStatus;
    EeIpu ipu;
    EeIpuDma ipuDma;
    uint64_t ipuCommands{};
    uint64_t ipuOutputTransfers{};
    uint32_t ipuFromStart{},ipuFromQwc{};
    Sif0Fifo sif0Fifo;
    unsigned deferredQuanta{};
    std::span<uint8_t> eeRam;
    std::span<uint8_t> eeScratchpad;
    std::function<uint32_t()> dmaControl;
    unsigned reads{};
    unsigned idleSlices{};
    CooperativeClock executionClock;
    unsigned runningSlices{};
    bool videoEvents{};
    std::array<uint32_t, 3> softwareRegisters{};
    uint32_t busRegister(uint32_t number) {
        return SifRegisters::kernelAddress(number);
    }
    bool busAddress(uint32_t address) { return address >= 0x1000f200 && address <= 0x1000f260; }
    void drainSif0() {
        sif0Fifo.drain([&](auto quantum) {
            const auto control = dmaControl();
            if (!firmwareDma.canReceiveSif0(control)) return false;
            if (eeWriteWatchSlot!=0xffffffffu && quantum.size()==16 && firmwareDma.read(0x1000c020)) {
                const auto destination=firmwareDma.read(0x1000c010);
                if (uint64_t(destination)<uint64_t(eeWriteWatchSlot)+8u &&
                    uint64_t(destination)+16u>eeWriteWatchSlot) {
                    uint64_t low{},high{};
                    std::memcpy(&low,quantum.data(),8);
                    std::memcpy(&high,quantum.data()+8,8);
                    traceEeWatchedWrite(eeRam.data(),destination,16u,low,high,"SIF0_DMA",nullptr);
                }
            }
            firmwareDma.feedSif0(eeRam, quantum, control);
            return true;
        });
    }
    void serviceSif0() {
        if (phase != Phase::FirmwareEntry) return;
        try { drainSif0(); boot->pumpSif0(); drainSif0(); }
        catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
    }
    void pumpIpu() {
        const bool enabled=dmaControl && (dmaControl()&1u) && !(firmwareDma.read(0x1000f520u)&0x10000u);
        ipuDma.pump(ipu,eeRam,eeScratchpad,enabled);
    }
public:
    uint32_t readIntcStatus() const { return intcStatus.read(); }
    void acknowledgeIntcStatus(uint32_t value) { intcStatus.acknowledge(value); }
    void latchIntcCause(uint32_t cause) { intcStatus.raise(cause); }
    uint32_t readIpu(uint32_t address) {
        pumpIpu();
        return EeIpuDma::owns(address)?ipuDma.read(address):ipu.read(address);
    }
    uint64_t readIpu64(uint32_t address) { pumpIpu(); return ipu.read64(address); }
    void writeIpu(uint32_t address,uint32_t value) {
        pumpIpu();
        if (EeIpuDma::owns(address)) {
            ipuDma.write(address,value);
            if (address==0x1000b000u && (value&0x100u)) {
                ipuFromStart=ipuDma.read(0x1000b010u); ipuFromQwc=ipuDma.read(0x1000b020u);
            }
        }
        else ipu.write(address,value);
        pumpIpu();
        if (address==EeIpu::Command) {
            const auto count=++ipuCommands;
            if (count<=16 || (count&(count-1))==0)
                std::cerr << "[ee-ipu-command] value=0x" << std::hex << value
                          << " result=0x" << ipu.diagnosticCommand()
                          << " bp=0x" << ipu.read(EeIpu::Position) << std::dec << " count=" << count << '\n';
        }
    }
    void writeIpuInput(std::span<const uint8_t,16> bytes) {
        pumpIpu();
        if (!ipu.push(bytes)) throw StrictDeviceError("CPU IPU input FIFO write would overflow");
        pumpIpu();
    }
    void readIpuOutput(std::span<uint8_t,16> bytes) {
        pumpIpu();
        if (!ipu.pop(bytes)) throw StrictDeviceError("CPU IPU output FIFO is empty");
        pumpIpu();
    }
    bool takeIpuInterrupt() { pumpIpu(); return ipu.takeInterrupt(); }
    uint32_t takeIpuDmaCompletions() {
        pumpIpu(); const auto causes=ipuDma.takeCompletions();
        if (causes&8u) {
            const auto count=++ipuOutputTransfers;
            if (count<=16 || (count&(count-1))==0) {
                std::cerr << "[ee-ipu-from-complete] start=0x" << std::hex << ipuFromStart
                          << " end=0x" << ipuDma.read(0x1000b010u) << " qwc=0x" << ipuFromQwc;
                if ((ipuFromStart&0x80000000u) && (ipuFromStart&0x7fffffffu)<eeScratchpad.size()) {
                    const auto offset=ipuFromStart&0x7fffffffu;
                    const uint64_t size=uint64_t(ipuFromQwc)*16;
                    if (size<=eeScratchpad.size()-offset) {
                        uint64_t hash=14695981039346656037ull;
                        for (uint64_t i=0;i<size;++i) hash=(hash^eeScratchpad[offset+i])*1099511628211ull;
                        std::cerr << " scratch_fnv1a64=0x" << hash;
                    }
                }
                std::cerr << " ctrl=0x" << ipu.read(EeIpu::Control) << " top=0x" << ipu.read(EeIpu::Top)
                          << std::dec << " count=" << count << '\n';
            }
        }
        return causes;
    }
    void videoEvent(bool start) {
        if (videoEvents && phase==Phase::FirmwareEntry) boot->videoEvent(start);
    }
    void publishPadInput(const VirtualPadInput& input) {
        if (boot) boot->publishPadInput(input);
    }
    void advanceFirmwareIopForEeCycles(uint64_t cycles) {
        if (phase!=Phase::FirmwareEntry) return;
        if (const auto quantum=executionClock.charge(cycles)) {
            if (++runningSlices<=4 || runningSlices%256==0)
                std::cerr << "[firmware-running-iop] slice=" << runningSlices << " ee_cycle=" << cycles
                          << " instructions=" << quantum << '\n';
            boot->advance(quantum);
            serviceSif0();
        }
    }
    bool firmwareGameEntryActive() const { return phase == Phase::FirmwareEntry; }
    void bindFirmwareEeRam(std::span<uint8_t> ram, std::span<uint8_t> scratchpad,
                           uint32_t initialEnableWord, std::function<uint32_t()> control) {
        if (phase != Phase::Prepared || !eeRam.empty() || ram.size() != 32 * 1024 * 1024 ||
            scratchpad.size() != 0x4000u)
            throw StrictDeviceError("Firmware EE memory binding requires fresh 32-MiB RDRAM and 16-KiB scratchpad");
        if (!control) throw StrictDeviceError("EE DMA control reader is required");
        eeRam = ram; eeScratchpad = scratchpad; dmaControl = std::move(control);
        firmwareDma.seedEnableWord(initialEnableWord);
    }
    bool takeFirmwareDmaCompletion() { return firmwareDma.takeCompletion(); }
    bool takeFirmwareSif0Completion() { serviceSif0(); return firmwareDma.takeSif0Completion(); }
    void advanceFirmwareIop() {
        if (phase != Phase::FirmwareEntry) throw StrictDeviceError("Firmware DMA call requires experimental game entry");
        boot->advance(4096);
    }
    bool advanceFirmwareIopWhileEeIdle() {
        if (phase != Phase::FirmwareEntry) return false;
        if (++idleSlices <= 4 || idleSlices % 256 == 0)
            std::cerr << "[firmware-idle-iop] slice=" << idleSlices << " instructions=4096\n";
        boot->advance(4096);
        return true;
    }
    bool strictIopActive() const { return active(); }
    void enterGameAfterFirmware() {
        if (phase != Phase::Firmware || eeRam.empty() || !(boot->readEe(0x1000f230) & 0x40000u))
            throw StrictDeviceError("Game entry requires actual completed firmware IOP startup");
        phase = Phase::FirmwareEntry;
        if (const auto* enabled=std::getenv("DRAKENGARD_IOP_VIDEO_EVENTS")) {
            if (std::string_view(enabled)!="1") throw StrictDeviceError("Unsupported IOP video event profile");
            videoEvents=true;
            std::cerr << "[iop-video-events] shared EE VBlank start/end enabled; scheduler timing remains approximate\n";
        }
        firmwareDma.enableSif1();
        boot->enableEntryDevices();
        if (const auto* enabled=std::getenv("DRAKENGARD_VIRTUAL_PAD")) {
            if (std::string_view(enabled)!="1" || !videoEvents)
                throw StrictDeviceError("Virtual pad requires shared IOP video events");
            boot->enableVirtualPad();
            std::cerr << "[virtual-pad] emulated DualShock2 port1; real SIO2 packets, host input snapshots\n";
        }
        boot->bindSif0([this](std::span<const uint8_t> quantum) {
            drainSif0();
            if (!sif0Fifo.push(quantum)) return false;
            drainSif0();
            if (sif0Fifo.size() && (++deferredQuanta <= 4 || deferredQuanta % 256 == 0))
                std::cerr << "[strict-sif0-buffered] quanta=" << sif0Fifo.size() << " deferred=" << deferredQuanta << '\n';
            return true;
        });
    }
    void prepareCooperativeIop(const IopBootInputs& inputs) {
        if (boot) throw std::runtime_error("Cooperative IOP already configured");
        boot = std::make_unique<IopBoot>(inputs);
    }
    void activateCooperativeIop(std::string_view arguments, uint32_t mode) {
        if (!boot) throw std::runtime_error("IOP reboot requires configured verified bootstrap inputs");
        if (active() || mode || arguments != "rom0:UDNL cdrom0:\\MODULES\\IOPRP280.IMG;1")
            throw std::runtime_error("Unsupported repeated/different IOP reboot request");
        boot->advance(4096); // Actually execute SYSMEM before accepting this request.
        phase = Phase::Reboot;
        std::cerr << "[cooperative-iop] reboot accepted; real CPU active; no BOOTEND injected\n";
    }
    void beginFirmwareExperiment() {
        if (!boot || active()) throw std::runtime_error("Firmware experiment requires a fresh configured strict IOP");
        boot->enableSif1Receiver();
        boot->enableHostRtc();
        boot->advance(4096);
        phase = Phase::Firmware;
        std::cerr << "[firmware-experiment] selected IOP boot plan active; bounded functional scheduling, not a full BIOS reset\n";
    }
    void finishFirmwareExperiment() {
        if (!firmwareExperiment()) throw StrictDeviceError("Firmware continuation without an active component experiment");
        std::cerr << "[post-firmware-iop] continue actual IOP startup; wait for guest-owned BOOTEND\n";
        for (unsigned slice = 0; slice < 2048; ++slice) {
            const auto status = boot->readEe(0x1000f230);
            if (status & 0x40000) {
                std::cerr << "[post-firmware-iop] actual guest BOOTEND; SMFLAG=0x" << std::hex << status << std::dec << '\n';
                return;
            }
            boot->advance(4096);
        }
        throw StrictDeviceError("Post-firmware IOP slice bound exhausted");
    }
    std::optional<uint32_t> firmwareDmaRead(uint32_t address) {
        if (!firmwareExperiment() || !EeSifDma::contains(address)) return std::nullopt;
        try { return firmwareDma.read(address); }
        catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
    }
    bool firmwareDmaWrite(uint32_t address, uint32_t value) {
        if (!firmwareExperiment() || !EeSifDma::contains(address)) return false;
        try {
            firmwareDma.write(address, value);
            if (address == 0x1000c000 || address == 0x1000f590) serviceSif0();
            if (address == 0x1000c400 && (value & 0x100u))
                firmwareDma.pumpSif1(eeRam, [&](std::span<const uint8_t> quantum) { boot->feedSif1(quantum); });
        }
        catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
        std::cerr << "[firmware-ee-dma] address=0x" << std::hex << address << " value=0x" << value << std::dec << '\n';
        return true;
    }
    std::optional<uint32_t> cooperativeSifRead(uint32_t address) {
        if (!active() || !busAddress(address)) return std::nullopt;
        boot->advance(4096);
        const auto value = boot->readEe(address);
        if (++reads <= 16 || reads % 256 == 0)
            std::cerr << "[cooperative-sif-read] address=0x" << std::hex << address << " value=0x" << value << std::dec << '\n';
        return value;
    }
    std::optional<uint32_t> cooperativeSifGetReg(uint32_t number) {
        if (!active()) return std::nullopt;
        boot->advance(4096);
        const auto value = number >= 0x80000000u && number <= 0x80000002u ?
            softwareRegisters[number - 0x80000000u] : boot->readEe(busRegister(number));
        if (++reads <= 16 || reads % 256 == 0)
            std::cerr << "[cooperative-sif-getreg] number=0x" << std::hex << number << " value=0x" << value << std::dec << '\n';
        return value;
    }
    std::optional<uint32_t> cooperativeSifSetReg(uint32_t number, uint32_t value) {
        if (number >= 0x80000000u && number <= 0x80000002u) {
            auto& slot = softwareRegisters[number - 0x80000000u];
            const auto previous = slot; slot = value;
            if (!active()) return std::nullopt; // Observe real pre-reset writes; original kernel path still runs.
            std::cerr << "[cooperative-sif-sysreg] number=0x" << std::hex << number << " value=0x" << value << std::dec << '\n';
            boot->advance(4096);
            return previous;
        }
        if (!active()) {
            std::cerr << "[observed-ee-sif-setreg] number=0x" << std::hex << number << " value=0x" << value << std::dec << '\n';
            if (boot && (number == 1 || number == 3 || number == 4)) boot->writeEe(busRegister(number), value);
            return std::nullopt;
        }
        const auto address = busRegister(number), previous = boot->readEe(address);
        cooperativeSifWrite(address, value);
        return previous;
    }
    bool cooperativeSifWrite(uint32_t address, uint32_t value) {
        if (!active() || !busAddress(address)) return false;
        std::cerr << "[cooperative-sif-write] address=0x" << std::hex << address << " value=0x" << value << std::dec << '\n';
        boot->writeEe(address, value);
        boot->advance(4096);
        return true;
    }
};

std::recursive_mutex bridgeMutex;
CooperativeIopBridge bridge;
}

std::optional<uint32_t> eeIntcRead(uint32_t address) {
    if (address != EeIntcStatus::Address) return std::nullopt;
    std::lock_guard lock(bridgeMutex);
    return bridge.readIntcStatus();
}
bool eeIntcWrite(uint32_t address, uint32_t value) {
    if (address != EeIntcStatus::Address) return false;
    std::lock_guard lock(bridgeMutex);
    bridge.acknowledgeIntcStatus(value);
    return true;
}
void latchEeIntcCause(uint32_t cause) {
    std::lock_guard lock(bridgeMutex);
    bridge.latchIntcCause(cause);
}

std::optional<uint32_t> eeIpuRead(uint32_t address) {
    if (!EeIpu::ownsRegisters(address) && !EeIpuDma::owns(address)) return std::nullopt;
    std::lock_guard lock(bridgeMutex);
    try { return bridge.readIpu(address); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
std::optional<uint64_t> eeIpuRead64(uint32_t address) {
    if (!EeIpu::ownsRegisters(address)) return std::nullopt;
    std::lock_guard lock(bridgeMutex);
    try { return bridge.readIpu64(address); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
bool eeIpuWrite(uint32_t address,uint32_t value) {
    if (!EeIpu::ownsRegisters(address) && !EeIpuDma::owns(address)) return false;
    std::lock_guard lock(bridgeMutex);
    try { bridge.writeIpu(address,value); return true; }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
bool eeIpuWrite64(uint32_t address,uint64_t value) {
    if (!EeIpu::ownsRegisters(address)) return false;
    if (address!=EeIpu::Command) throw StrictDeviceError("Unsupported IPU 64-bit register write");
    return eeIpuWrite(address,static_cast<uint32_t>(value));
}
void writeEeIpuInput(std::span<const uint8_t,16> bytes) {
    std::lock_guard lock(bridgeMutex);
    try { bridge.writeIpuInput(bytes); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
void readEeIpuOutput(std::span<uint8_t,16> bytes) {
    std::lock_guard lock(bridgeMutex);
    try { bridge.readIpuOutput(bytes); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
bool takeEeIpuInterrupt() {
    std::lock_guard lock(bridgeMutex);
    try { return bridge.takeIpuInterrupt(); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
uint32_t takeEeIpuDmaCompletions() {
    std::lock_guard lock(bridgeMutex);
    try { return bridge.takeIpuDmaCompletions(); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}

void prepareCooperativeIop(const IopBootInputs& inputs) {
    std::lock_guard lock(bridgeMutex);
    return bridge.prepareCooperativeIop(inputs);
}
void activateCooperativeIop(std::string_view arguments, uint32_t mode) {
    std::lock_guard lock(bridgeMutex);
    return bridge.activateCooperativeIop(arguments, mode);
}
void beginFirmwareExperiment() {
    std::lock_guard lock(bridgeMutex);
    return bridge.beginFirmwareExperiment();
}
void finishFirmwareExperiment() {
    std::lock_guard lock(bridgeMutex);
    return bridge.finishFirmwareExperiment();
}
void enterGameAfterFirmware() {
    std::lock_guard lock(bridgeMutex);
    return bridge.enterGameAfterFirmware();
}
bool strictIopActive() {
    std::lock_guard lock(bridgeMutex);
    return bridge.strictIopActive();
}
bool firmwareGameEntryActive() {
    std::lock_guard lock(bridgeMutex);
    return bridge.firmwareGameEntryActive();
}
void bindFirmwareEeRam(std::span<uint8_t> ram, std::span<uint8_t> scratchpad,
                       uint32_t initialEnableWord, std::function<uint32_t()> dmaControl) {
    std::lock_guard lock(bridgeMutex);
    return bridge.bindFirmwareEeRam(ram, scratchpad, initialEnableWord, std::move(dmaControl));
}
void advanceFirmwareIop() {
    std::lock_guard lock(bridgeMutex);
    return bridge.advanceFirmwareIop();
}
bool advanceFirmwareIopWhileEeIdle() {
    std::lock_guard lock(bridgeMutex);
    return bridge.advanceFirmwareIopWhileEeIdle();
}
void advanceFirmwareIopForEeCycles(uint64_t cycles) {
    std::lock_guard lock(bridgeMutex);
    bridge.advanceFirmwareIopForEeCycles(cycles);
}
void firmwareIopVideoEvent(bool start) {
    std::lock_guard lock(bridgeMutex);
    bridge.videoEvent(start);
}
void publishVirtualPadInput(const VirtualPadInput& input) {
    std::lock_guard lock(bridgeMutex);
    bridge.publishPadInput(input);
}
bool takeFirmwareDmaCompletion() {
    std::lock_guard lock(bridgeMutex);
    return bridge.takeFirmwareDmaCompletion();
}
bool takeFirmwareSif0Completion() {
    std::lock_guard lock(bridgeMutex);
    return bridge.takeFirmwareSif0Completion();
}
std::optional<uint32_t> firmwareDmaRead(uint32_t address) {
    std::lock_guard lock(bridgeMutex);
    return bridge.firmwareDmaRead(address);
}
bool firmwareDmaWrite(uint32_t address, uint32_t value) {
    std::lock_guard lock(bridgeMutex);
    return bridge.firmwareDmaWrite(address, value);
}
std::optional<uint32_t> cooperativeSifRead(uint32_t address) {
    std::lock_guard lock(bridgeMutex);
    return bridge.cooperativeSifRead(address);
}
std::optional<uint32_t> cooperativeSifGetReg(uint32_t number) {
    std::lock_guard lock(bridgeMutex);
    return bridge.cooperativeSifGetReg(number);
}
std::optional<uint32_t> cooperativeSifSetReg(uint32_t number, uint32_t value) {
    std::lock_guard lock(bridgeMutex);
    return bridge.cooperativeSifSetReg(number, value);
}
bool cooperativeSifWrite(uint32_t address, uint32_t value) {
    std::lock_guard lock(bridgeMutex);
    return bridge.cooperativeSifWrite(address, value);
}
}
