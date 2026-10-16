#include "ps2_runtime_macros.h"
#include "ps2_recompiled_functions.h"
#include <array>
#include <iostream>

int main() {
    R5900Context context{};
    R5900Context* ctx = &context;
    std::array<uint8_t, 16> memory{};
    context.r[31] = _mm_set_epi32(0, 0, 0, 0x2000);
    context.r[16] = _mm_set_epi32(0, 0, 0, 0x12345678);
    return42_0x1000(memory.data(), &context, nullptr);
    if (GPR_U32(ctx, 2) != 42 || context.pc != 0x2000 ||
        GPR_U32(ctx, 0) != 0 || GPR_U32(ctx, 16) != 0x12345678) {
        std::cerr << "Synthetic addiu/jr invariant failed\n";
        return 1;
    }
    std::cout << "Synthetic generated code executed: v0=42, pc=ra, zero/s0 preserved\n";
    return 0;
}
