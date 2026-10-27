#include "ee_gif_write_probe.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition) {
    if (!condition) throw std::runtime_error("EE GIF write probe contract failed");
}
}

int main() {
    drakengard::EeGifWriteProbe probe;
    drakengard::EeGifProbeReport report;
    probe.observe(0x1000, 8, 0, 0, 0x2000, 0x3000);
    require(!probe.snapshot(report) && !probe.active());
    for (auto address : {0x1000a000u, 0x70000000u, 0x80001000u, 0xffffffffu})
        require(!probe.arm(address, 8, 1));
    require(!probe.arm(0, 0, 1) && !probe.arm(0x1000, 17, 1) && !probe.arm(0x1000, 8, 0));
    require(!probe.arm(drakengard::EeGifProbeRamBytes - 7, 8, 1));

    require(probe.arm(0x1008, 8, 10, 0x4000));
    require(!probe.arm(0x2000, 8, 11));
    probe.observe(0x1000, 8, 5, 0, 1, 2); // Adjacent, not overlapping.
    probe.observe(0x1010, 8, 5, 0, 1, 2);
    probe.observe(0x1008, 0, 5, 0, 1, 2);
    probe.observe(0x1008, 32, 5, 0, 1, 2); // Bulk writes require a separate producer hook.
    probe.observe(0x80001008, 8, 5, 0, 1, 2);
    probe.observe(0x1008, 8, 0, 0, 0x2000, 0x3000, 0x5000);
    probe.observe(0x1008, 8, 0, 0, 0x2004, 0x3004, 0x5000); // Unchanged zero is still an attempt.
    probe.observe(0x1000, 16, 0x1234, 0x5678, 0x2008, 0x3008, 0x5000);
    require(!probe.consume(0x1008, 8, 10, report)); // Same packet did not consume a new generation.
    require(!probe.consume(0x1008, 4, 11, report)); // Partial consumption remains open.
    require(probe.snapshot(report) && report.consumedPacket == 0 && report.totalAttempts == 3);
    require(probe.consume(0x1000, 16, 11, report, 0x4010));
    require(!probe.active() && report.generation == 1 && report.armedPacket == 10 && report.consumedPacket == 11);
    require(report.address == 0x1008 && report.bytes == 8 && report.armedDmaTag == 0x4000 && report.consumedDmaTag == 0x4010);
    require(report.count == 3 && report.droppedAttempts == 0);
    require(report.attempts[0].low == 0 && report.attempts[1].low == 0 && report.attempts[1].pc == 0x2004);
    require(report.attempts[2].high == 0x5678 && report.attempts[2].address == 0x1000 && report.attempts[2].width == 16);
    probe.observe(0x1008, 8, 7, 0, 1, 2); // Closed interval does not collect more stores.
    require(!probe.snapshot(report) && !probe.consume(0x1008, 8, 12, report));

    require(probe.arm(0x1008, 8, 20));
    for (uint64_t i = 0; i < 140; ++i) probe.observe(0x1008, 8, i, 0, 0x2000 + uint32_t(i) * 4, 0x3000);
    require(probe.consume(0x1008, 8, 21, report));
    require(report.generation == 2 && report.totalAttempts == 140 && report.count == 128 && report.droppedAttempts == 12);
    require(report.attempts[0].sequence == 13 && report.attempts[0].low == 12);
    require(report.attempts[127].sequence == 140 && report.attempts[127].low == 139);
    std::ostringstream json;
    drakengard::writeEeGifProbeJson(json, report);
    require(json.str().find("cpu_store_attempts_only") != std::string::npos);
    require(json.str().find("\"dropped_attempts\":12") != std::string::npos);

    require(probe.arm(drakengard::EeGifProbeRamBytes - 8, 8, 30));
    probe.observe(drakengard::EeGifProbeRamBytes - 4, 8, 9, 0, 1, 2); // Out-of-RAM store is rejected.
    require(probe.consume(drakengard::EeGifProbeRamBytes - 8, 8, 31, report));
    require(report.totalAttempts == 0); // Empty CPU report makes no statement about other writers.
    require(probe.arm(0x1008, 16, 32));
    for (uint32_t width : {1u, 2u, 4u, 8u, 16u}) probe.observe(0x1008, width, 0x12, 0x34, 0x2000, 0x3000);
    require(probe.consume(0x1008, 16, 33, report) && report.count == 5);
    for (size_t i = 0; i < report.count; ++i) require(report.attempts[i].width == (1u << i));
    require(probe.arm(0x1008, 8, 40)); probe.cancel(); require(!probe.active());
    std::cout << "Bounded physical-RAM CPU attempts, zero writes, reuse generations and later consumption passed\n";
}
