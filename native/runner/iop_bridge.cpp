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
std::recursive_mutex mutex;
std::unique_ptr<IopBoot> boot;
bool active{};
bool firmwareExperiment{};
EeSifDma firmwareDma;
unsigned reads{};
std::array<uint32_t, 3> softwareRegisters{};
uint32_t busRegister(uint32_t number) {
    return SifRegisters::kernelAddress(number);
}
bool busAddress(uint32_t address) { return address >= 0x1000f200 && address <= 0x1000f260; }
}
void prepareCooperativeIop(const IopBootInputs& inputs) {
    std::lock_guard lock(mutex);
    if (boot) throw std::runtime_error("Cooperative IOP already configured");
    boot = std::make_unique<IopBoot>(inputs);
}
void activateCooperativeIop(std::string_view arguments, uint32_t mode) {
    std::lock_guard lock(mutex);
    if (!boot) throw std::runtime_error("IOP reboot requires configured verified bootstrap inputs");
    if (active || mode || arguments != "rom0:UDNL cdrom0:\\MODULES\\IOPRP280.IMG;1")
        throw std::runtime_error("Unsupported repeated/different IOP reboot request");
    boot->advance(4096); // Actually execute SYSMEM before accepting this request.
    active = true;
    std::cerr << "[cooperative-iop] reboot accepted; real CPU active; no BOOTEND injected\n";
}
void beginFirmwareExperiment() {
    std::lock_guard lock(mutex);
    if (!boot || active) throw std::runtime_error("Firmware experiment requires a fresh configured strict IOP");
    boot->advance(4096);
    active = true; firmwareExperiment = true;
    std::cerr << "[firmware-experiment] selected IOP boot plan active; bounded functional scheduling, not a full BIOS reset\n";
}
std::optional<uint32_t> firmwareDmaRead(uint32_t address) {
    std::lock_guard lock(mutex);
    if (!firmwareExperiment || !EeSifDma::contains(address)) return std::nullopt;
    try { return firmwareDma.read(address); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
bool firmwareDmaWrite(uint32_t address, uint32_t value) {
    std::lock_guard lock(mutex);
    if (!firmwareExperiment || !EeSifDma::contains(address)) return false;
    try { firmwareDma.write(address, value); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
    std::cerr << "[firmware-ee-dma] address=0x" << std::hex << address << " value=0x" << value << std::dec << '\n';
    return true;
}
std::optional<uint32_t> cooperativeSifRead(uint32_t address) {
    std::lock_guard lock(mutex);
    if (!active || !busAddress(address)) return std::nullopt;
    boot->advance(4096);
    const auto value = boot->readEe(address);
    if (++reads <= 16 || reads % 256 == 0)
        std::cerr << "[cooperative-sif-read] address=0x" << std::hex << address << " value=0x" << value << std::dec << '\n';
    return value;
}
std::optional<uint32_t> cooperativeSifGetReg(uint32_t number) {
    std::lock_guard lock(mutex);
    if (!active) return std::nullopt;
    boot->advance(4096);
    const auto value = number >= 0x80000000u && number <= 0x80000002u ?
        softwareRegisters[number - 0x80000000u] : boot->readEe(busRegister(number));
    if (++reads <= 16 || reads % 256 == 0)
        std::cerr << "[cooperative-sif-getreg] number=0x" << std::hex << number << " value=0x" << value << std::dec << '\n';
    return value;
}
std::optional<uint32_t> cooperativeSifSetReg(uint32_t number, uint32_t value) {
    std::lock_guard lock(mutex);
    if (number >= 0x80000000u && number <= 0x80000002u) {
        auto& slot = softwareRegisters[number - 0x80000000u];
        const auto previous = slot; slot = value;
        if (!active) return std::nullopt; // Observe real pre-reset writes; original kernel path still runs.
        std::cerr << "[cooperative-sif-sysreg] number=0x" << std::hex << number << " value=0x" << value << std::dec << '\n';
        boot->advance(4096);
        return previous;
    }
    if (!active) {
        std::cerr << "[observed-ee-sif-setreg] number=0x" << std::hex << number << " value=0x" << value << std::dec << '\n';
        if (boot && (number == 1 || number == 3 || number == 4)) boot->writeEe(busRegister(number), value);
        return std::nullopt;
    }
    const auto address = busRegister(number), previous = boot->readEe(address);
    cooperativeSifWrite(address, value);
    return previous;
}
bool cooperativeSifWrite(uint32_t address, uint32_t value) {
    std::lock_guard lock(mutex);
    if (!active || !busAddress(address)) return false;
    std::cerr << "[cooperative-sif-write] address=0x" << std::hex << address << " value=0x" << value << std::dec << '\n';
    boot->writeEe(address, value);
    boot->advance(4096);
    return true;
}
}
