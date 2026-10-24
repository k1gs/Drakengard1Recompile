#pragma once

#include "bare_call.h"

namespace iop_tests {
inline void nonzeroSpuWetRouting() {
    using ps2x::iop::detail::IopMemory;
    const auto run=[&](uint16_t vmixel,uint16_t vmixer,uint16_t mmix,bool mustReject,bool fxActive=true,bool externalWet=false) {
        IopMemory memory; SpuRamWritePort ram; ram.enable();
        SpuIdleVoices voices; voices.enable(0,&ram);
        SpuReverbSetup effects; effects.enable(&ram); if (fxActive) effects.setEffects(0,true);
        const auto store=[&](auto& device,uint32_t address,uint16_t value) {
            auto access=device.stage(memory,address,address,0x29,value);
            device.complete(memory,access,false);
        };
        // Predictor 0, shift 4, all-positive nibbles: a real nonzero loop.
        store(ram,0x1f9001a8,0);
        store(ram,0x1f9001aa,0x2800);
        store(ram,0x1f9001ac,0x0704);
        for (unsigned word=1;word<8;++word) store(ram,0x1f9001ac,0x1111);

        const uint32_t voice=0x1f900000;
        store(voices,voice,vmixer ? 0 : 0x3fff); // isolate left when wet-left is selected
        store(voices,voice+2,vmixer ? 0x3fff : 0); // isolate right for wet-right
        store(voices,voice+4,0x1000);     // one-sample pitch
        store(voices,0x1f90018c,vmixel);  // VMIXEL: wet-left voice gate
        store(voices,0x1f900190,0x0001);  // VMIXR: dry-right only, must not wet-left
        store(voices,0x1f900194,vmixer);  // VMIXER: wet-right voice gate
        store(voices,0x1f900198,mmix);    // MMIX source gates
        if (externalWet) store(voices,0x1f900598,2); // core1 external-wet-left
        store(voices,0x1f9001a0,1);       // core 0 voice 0 KON

        bool rejected=false;
        try { voices.advanceTo(768*2,&effects); }
        catch (const std::runtime_error&) { rejected=true; }
        if (voices.nonzeroSampleCount()==0 || voices.pcm(0,0)==0)
            throw std::runtime_error("Nonzero SPU wet fixture did not produce PCM");
        if (rejected!=mustReject)
            throw std::runtime_error("SPU wet routing guard result differed from gate mapping");
    };

    // Wet-left and wet-right are independently routed into the active FX path.
    run(1,0,0x0ff0,true);
    run(0,1,0x0ff0,true);

    // A dry-only gate does not feed the wet input, and disabled MMIX voice
    // sources legitimately bypass the zero-input guard.
    run(0,0,0x0ff0,false);
    run(1,0,0x0df0,false); // clear MMIX wet-sound-left (bit 9)
    run(1,0,0x0ff0,false,false); // nonzero wet routing is allowed while FX is inactive
    run(0,0,0x0ff0,true,false,true); // unsupported nonzero cross-core wet path stays closed
}
}
