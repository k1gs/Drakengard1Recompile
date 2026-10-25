#pragma once
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <mutex>

// Actual pre-raster samples only. Diagnostic tracing must not change any color.
// Bounded records from textured, blended primitives with visible sampled texels.
template<class State> void observeGsColor(const State& state,unsigned r,unsigned g,unsigned b,unsigned a,
                                          uint32_t texel,unsigned outR,unsigned outG,unsigned outB,unsigned outA) {
    if (a<64 || !state.prim.abe || state.context.tex0.tfx!=0 || ((texel&0xffffffu)==0) || !(texel>>24)) return;
    static const auto* path=std::getenv("DRAKENGARD_GS_COLOR_OUTPUT"); if (!path) return;
    static std::mutex mutex; static std::atomic<unsigned> samples{};
    if (samples.load(std::memory_order_relaxed)>=256) return;
    std::lock_guard lock(mutex);
    if (samples.fetch_add(1,std::memory_order_relaxed)>=256) return;
    std::ofstream file(path,std::ios::app);
    const auto& tex=state.context.tex0;
    file << "frame=" << state.context.frame.fbp << " rgba=" << r << ',' << g << ',' << b << ',' << a
         << " texel=0x" << std::hex << texel << " blend=0x" << state.context.alpha << std::dec
         << " tcc=" << unsigned(tex.tcc) << " tfx=" << unsigned(tex.tfx) << " psm=" << unsigned(tex.psm)
         << " cpsm=" << unsigned(tex.cpsm) << " cbp=" << tex.cbp
         << " combined=" << outR << ',' << outG << ',' << outB << ',' << outA << '\n';
}
