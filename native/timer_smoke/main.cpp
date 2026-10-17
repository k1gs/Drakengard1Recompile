#include "iop_timers.h"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool result) { if (!result) throw std::runtime_error("Synthetic IOP timer contract failed"); }
template<class Action> void rejects(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unsupported timer operation accepted");
}
}
int main() {
    try {
        constexpr uint32_t Timer5 = 0x1f8014a0, Timer0 = 0x1f801100;
        drakengard::IopTimers timers;
        timers.advance(16); check(timers.read(Timer5, 4) == 16);
        timers.write(Timer5 + 4, 2, 0x2000); timers.advance(7);
        check(timers.read(Timer5, 4) == 0); timers.advance(1); check(timers.read(Timer5, 4) == 1);
        timers.write(Timer5 + 8, 4, 3); timers.write(Timer5 + 4, 2, 0x58);
        check(timers.advance(2) == 0 && timers.read(Timer5, 4) == 2);
        check(timers.advance(1) == (1u << 16) && timers.read(Timer5, 4) == 0);
        const auto mode = timers.read(Timer5 + 4, 2);
        check((mode & 0x800) && !(timers.read(Timer5 + 4, 2) & 0x1800));
        check(timers.advance(3) == (1u << 16));
        timers.read(Timer5 + 4, 2); timers.write(Timer5 + 4, 2, 0x10);
        check(timers.advance(3) == (1u << 16)); check(timers.advance(10) == 0);
        timers.write(Timer0 + 4, 2, 0x20); timers.write(Timer0, 2, 0xfffe);
        check(timers.advance(1) == 0); check(timers.advance(1) == (1u << 4));
        check(timers.read(Timer0, 2) == 0 && (timers.read(Timer0 + 4, 2) & 0x1000));
        timers.write(Timer5, 4, 0x12345678);
        check(timers.read(Timer5 + 2, 2) == 0x1234 && timers.read(Timer5, 2) == 0x5678);
        rejects([&] { timers.write(Timer0 + 4, 2, 0x100); });
        rejects([&] { timers.write(Timer5 + 2, 2, 7); });
        rejects([&] { timers.read(Timer5 + 4, 1); });
        rejects([&] { timers.advance(1000001); });
        std::cout << "Internal timer count, prescale, target/overflow IRQ, flags and bounds passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
