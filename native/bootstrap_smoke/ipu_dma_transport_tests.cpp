#include "../runner/ee_ipu.h"
#include "../runner/ee_ipu_dma.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

using drakengard::EeIpu;
using drakengard::EeIpuDma;

static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static void refuses(auto&& operation) {
    try { operation(); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("unsupported IPU DMA boundary was accepted");
}

int main() {
    try {
        EeIpu ipu;
        std::vector<uint8_t> ram(0x100, 0xa5);
        std::vector<uint8_t> scratch(0x4000, 0x5a);
        EeIpuDma dma;
        const uint32_t from=0x1000b000u;

        // An empty output FIFO stalls from-IPU DMA.  It must not consume the
        // destination or manufacture a completion edge.
        dma.write(from+0x10, 0x80001800u);
        dma.write(from+0x20, 1u);
        dma.write(from, 0x100u);
        dma.pump(ipu, ram, scratch, true);
        check(dma.read(from+0x20)==1u && (dma.read(from)&0x100u),
              "empty from-IPU output did not stall");
        check(dma.takeCompletions()==0u && scratch[0x1800]==0x5a,
              "empty from-IPU output changed state");

        const uint32_t to=0x1000b400u;
        dma.write(from, 0u);
        dma.write(to+0x10, 0x100u);
        dma.write(to+0x20, 1u);
        dma.write(to, 0x101u);
        refuses([&] { dma.pump(ipu, ram, scratch, true); });
        dma.write(to, 0u);

        // A main-RAM transfer crossing the bound retains the accepted first QW
        // and rejects the next QW. Scratchpad aliases never become RDRAM.
        dma.write(to+0x10, 0xf0u);
        dma.write(to+0x20, 2u);
        dma.write(to, 0x101u);
        refuses([&] { dma.pump(ipu, ram, scratch, true); });
        check(dma.read(to+0x20)==1 && dma.read(to+0x10)==0x100,
              "crossing RAM bound lost accepted progress");
        dma.write(to, 0u);

        dma.write(to+0x10, 0x80000000u);
        dma.write(to+0x20, 1u);
        dma.write(to, 0x101u);
        refuses([&] { dma.pump(ipu, ram, scratch, true); });

        std::cout << "IPU DMA transport boundary regressions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
