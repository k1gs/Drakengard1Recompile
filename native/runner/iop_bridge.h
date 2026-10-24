#pragma once
#include "iop_boot.h"
#include <optional>
#include <span>
#include <string_view>

namespace drakengard {
void prepareCooperativeIop(const IopBootInputs& inputs);
void activateCooperativeIop(std::string_view arguments, uint32_t mode);
void beginFirmwareExperiment();
void finishFirmwareExperiment();
void enterGameAfterFirmware();
bool strictIopActive();
bool firmwareGameEntryActive();
void bindFirmwareEeRam(std::span<uint8_t> ram, std::span<uint8_t> scratchpad,
                       uint32_t initialEnableWord, std::function<uint32_t()> dmaControl);
void advanceFirmwareIop();
bool advanceFirmwareIopWhileEeIdle();
void advanceFirmwareIopForEeCycles(uint64_t cycles);
void firmwareIopVideoEvent(bool start);
void publishVirtualPadInput(const VirtualPadInput& input);
bool takeFirmwareDmaCompletion();
bool takeFirmwareSif0Completion();
std::optional<uint32_t> firmwareDmaRead(uint32_t address);
bool firmwareDmaWrite(uint32_t address, uint32_t value);
std::optional<uint32_t> eeIntcRead(uint32_t address);
bool eeIntcWrite(uint32_t address, uint32_t value);
void latchEeIntcCause(uint32_t cause);
std::optional<uint32_t> eeIpuRead(uint32_t address);
std::optional<uint64_t> eeIpuRead64(uint32_t address);
bool eeIpuWrite(uint32_t address,uint32_t value);
bool eeIpuWrite64(uint32_t address,uint64_t value);
void writeEeIpuInput(std::span<const uint8_t,16> bytes);
void readEeIpuOutput(std::span<uint8_t,16> bytes);
bool takeEeIpuInterrupt();
uint32_t takeEeIpuDmaCompletions();
std::optional<uint32_t> cooperativeSifGetReg(uint32_t number);
std::optional<uint32_t> cooperativeSifSetReg(uint32_t number, uint32_t value);
std::optional<uint32_t> cooperativeSifRead(uint32_t address);
bool cooperativeSifWrite(uint32_t address, uint32_t value);
}
