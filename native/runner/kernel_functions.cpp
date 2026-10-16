#include "kernel_functions.h"
#include "runtime/ps2_memory.h"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <kernel_recompiled_functions.h>

namespace {
struct KernelEntry {
    uint32_t address;
    PS2Runtime::RecompiledFunction function;
};
struct KernelCode {
    uint32_t address;
    const uint8_t* bytes;
    size_t size;
};
struct KernelRegion { uint32_t start; uint32_t end; };
// Generated privately from the verified translation image, never committed.
#include <kernel_bindings.h>

PS2Runtime::RecompiledFunction lookupRaw(uint32_t address) {
    const auto* last = std::end(kernelEntries);
    const auto* entry = std::lower_bound(std::begin(kernelEntries), last, address,
        [](const KernelEntry& item, uint32_t pc) { return item.address < pc; });
    return entry != last && entry->address == address ? entry->function : nullptr;
}

void dispatchKernel(uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime) {
    verifyKernelInstructions(ram, ctx->pc);
    auto function = lookupRaw(ctx->pc);
    if (!function) throw std::runtime_error("Unknown kernel patch entry");
    function(ram, ctx, runtime);
}
}

void verifyKernelInstructions(const uint8_t* ram, uint32_t address) {
    if (!ram) throw std::runtime_error("Missing EE RAM for kernel verification");
    const auto* region = std::find_if(std::begin(kernelRegions), std::end(kernelRegions),
        [address](const KernelRegion& item) { return item.start <= address && address < item.end; });
    if (region == std::end(kernelRegions)) throw std::runtime_error("Unknown kernel patch region");
    // Require actual copied instructions. Data tables may legitimately change.
    // Any code mutation remains an explicit blocker until separately analyzed.
    for (const auto& code : kernelCode) {
        if (code.address < region->start || code.address >= region->end) continue;
        const uint32_t physical = code.address & 0x1FFFFFFFu;
        if (physical >= PS2_RAM_SIZE || code.size > PS2_RAM_SIZE - physical ||
            std::memcmp(ram + physical, code.bytes, code.size) != 0)
            throw std::runtime_error("Kernel patch instructions differ from verified translation");
    }
}

PS2Runtime::RecompiledFunction lookupKernelFunction(uint32_t address) {
    return lookupRaw(address) ? &dispatchKernel : nullptr;
}
