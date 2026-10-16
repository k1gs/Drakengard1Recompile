#pragma once
#include "ps2_runtime.h"
PS2Runtime::RecompiledFunction lookupKernelFunction(uint32_t address);
void verifyKernelInstructions(const uint8_t* ram, uint32_t address);
