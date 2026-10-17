#pragma once
#include <array>
#include <cstdint>

namespace drakengard {
// Deterministic internal-clock subset. The caller supplies modeled IOP cycles;
// no host clock, guessed H/V blank pulses or constant counter replies are used.
class IopTimers {
public:
    bool supports(uint32_t address, unsigned bytes, bool write) const;
    uint32_t read(uint32_t address, unsigned bytes);
    void write(uint32_t address, unsigned bytes, uint32_t value);
    uint32_t advance(uint32_t cycles); // Actual event bits for IOP I_STAT.
    uint64_t cycles() const { return m_cycles; }
private:
    struct Timer {
        uint64_t count{};
        uint32_t target{}, mode{0x400}, fraction{};
        bool armed{}, irqRepeat{}, irqToggle{};
    };
    static unsigned index(uint32_t address);
    static uint32_t divider(unsigned index, uint32_t mode);
    static uint64_t modulus(unsigned index);
    static void rearm(Timer& timer);
    static bool fire(Timer& timer, bool overflow);
    std::array<Timer, 6> m_timers{};
    uint64_t m_cycles{};
};
}
