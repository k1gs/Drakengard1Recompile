#pragma once
#include <cstdint>
struct BranchInput { uint64_t low; };
bool branchNegative(BranchInput*);
bool branchNonpositive(BranchInput*);
bool branchPositive(BranchInput*);
bool branchNonnegative(BranchInput*);
