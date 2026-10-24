#include "pad_control.h"

#include <cstdint>
#include <stdexcept>

namespace {
void check(bool value) { if (!value) throw std::runtime_error("pad control regression failed"); }
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("invalid pad command was accepted");
}
}

void checkPadControl() {
    drakengard::PadControl pad;
    check(pad.sample("D1PAD1 0 FFFF 0\n", 0) == 0xffff);
    check(pad.sample("D1PAD1 1 FFFE 100\n", 1000) == 0xfffe);
    check(pad.sample("D1PAD1 1 FFFE 100\n", 1050) == 0xfffe);
    check(pad.sample("D1PAD1 1 FFFE 100\n", 1100) == 0xffff);

    // A duplicate never renews the prior deadline; a changed payload cannot
    // mutate an already accepted sequence.
    drakengard::PadControl duplicate;
    check(duplicate.sample("D1PAD1 4 1234 100\n", 10) == 0x1234);
    check(duplicate.sample("D1PAD1 4 1234 100\n", 90) == 0x1234);
    check(duplicate.sample("D1PAD1 4 1234 100\n", 110) == 0xffff);
    refuses([&] { duplicate.sample("D1PAD1 4 1235 100\n", 111); });
    refuses([&] { duplicate.sample("D1PAD1 4 1234 99\n", 111); });
    refuses([&] { duplicate.sample("D1PAD1 3 1234 100\n", 112); });
    refuses([&] { duplicate.sample("D1PAD1 5 1234 0\n", 113); });

    refuses([&] { duplicate.sample("D1PAD1 6 1234 1\n", 100); });
    refuses([&] { duplicate.sample("D1PAD1 7 1234 1", 120); });
    refuses([&] { duplicate.sample("D1PAD1 8 1234 1\nextra\n", 120); });
    refuses([&] { duplicate.sample("D1PAD1 9 1234 10001\n", 120); });
    refuses([&] { duplicate.sample("D1PAD1 10 123 1\n", 120); });
    refuses([&] { duplicate.sample("D1PAD1 11 12G4 1\n", 120); });
    refuses([&] { duplicate.sample("D1PAD1 18446744073709551616 1234 1\n", 120); });
    refuses([&] { duplicate.sample("D1PAD1 12 1234 1 2\n", 120); });
    refuses([&] { duplicate.sample("D1PAD1 13 1234 0\n", 120); });
}

int main() { checkPadControl(); }
