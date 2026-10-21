#include "quiescent_sio2.h"
#include <initializer_list>

namespace {
void check(bool value) { if (!value) throw std::runtime_error("SIO2 quiescent control check failed"); }
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid SIO2 control operation accepted");
}
struct Bus {
    uint32_t word{};
    uint32_t read32(uint32_t) const { return word; }
};
}
void checkSio2Control() {
    QuiescentSio2 sio2; Bus memory;
    check(!sio2.allows(0x1f808268, 0x2b, 0x3bc));
    sio2.enable();
    check(!sio2.allows(0x1f808268, 0x23, 0));
    refuses([&] { sio2.afterWrite(memory); });
    check(!sio2.allows(0x1f808268, 0x23, 0));
    memory.word = 0x3bc; sio2.afterWrite(memory);
    check(sio2.allows(0x1f808268, 0x23, 0));
    for (auto value : {0u, 1u, 0x3bdu, 0xffffffffu}) check(!sio2.allows(0x1f808268, 0x2b, value));
    for (auto opcode : {0x20u, 0x21u, 0x25u, 0x28u, 0x29u, 0x2eu})
        check(!sio2.allows(0x1f808268, opcode, 0x3bc));
    for (auto address : {0x1f808260u, 0x1f808264u, 0x1f80826cu, 0x1f808280u})
        check(!sio2.allows(address, 0x23, 0) && !sio2.allows(address, 0x2b, 0x3bc));
    refuses([&] { sio2.enable(); });
}
