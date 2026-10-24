#include "signed_branch_fixture.h"
#include <array>
#include <iostream>
int main() {
    struct Case { uint64_t value; bool negative; bool zero; };
    constexpr std::array cases{
        Case{0,false,true}, Case{1,false,false},
        Case{0x80000000ull,false,false}, Case{0xffffffffull,false,false},
        Case{0x100000000ull,false,false}, Case{0x7fffffffffffffffull,false,false},
        Case{0x8000000000000000ull,true,false}, Case{0xffffffff00000000ull,true,false},
        Case{0xffffffffffffffffull,true,false}};
    for (auto row:cases) {
        BranchInput input{row.value};
        if (branchNegative(&input)!=row.negative ||
            branchNonpositive(&input)!=(row.negative || row.zero) ||
            branchPositive(&input)!=( !row.negative && !row.zero) ||
            branchNonnegative(&input)!=!row.negative) {
            std::cerr << "Signed EE branch used wrong width for0x" << std::hex << row.value << '\n';
            return 1;
        }
    }
    std::cout << "Signed EE branches preserve bit63, positive bit31, and zero boundaries\n";
}
