#pragma once
#include <array>
#include <cstdint>

// Fixed functional bus profile, reviewed against the verified reboot path and
// PCSX2 v2.8.2 IopHw.h. These are configuration latches, not device ports.
// Retaining a delay does not simulate cycles; retaining a base never opens it.
namespace ssbus {
struct Setting {
    uint32_t address;
    std::array<uint32_t, 3> writes; // Zero entries are unused; zero is not an accepted profile.
};
inline constexpr std::array settings{
    Setting{0x1f801004, {0x1f802000, 0x1f802000}}, // PIO base
    Setting{0x1f80100c, {0x00183444, 0xff, 0x0018344f}},       // DEV1 delay
    Setting{0x1f801010, {0x0016244f, 0x0016244f}}, // ROM delay
    Setting{0x1f801014, {0x200b31e1, 0xff}},       // SPU delay
    Setting{0x1f801018, {0x6f060011, 0xff}},       // DEV5 delay
    Setting{0x1f80101c, {0x000d2077, 0xff}},       // PIO delay
    Setting{0x1f801020, {0x00001225, 0x00001225}}, // common delay
    Setting{0x1f801400, {0x1e000000, 0xbe000000}}, // DEV1 base and its fixed KSEG1 alias
    Setting{0x1f801404, {0x1f801da8, 0x1f801da8}}, // SPU base
    Setting{0x1f801408, {0x1f402000, 0x1f402000}}, // DEV5 base
    Setting{0x1f80140c, {0x1f400010, 0x1f400010}}, // SPU1 base
    Setting{0x1f801410, {0x14000000, 0x14000000}}, // DEV9 base 3
    Setting{0x1f801414, {0x200931e1, 0xff}},       // SPU1 delay
    Setting{0x1f801418, {0xff, 0xff}},             // DEV9 delay 2
    Setting{0x1f80141c, {0x001a1055, 0xff}},       // DEV9 delay 3
    Setting{0x1f801420, {0x000510ff, 0x000510ff}}, // DEV9 delay 1
};
inline bool allows(uint32_t address, uint32_t opcode, uint32_t value) {
    if (opcode != 0x23 && opcode != 0x2b) return false;
    for (const auto& setting : settings)
        if (address == setting.address) {
            if (opcode == 0x23) return true;
            for (auto accepted : setting.writes)
                if (accepted && value == accepted) return true;
            return false;
        }
    return false;
}
}
