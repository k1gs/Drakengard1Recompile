#include "signed_branch_fixture.h"
#include <bit>
#define GPR_S32(ctx, reg) std::bit_cast<int32_t>(static_cast<uint32_t>((ctx)->low))
#define GPR_S64(ctx, reg) std::bit_cast<int64_t>((ctx)->low)
bool branchNegative(BranchInput* ctx) {
    const bool branch_taken_0x1000 = (GPR_S32(ctx, 4) < 0);
    return branch_taken_0x1000;
}
bool branchNonpositive(BranchInput* ctx) {
    const bool branch_taken_0x1004 = (GPR_S32(ctx, 4) <= 0);
    return branch_taken_0x1004;
}
bool branchPositive(BranchInput* ctx) {
    const bool branch_taken_0x1008 = (GPR_S32(ctx, 4) > 0);
    return branch_taken_0x1008;
}
bool branchNonnegative(BranchInput* ctx) {
    const bool branch_taken_0x100c = (GPR_S32(ctx, 4) >= 0);
    return branch_taken_0x100c;
}
