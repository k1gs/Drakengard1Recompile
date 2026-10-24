#pragma once

#include "spu_reverb_dsp.h"
#include "spu_ram_write_port.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace iop_tests {

inline void nonzeroReverbCheck(bool value, const char* message) {
    if (!value) throw std::runtime_error(std::string("SPU reverb DSP: ") + message);
}

inline std::array<uint16_t, 48> reverbTestOffsets() {
    std::array<uint16_t, 48> result{};
    // These are SPU word offsets.  Deliberately do not multiply them by four:
    // the guest's reverb addresses are word-pair units.
    const std::array<uint32_t, 24> pairs{
        0x000f8000, 0x00000080, 0x00000080, 0x00000180, 0x000001a0,
        0x0000017c, 0x0000019c, 0x00000200, 0x00000220, 0x00000240,
        0x00000260, 0x00000280, 0x000002a0, 0x000002c0, 0x000002e0,
        0x00000300, 0x00000320, 0x00000340, 0x00000360, 0x00000400,
        0x00000440, 0x00000500, 0x00000540, 0x000fffff};
    for (unsigned i = 0; i < pairs.size(); ++i) {
        result[i * 2] = uint16_t(pairs[i] >> 16);
        result[i * 2 + 1] = uint16_t(pairs[i]);
    }
    return result;
}

inline std::array<uint16_t, 10> zeroReverbCoefficients() {
    return {};
}

inline std::array<uint16_t, 10> activeReverbCoefficients() {
    std::array<uint16_t, 10> result{};
    result.fill(0x4000u);
    return result;
}

inline uint32_t reverbTestAddress(const std::array<uint16_t, 48>& offsets,
                                  uint64_t cycle, uint32_t displacement) {
    const auto pair=[&](unsigned field) {
        return (uint32_t(offsets[field*2])<<16)|offsets[field*2+1];
    };
    const uint32_t start=pair(0)&0x3fffffu, end=(pair(23)&0x3fffffu)|0xffffu;
    return (((uint32_t(cycle)>>1)+displacement)%(end-start+1)+start)&0xfffffu;
}

