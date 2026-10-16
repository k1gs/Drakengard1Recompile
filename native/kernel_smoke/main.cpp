#include "kernel_functions.h"
#include "runtime/ps2_memory.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    std::vector<uint8_t> ram(PS2_RAM_SIZE);
    R5900Context context{0x80001000u, 0};
    PS2Runtime runtime;
    auto function = lookupKernelFunction(context.pc);
    if (!function || lookupKernelFunction(context.pc + 4) || lookupKernelFunction(0x1000)) return 1;
    try { function(ram.data(), &context, &runtime); return 2; }
    catch (const std::runtime_error&) {}
    if (context.result != 0) return 3;
    const uint8_t code[] = {0x2a, 0, 2, 0x24, 8, 0, 0xe0, 3};
    std::memcpy(ram.data() + 0x1000, code, sizeof(code));
    function(ram.data(), &context, &runtime);
    if (context.result != 42) return 4;
    context.result = 0;
    ram[0x1000] ^= 1;
    try { function(ram.data(), &context, &runtime); return 5; }
    catch (const std::runtime_error&) {}
    if (context.result != 0) return 6;
    try { verifyKernelInstructions(ram.data(), 0x80003000u); return 7; }
    catch (const std::runtime_error&) {}
    try { verifyKernelInstructions(nullptr, 0x80001000u); return 8; }
    catch (const std::runtime_error&) {}
    std::cout << "Exact sparse entries and copied-instruction gate passed\n";
}
