#pragma once
#include "ps2_runtime.h"
PS2Runtime::RecompiledFunction lookupFirmwareFunction(uint32_t address);
void verifyFirmwareInstructions(const uint8_t* ram, uint32_t address);
void runFirmwareInitialization(PS2Runtime& runtime, const char* verifiedLocalRom);
bool invokeFirmwareSifSyscall(uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime, uint32_t number);