inline void nonzeroSpuReverbDsp() {
    const auto offsets = reverbTestOffsets();
    const auto half = activeReverbCoefficients();

    // Empty delay RAM and zero coefficients isolate arbitrary signed input.
    // In FX-off mode no delay address may change.
    {
        SpuRamWritePort ram;
        ram.enable();
        const std::array<uint32_t, 4> destinations{
            reverbTestAddress(offsets, 0, 0x180), reverbTestAddress(offsets, 0, 0x1a0),
            reverbTestAddress(offsets, 0, 0x3a0), reverbTestAddress(offsets, 0, 0x3c0)};
        const std::array<uint16_t, 4> before{
            ram.peek(destinations[0]), ram.peek(destinations[1]),
            ram.peek(destinations[2]), ram.peek(destinations[3])};
        SpuReverbDsp dsp;
        for (uint64_t cycle = 0; cycle < 96; ++cycle) {
            const std::array<int32_t, 2> input{
                int32_t((cycle & 1) ? -12345 : 23456),
                int32_t((cycle & 1) ? 3011 : -17002)};
            const auto output = dsp.tick(ram, offsets, zeroReverbCoefficients(), false, cycle, input);
            nonzeroReverbCheck(output == std::array<int32_t, 2>{}, "zero coefficients produced output");
        }
        for (unsigned i = 0; i < before.size(); ++i)
            nonzeroReverbCheck(ram.peek(destinations[i]) == before[i], "FX-off changed effect RAM");
    }

    // Seed one comb source and use a zero input.  This isolates the downstream
    // comb/APF chain: 1000 * 0x4000 = 500, then the two 0x4000 APF stages store
    // 500 and 250 respectively, with read-before-write ordering.
    {
        SpuRamWritePort ram;
        ram.enable();
        ram.writeWord(reverbTestAddress(offsets, 0, 0x17c), 1000);
        SpuReverbDsp dsp;
        dsp.tick(ram, offsets, half, true, 0, {0, 0});
        nonzeroReverbCheck(ram.peek(reverbTestAddress(offsets, 0, 0x400)) == 500,
                           "seeded comb did not reach APF1");
        nonzeroReverbCheck(ram.peek(reverbTestAddress(offsets, 0, 0x500)) == 250,
                           "seeded APF1 did not reach APF2");
    }

    // FX-off advances the FIR/history clock but must not write the effect area.
    // Re-enabling writes then exposes the retained history, rather than acting
    // like a reset.  The comparison is against a fresh instance at the same
    // post-latency point, so it does not depend on a particular preset value.
    {
        SpuRamWritePort ram;
        ram.enable();
        SpuReverbDsp retained;
        const auto active = activeReverbCoefficients();
        for (uint64_t cycle = 0; cycle < 64; ++cycle)
            retained.tick(ram, offsets, active, false, cycle, {16000, -9000});
        retained.tick(ram, offsets, active, true, 64, {16000, -9000});
        // The 39 taps sum to 32766: settled 16000 becomes 15999, input gain
        // 0x4000 produces 7999, and the first IIR store is exactly 3999.
        nonzeroReverbCheck(ram.peek(reverbTestAddress(offsets, 64, 0x180)) == 3999,
                           "FX-on did not commit the expected delayed reflection");

        SpuRamWritePort freshRam;
        freshRam.enable();
        SpuReverbDsp fresh;
        fresh.tick(freshRam, offsets, active, true, 0, {16000, -9000});
        nonzeroReverbCheck(freshRam.peek(reverbTestAddress(offsets, 0, 0x180)) != 3999,
                           "fresh DSP unexpectedly contained retained FIR history");
    }

    // Negative input and positive IIR/comb/APF coefficients must eventually
    // produce a real delayed response and writes; zero substitution would fail.
    {
        SpuRamWritePort ram;
        ram.enable();
        SpuReverbDsp dsp;
        const auto active = activeReverbCoefficients();
        for (unsigned word = 0; word < 96; ++word) {
            ram.writeWord(reverbTestAddress(offsets, 0, 0x17c + word), 1000);
            ram.writeWord(reverbTestAddress(offsets, 0, 0x19c + word), 1000);
        }
        bool nonzeroOutput = false;
        for (uint64_t cycle = 0; cycle < 128; ++cycle) {
            const auto output = dsp.tick(ram, offsets, active, true, cycle, {-28000, 17000});
            nonzeroOutput |= output[0] != 0 || output[1] != 0;
        }
        nonzeroReverbCheck(nonzeroOutput, "active reverb suppressed nonzero delayed response");
    }

    // Odd/even phase selects the active channel.  Seed only the left comb
    // source: an even tick must consume it, while the following odd tick must
    // leave that left-only source untouched by the right projection.
    {
        SpuRamWritePort ram;
        ram.enable();
        const auto active = activeReverbCoefficients();
        const auto source = reverbTestAddress(offsets, 0, 0x17c);
        ram.writeWord(source, 2000);
        SpuReverbDsp dsp;
        dsp.tick(ram, offsets, active, true, 0, {0, 0});
        dsp.tick(ram, offsets, active, true, 1, {0, 0});
        const auto rightDestination = reverbTestAddress(offsets, 1, 0x1a0);
        nonzeroReverbCheck(ram.peek(rightDestination) == 0,
                           "odd parity consumed the left-only source");
    }
    {
        // All destinations and allpass sources alias. The two APFs must read
        // the original1000 before any reflection write. Final store=500:
        // comb=500; APF1 store0/output1000; APF2 store500/output1250.
        auto aliases=offsets;
        for (unsigned field=1;field<23;++field) aliases[field*2]=aliases[field*2+1]=0;
        std::array<uint16_t,10> gains{}; gains[1]=gains[6]=gains[7]=0x4000;
        SpuRamWritePort ram; ram.enable();
        ram.writeWord(0xf8000,1000); ram.writeWord(0xfffff,50);
        SpuReverbDsp dsp; dsp.tick(ram,aliases,gains,true,0,{0,0});
        nonzeroReverbCheck(ram.peek(0xf8000)==500,"alias write changed later source reads");
        nonzeroReverbCheck(ram.peek(0xfffff)==50,"wrapped previous address was overwritten");
        nonzeroReverbCheck(ram.peek(0xf7fff)==0,"write escaped effect workspace");
    }
    {
        // A negative settled input uses floor shifts, not truncation toward0.
        SpuRamWritePort ram; ram.enable(); SpuReverbDsp dsp;
        const auto gains=activeReverbCoefficients();
        for (uint64_t cycle=0;cycle<64;++cycle) dsp.tick(ram,offsets,gains,false,cycle,{-16000,0});
        dsp.tick(ram,offsets,gains,true,64,{-16000,0});
        nonzeroReverbCheck(ram.peek(reverbTestAddress(offsets,64,0x180))==uint16_t(-4000),
                           "negative FIR/IIR shifts lost their floor rounding");
    }
}

} // namespace iop_tests
