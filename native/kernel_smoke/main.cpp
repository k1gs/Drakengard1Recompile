#include "kernel_functions.h"
#include "sparse_dispatch.h"
#include "runtime/ps2_memory.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    struct Entry { uint32_t address; };
    const Entry singleton[] = {{0xffffffffu}};
    if (drakengard::findExactSparseEntry(singleton, 0xffffffffu) != &singleton[0] ||
        drakengard::findExactSparseEntry(singleton, 0xfffffffeu) ||
        drakengard::findExactSparseEntry(singleton, 0u)) return 14;
    const Entry extremes[] = {{0u}, {4u}, {0xfffffffcu}};
    if (drakengard::findExactSparseEntry(extremes, 0u) != &extremes[0] ||
        drakengard::findExactSparseEntry(extremes, 4u) != &extremes[1] ||
        drakengard::findExactSparseEntry(extremes, 0xfffffffcu) != &extremes[2] ||
        drakengard::findExactSparseEntry(extremes, 1u) ||
        drakengard::findExactSparseEntry(extremes, 0x80000000u) ||
        drakengard::findExactSparseEntry(extremes, 0xffffffffu)) return 15;
    std::vector<uint8_t> ram(PS2_RAM_SIZE);
    R5900Context context{0x80001000u, 0};
    PS2Runtime runtime;
    // Exact sparse hits, holes, unaligned PCs and both inclusive bounds must
    // retain their meaning when common out-of-range misses are rejected early.
    for (uint32_t pc = 0x80000ff0u; pc <= 0x80002010u; ++pc) {
        const bool expected = pc == 0x80001000u || pc == 0x80001020u || pc == 0x80002000u;
        if ((lookupKernelFunction(pc) != nullptr) != expected) return 9;
    }
    for (uint32_t pc : {0u, 0x1000u, 0x7fffffffu, 0x80000000u,
                        0x80002004u, 0xa0001000u, 0xffffffffu})
        if (lookupKernelFunction(pc)) return 10;
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
    // The last entry must still verify copied code on every dispatch, including
    // after a successful call; the lookup shortcut cannot cache verification.
    context.pc = 0x80002000u;
    auto last = lookupKernelFunction(context.pc);
    std::memcpy(ram.data() + 0x2000, code, sizeof(code));
    last(ram.data(), &context, &runtime);
    if (context.result != 42) return 11;
    context.result = 0;
    ram[0x2000] ^= 1;
    try { last(ram.data(), &context, &runtime); return 12; }
    catch (const std::runtime_error&) {}
    if (context.result != 0) return 13;
    try { verifyKernelInstructions(ram.data(), 0x80003000u); return 7; }
    catch (const std::runtime_error&) {}
    try { verifyKernelInstructions(nullptr, 0x80001000u); return 8; }
    catch (const std::runtime_error&) {}
    std::cout << "Exact sparse entries and copied-instruction gate passed\n";
}
