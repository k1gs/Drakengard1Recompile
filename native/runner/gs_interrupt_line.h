#pragma once
#include <cstdint>

// CSR interrupt flags 0..4 correspond to IMR mask bits 8..12. Capture an
// asserted source/unmask edge, retain it while the HLE INTC cause is disabled,
// and hand it to the executor once. Never manufactures a GS FINISH/SIGNAL bit.
class GsInterruptLine {
    uint32_t previous{};
    bool pending{};
public:
    bool observe(uint64_t csr,uint64_t imr,bool intcEnabled) {
        const uint32_t asserted=uint32_t(csr)&~uint32_t(imr>>8)&31;
        pending|=(asserted&~previous)!=0;
        previous=asserted;
        if (!intcEnabled || !pending) return false;
        pending=false; return true;
    }
};
