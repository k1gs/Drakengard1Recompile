#include "iop_timers.h"
#include <stdexcept>

namespace drakengard {
namespace {
constexpr uint32_t TargetFlag = 0x800, OverflowFlag = 0x1000, InterruptEnable = 0x400;
constexpr unsigned Irqs[] = {4, 5, 6, 14, 15, 16};
}

unsigned IopTimers::index(uint32_t address) {
    if (address >= 0x1f801100 && address < 0x1f801130) return (address - 0x1f801100) / 16;
    if (address >= 0x1f801480 && address < 0x1f8014b0) return 3 + (address - 0x1f801480) / 16;
    return 6;
}

bool IopTimers::supports(uint32_t address, unsigned bytes, bool write) const {
    const auto timer = index(address), offset = address & 15u;
    if (timer >= 6 || (bytes != 2 && bytes != 4) || address % bytes) return false;
    if (offset == 0 || offset == 4 || offset == 8) return true;
    return !write && timer >= 3 && bytes == 2 && (offset == 2 || offset == 10);
}

uint32_t IopTimers::divider(unsigned timer, uint32_t mode) {
    if (timer == 2) return (mode & 0x200) ? 8 : 1;
    if (timer >= 4) {
        constexpr uint32_t divisors[] = {1, 8, 16, 256};
        return divisors[(mode >> 13) & 3];
    }
    return 1;
}

uint64_t IopTimers::modulus(unsigned timer) { return timer < 3 ? 0x10000ull : 0x100000000ull; }

void IopTimers::rearm(Timer& timer) {
    timer.mode = (timer.mode & ~(TargetFlag | OverflowFlag)) | InterruptEnable;
    timer.irqRepeat = (timer.mode & 0x40) != 0;
    timer.irqToggle = (timer.mode & 0x80) != 0;
}

uint32_t IopTimers::read(uint32_t address, unsigned bytes) {
    if (!supports(address, bytes, false)) throw std::runtime_error("Unsupported IOP timer read register/width");
    auto& timer = m_timers[index(address)];
    uint32_t value = 0;
    switch (address & 15) {
    case 0: value = static_cast<uint32_t>(timer.count); break;
    case 2: value = static_cast<uint32_t>(timer.count >> 16); break;
    case 4: value = timer.mode; rearm(timer); break;
    case 8: value = timer.target; break;
    case 10: value = timer.target >> 16; break;
    }
    return bytes == 2 ? value & 0xffffu : value;
}

void IopTimers::write(uint32_t address, unsigned bytes, uint32_t value) {
    if (!supports(address, bytes, true)) throw std::runtime_error("Unsupported IOP timer write register/width");
    const unsigned number = index(address);
    auto& timer = m_timers[number];
    if (bytes == 2) value &= 0xffffu;
    const uint32_t mask = number < 3 ? 0xffffu : 0xffffffffu;
    switch (address & 15) {
    case 0:
        timer.count = value & mask;
        timer.armed = timer.count <= timer.target;
        break;
    case 4: {
        const uint32_t nextMode = value & 0x63ffu;
        if ((nextMode & 0x100u) && number != 2 && number < 4)
            throw std::runtime_error("External pixel/HBlank timer source requires actual EE blank events");
        if ((nextMode & 1) && (number == 0 || number == 1 || number == 3))
            throw std::runtime_error("IOP timer gating requires actual EE blank events");
        const bool pending = ((timer.mode & TargetFlag) && (timer.mode & 0x10)) ||
                             ((timer.mode & OverflowFlag) && (timer.mode & 0x20));
        timer.mode = nextMode | (timer.mode & 0x1c00u);
        if (!pending) rearm(timer);
        timer.count = 0; timer.armed = true;
        timer.fraction = static_cast<uint32_t>(m_cycles % divider(number, timer.mode));
        break;
    }
    case 8:
        timer.target = value & mask;
        timer.armed = timer.target > timer.count;
        break;
    }
}

bool IopTimers::fire(Timer& timer, bool overflow) {
    const uint32_t flag = overflow ? OverflowFlag : TargetFlag;
    bool triggered = false, update = timer.irqRepeat;
    if (timer.mode & InterruptEnable) {
        triggered = timer.irqRepeat || !(timer.mode & flag);
        update |= timer.irqToggle;
    }
    if (update) {
        if (timer.irqToggle) timer.mode ^= InterruptEnable;
        else timer.mode &= ~InterruptEnable;
    }
    return triggered;
}

uint32_t IopTimers::advance(uint32_t cycles) {
    if (cycles > 1000000) throw std::runtime_error("IOP timer cycle step exceeds bounded model input");
    uint32_t events = 0;
    for (uint32_t step = 0; step < cycles; ++step) {
        ++m_cycles;
        for (unsigned number = 0; number < m_timers.size(); ++number) {
            auto& timer = m_timers[number];
            if (timer.irqRepeat && !timer.irqToggle) timer.mode |= InterruptEnable;
            if ((timer.mode & 1) && !(((timer.mode >> 1) & 3) & 1)) {
                timer.fraction = static_cast<uint32_t>(m_cycles % divider(number, timer.mode));
                if (((timer.mode >> 1) & 3) == 2) timer.count = 0;
                continue;
            }
            if (++timer.fraction < divider(number, timer.mode)) continue;
            timer.fraction = 0;
            ++timer.count;
            if (timer.armed && timer.count >= timer.target) {
                if ((timer.mode & 0x10) && fire(timer, false)) events |= 1u << Irqs[number];
                timer.mode |= TargetFlag;
                if (timer.mode & 8) timer.count -= timer.target;
                else timer.armed = false;
            }
            if (timer.count >= modulus(number)) {
                if ((timer.mode & 0x20) && fire(timer, true)) events |= 1u << Irqs[number];
                timer.mode |= OverflowFlag;
                timer.count -= modulus(number); timer.armed = true;
            }
        }
    }
    return events;
}
}
