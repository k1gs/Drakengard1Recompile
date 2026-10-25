#pragma once
#include <cstdint>
#include <stdexcept>

// Initial boot, at most one guest reboot, then one post-reboot runtime phase. Counters
// stay monotonic; an idle loop or device poll cannot renew the deadline.
class IopExecutionBudget {
    uint32_t begin{}, end{PhaseLimit};
    bool rebootAllowed{}, rebootStarted{}, runtimeStarted{};
    uint32_t runtimeLimit{RuntimeLimit};
    bool runtimeConfigured{};
public:
    static constexpr uint32_t PhaseLimit = 8000000;
    // Post-boot guest services now execute continuously during EE spin loops.
    // Keep a finite diagnostic run allowance; idle loops cannot renew it.
    static constexpr uint32_t RuntimeLimit = 256000000;
    static constexpr uint32_t MaxRuntimeLimit = 2048000000;
    static_assert(MaxRuntimeLimit <= UINT32_MAX - 2u * PhaseLimit);
    uint32_t deadline() const { return end; }
    bool inReboot() const { return rebootStarted && !runtimeStarted; }
    bool inRuntime() const { return runtimeStarted; }
    void configureRuntimeLimit(uint32_t limit) {
        if (runtimeConfigured || rebootStarted || runtimeStarted)
            throw std::runtime_error("IOP runtime budget requires one configuration before reboot entry");
        if (limit != RuntimeLimit && limit != 512000000 && limit != 1024000000 && limit != MaxRuntimeLimit)
            throw std::runtime_error("Unsupported finite IOP runtime budget");
        runtimeLimit = limit;
        runtimeConfigured=true;
    }
    void enableGuestReboot() {
        if (rebootAllowed) throw std::runtime_error("IOP reboot budget already enabled");
        rebootAllowed = true;
    }
    // Called only after a successfully retired, verified original IOPBOOT entry.
    bool retiredRebootEntry(uint32_t retired) {
        if (!rebootAllowed) return false;
        if (rebootStarted || !retired || retired > PhaseLimit)
            throw std::runtime_error("IOP reboot budget cannot be renewed");
        rebootStarted = true;
        begin = retired - 1;
        end = retired - 1 + PhaseLimit;
        return true;
    }
    // Only a new, successfully retired guest BOOTEND after the reboot may enter runtime.
    void retiredRebootCompletion(uint32_t retired) {
        if (!rebootStarted || runtimeStarted || retired <= begin || retired > end)
            throw std::runtime_error("IOP runtime budget requires a single completed guest reboot");
        runtimeStarted = true;
        end = retired + runtimeLimit;
    }
};
