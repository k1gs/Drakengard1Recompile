#pragma once
#include <cstdint>
#include <stdexcept>

// Bounded functional scheduling, not instruction-exact PS2 timing. Both CPUs
// stay on the EE executor; a guest spin loop must not starve the IOP.
class CooperativeClock {
    uint64_t previous{}, debt{};
public:
    uint32_t charge(uint64_t eeCycles) {
        if (eeCycles<previous) throw std::runtime_error("Cooperative EE clock moved backwards");
        debt+=eeCycles-previous; previous=eeCycles;
        if (debt<32768) return 0;
        debt-=32768;
        return 4096;
    }
};
