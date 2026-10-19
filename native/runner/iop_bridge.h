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
void bindFirmwareEeRam(std::span<uint8_t> ram, uint32_t initialEnableWord);
void advanceFirmwareIop();
bool takeFirmwareDmaCompletion();
std::optional<uint32_t> firmwareDmaRead(uint32_t address);
bool firmwareDmaWrite(uint32_t address, uint32_t value);
std::optional<uint32_t> cooperativeSifGetReg(uint32_t number);
std::optional<uint32_t> cooperativeSifSetReg(uint32_t number, uint32_t value);
std::optional<uint32_t> cooperativeSifRead(uint32_t address);
bool cooperativeSifWrite(uint32_t address, uint32_t value);
}
