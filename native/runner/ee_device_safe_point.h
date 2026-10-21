#pragma once
#include "iop_bridge.h"
#include "ee_failure_diagnostic.h"
#include "ee_progress_diagnostic.h"
#include "gs_interrupt_line.h"
#include "runtime/ee_scheduler.h"
#include <array>

class EeDeviceSafePoint {
    GsInterruptLine gsInterrupt;
    EeProgressDiagnostic progress;
    unsigned gsInterrupts{};
public:
    void advance(EeScheduler& scheduler,PS2Runtime& runtime,uint64_t cycle,
                 bool gsCauseEnabled,const R5900Context* mainContext) {
        progress.observe(runtime,mainContext,cycle);
        try { drakengard::advanceFirmwareIopForEeCycles(cycle); }
        catch (...) {
            diagnoseEeDeviceFailure(mainContext,runtime.memory().getRDRAM());
            throw;
        }
        for (uint32_t cause:runtime.memory().consumeCompletedDmacCauses()) scheduler.dispatchIrq(true,cause);
        auto& gs=runtime.memory().gs();
        if (gsInterrupt.observe(gs.csr.load(),gs.imr,gsCauseEnabled)) {
            if (++gsInterrupts<=16 || gsInterrupts%256==0)
                std::cerr << "[actual-gs-irq] csr=0x" << std::hex << gs.csr.load() << " imr=0x" << gs.imr
                          << std::dec << " count=" << gsInterrupts << '\n';
            scheduler.dispatchIrq(false,0);
        }
    }
};

inline void logEeIrqRegistration(bool dmac,uint32_t cause,uint32_t handler,uint32_t argument,uint32_t gp) {
    std::cerr << "[ee-irq-register] dmac=" << dmac << " cause=" << cause << " handler=0x" << std::hex
              << handler << " argument=0x" << argument << " gp=0x" << gp << std::dec << '\n';
}
inline void logEeIrqDelivery(bool dmac,uint32_t cause,size_t handlers) {
    if (cause>=7 || !handlers) return;
    static std::array<unsigned,14> counts{}; // EE executor only.
    const auto count=++counts[cause+(dmac?7:0)];
    if (count<=8) std::cerr << "[ee-irq-delivery] dmac=" << dmac << " cause=" << cause
                          << " handlers=" << handlers << " count=" << count << '\n';
}
