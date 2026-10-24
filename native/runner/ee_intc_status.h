#pragma once
#include <cstdint>
#include <stdexcept>

namespace drakengard {
// EE executor events latch pending causes independently of handler masks.
// INTC_STAT writes acknowledge ones; a guest write can never raise a cause.
class EeIntcStatus {
    uint32_t pending{};
public:
    static constexpr uint32_t Address = 0x1000f000u;
    uint32_t read() const noexcept { return pending; }
    void acknowledge(uint32_t value) noexcept { pending &= ~value; }
    void raise(uint32_t cause) {
        if (cause >= 15u) throw std::runtime_error("Unsupported EE INTC cause");
        pending |= 1u << cause;
    }
};
}
