#pragma once
#include "core/iop_cpu.h"
#include "core/iop_memory.h"
#include <stdexcept>

namespace iop_tests {
// Synthetic byte values and addresses only. Exercise the real pinned CPU,
// including the load delay after the second member of an unaligned pair.
inline void unalignedLoadPairs() {
    using namespace ps2x::iop::detail;
    for (unsigned alignment = 0; alignment < 4; ++alignment) {
        for (bool rightFirst : {false, true}) {
            IopMemory memory;
            IopCpuCore core(memory);
            IopCpuState cpu{};
            cpu.pc = 0x1000;
            cpu.gpr[4] = 0x800 + alignment;
            cpu.gpr[2] = 0xa1b2c3d4;
            for (unsigned n = 0; n < 8; ++n) memory.write8(0x800 + n, uint8_t(0x31 + n * 17));
            const uint32_t left = 0x88820003, right = 0x98820000; // lwl/lwr v0,3/0(a0)
            memory.write32(0x1000, rightFirst ? right : left);
            memory.write32(0x1004, rightFirst ? left : right);
            memory.write32(0x1008, 0x00401821); // addu v1,v0,zero: still sees first partial result.
            memory.write32(0x100c, 0);
            uint32_t expected = 0, partial = cpu.gpr[2];
            for (unsigned n = 0; n < 4; ++n) {
                const uint32_t byte = memory.read8(cpu.gpr[4] + n);
                expected |= byte << (n * 8);
                const bool fromFirst = rightFirst ? n < 4 - alignment : n >= (4 - alignment) % 4;
                if (fromFirst) partial = (partial & ~(0xffu << (n * 8))) | (byte << (n * 8));
            }
            const auto retire = [&] {
                if (!core.executeInstruction(cpu) || cpu.exception)
                    throw std::runtime_error("Synthetic unaligned load raised an exception");
            };
            retire();
            if (!cpu.pendingLoad || cpu.gpr[2] != 0xa1b2c3d4)
                throw std::runtime_error("First partial load lost its architectural delay");
            retire();
            if (!cpu.pendingLoad || cpu.gpr[2] != partial)
                throw std::runtime_error("First partial load did not retire at the pair boundary");
            retire();
            if (cpu.gpr[2] != expected || cpu.gpr[3] != partial || cpu.pendingLoad)
                throw std::runtime_error("Consecutive LWL/LWR failed byte merge or final load delay");
        }
    }
}
}
