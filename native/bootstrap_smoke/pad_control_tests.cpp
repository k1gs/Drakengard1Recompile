#include "pad_control.h"
#include "../iop_probe/dualshock2.h"

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
    drakengard::PadControl analog;
    auto state=analog.sampleInput("D1PAD2 1 FFFF 100 7F7F7F00\n",10);
    check(state.buttons==0xffff && state.axes==std::array<uint8_t,4>{127,127,127,0});
    check(analog.sampleInput("D1PAD2 1 FFFF 100 7F7F7F00\n",109).axes[3]==0);
    check(analog.sampleInput("D1PAD2 1 FFFF 100 7F7F7F00\n",110).axes==drakengard::PadControlSample{}.axes);
    refuses([&] { analog.sampleInput("D1PAD2 1 FFFF 100 7F7F7FFF\n",111); });
    refuses([&] { analog.sampleInput("D1PAD2 2 FFFF 0 7F7F7F00\n",111); });
    refuses([&] { analog.sampleInput("D1PAD2 2 FFFF 100 7F7F7F0G\n",111); });
    refuses([&] { analog.sampleInput("D1PAD2 2 FFFF 100 7F7F7F\n",111); });
    refuses([&] { analog.sampleInput("D1PAD2 2 FFFF 100 7F7F7F00 extra\n",111); });
    refuses([&] { analog.sampleInput("D1PAD2 2 FFFF 100\n",111); });
    // The command's actual axis bytes survive the existing controller serial path.
    Dualshock2 serial; VirtualPadInput input; input.connected=true;
    serial.exchange(std::array<uint8_t,5>{1,0x43,0,1,0},input);
    serial.exchange(std::array<uint8_t,9>{1,0x44,0,1,3},input);
    serial.exchange(std::array<uint8_t,9>{1,0x43},input);
    drakengard::PadControl transport;
    const auto command=transport.sampleInput("D1PAD2 1 FFFF 100 00FF2A7F\n",0);
    input.buttons=command.buttons;input.axes=command.axes;
    auto reply=serial.exchange(std::array<uint8_t,9>{1,0x42},input);
    check(reply[1]==0x73 && reply[5]==0 && reply[6]==255 && reply[7]==42 && reply[8]==127);
    input.axes=transport.sampleInput("D1PAD2 1 FFFF 100 00FF2A7F\n",100).axes;
    reply=serial.exchange(std::array<uint8_t,9>{1,0x42},input);
    check(reply[5]==127 && reply[6]==127 && reply[7]==127 && reply[8]==127);
    // Switching back to the original button protocol centers all axes.
    check(analog.sampleInput("D1PAD1 2 FFFF 0\n",111).axes==drakengard::PadControlSample{}.axes);
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
