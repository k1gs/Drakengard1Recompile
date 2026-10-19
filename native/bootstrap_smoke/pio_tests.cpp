#include "pio_write_capture.h"
#include <map>

namespace {
void check(bool value) { if (!value) throw std::runtime_error("PIO capture contract check failed"); }
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Unreviewed PIO capture operation accepted");
}
struct Bus {
    std::map<uint32_t, uint32_t> words{{0x1f801004, 0x1f802000}, {0x1f80101c, 0xd2077}};
    uint32_t read32(uint32_t address) const {
        auto it = words.find(address); return it == words.end() ? 0 : it->second;
    }
    uint8_t read8(uint32_t address) const { return uint8_t(read32(address)); }
};
}
void checkPioCapture() {
    Bus bus; PioWriteCapture capture;
    check(!capture.allows(bus, 0x1f802070, 0x2b, 0xfc));
    capture.enable(); refuses([&] { capture.enable(); });
    for (auto opcode : {0x20u, 0x21u, 0x23u, 0x24u, 0x25u, 0x29u, 0x2eu})
        check(!capture.allows(bus, 0x1f802070, opcode, 0xfc));
    check(!capture.allows(bus, 0x1f802071, 0x28, 1));
    check(!capture.allows(bus, 0x1f802070, 0x2b, 0xfd));
    refuses([&] { capture.afterWrite(bus, 0x2b, 0xfc, 0x1000); });
    check(!capture.writes());
    bus.words[0x1f802070] = 0xfc;
    capture.afterWrite(bus, 0x2b, 0xfc, 0x1000);
    bus.words[0x1f802070] = 0x123456a5;
    capture.afterWrite(bus, 0x28, 0xcafebaa5, 0x1004);
    check(capture.writes() == 2 && capture.value() == 0xa5 && capture.width() == 1);
    bus.words[0x1f801004] = 0x1f803000;
    check(!capture.allows(bus, 0x1f802070, 0x28, 1));
    bus.words[0x1f801004] = 0x1f802000; bus.words[0x1f80101c] = 0xff;
    refuses([&] { capture.afterWrite(bus, 0x28, 0xa5, 0x1008); });
    check(capture.writes() == 2);
}
