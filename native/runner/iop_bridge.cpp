#include "iop_bridge.h"
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <array>
#include "../iop_probe/sif_registers.h"
#include "../iop_probe/ee_sif_dma.h"
#include "strict_device_error.h"

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
    std::span<uint8_t> eeRam;
    unsigned reads{};
    std::array<uint32_t, 3> softwareRegisters{};
    uint32_t busRegister(uint32_t number) {
        return SifRegisters::kernelAddress(number);
    }
    bool busAddress(uint32_t address) { return address >= 0x1000f200 && address <= 0x1000f260; }
public:
    bool firmwareGameEntryActive() const { return phase == Phase::FirmwareEntry; }
    void bindFirmwareEeRam(std::span<uint8_t> ram, uint32_t initialEnableWord) {
        if (phase != Phase::Prepared || !eeRam.empty() || ram.size() != 32 * 1024 * 1024)
            throw StrictDeviceError("Firmware EE memory binding requires one fresh 32-MiB runtime");
        eeRam = ram;
        firmwareDma.seedEnableWord(initialEnableWord);
    }
    bool takeFirmwareDmaCompletion() { return firmwareDma.takeCompletion(); }
    void advanceFirmwareIop() {
        if (phase != Phase::FirmwareEntry) throw StrictDeviceError("Firmware DMA call requires experimental game entry");
        boot->advance(4096);
    }
    bool strictIopActive() const { return active(); }
    void enterGameAfterFirmware() {
        if (phase != Phase::Firmware || eeRam.empty() || !(boot->readEe(0x1000f230) & 0x40000u))
            throw StrictDeviceError("Game entry requires actual completed firmware IOP startup");
        phase = Phase::FirmwareEntry;
        firmwareDma.enableSif1();
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
void bindFirmwareEeRam(std::span<uint8_t> ram, uint32_t initialEnableWord) {
    std::lock_guard lock(bridgeMutex);
    return bridge.bindFirmwareEeRam(ram, initialEnableWord);
}
void advanceFirmwareIop() {
    std::lock_guard lock(bridgeMutex);
    return bridge.advanceFirmwareIop();
}
bool takeFirmwareDmaCompletion() {
    std::lock_guard lock(bridgeMutex);
    return bridge.takeFirmwareDmaCompletion();
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
