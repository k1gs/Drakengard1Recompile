#pragma once
#include <array>
#include <cstdint>
#include <ostream>

namespace drakengard {
// One selected draw only. Counters describe executed backend decisions, not
// hardware correctness, object identity, or visibility after later draws.
struct GsFragmentProbe {
    enum Stage { Enter, ScissorReject, AlphaReject, DestinationReject,
                 DepthReject, FrameWrite, DepthWrite, StageCount };
    std::array<uint64_t,StageCount> counts{};
    std::array<uint64_t,8> alphaMasks{};
    std::array<uint64_t,4> textureAlpha{}, combinedAlpha{};
    struct Depth { uint32_t incoming{}, stored{}; bool pass{}; };
    std::array<Depth,16> depth{};
    unsigned depthCount{};
    static unsigned alphaBucket(unsigned a) { return a==0?0:a<128?1:a==128?2:3; }
    void texture(unsigned texelAlpha,unsigned resultAlpha) {
        ++textureAlpha[alphaBucket(texelAlpha)];++combinedAlpha[alphaBucket(resultAlpha)];
    }
    void mask(bool rgb,bool alpha,bool z) { ++alphaMasks[unsigned(rgb)|(unsigned(alpha)<<1)|(unsigned(z)<<2)]; }
    void testDepth(uint32_t incoming,uint32_t stored,bool pass) {
        if(depthCount<depth.size())depth[depthCount++]={incoming,stored,pass};
    }
    template<class A> static void array(std::ostream& out,const A& a) {
        out<<'[';bool first=true;for(auto v:a) { if(!first)out<<',';first=false;out<<v; }out<<']';
    }
    void write(std::ostream& out) const {
        out<<"{\"stages_enter_scissor_alpha_date_depth_framewrite_zwrite\":";array(out,counts);
        out<<",\"alpha_masks_rgb_alpha_z\":";array(out,alphaMasks);
        out<<",\"texture_alpha_zero_low_128_high\":";array(out,textureAlpha);
        out<<",\"combined_alpha_zero_low_128_high\":";array(out,combinedAlpha);
        out<<",\"first_depth_tests\":[";
        for(unsigned i=0;i<depthCount;++i) { if(i)out<<',';out<<'['<<depth[i].incoming<<','<<depth[i].stored<<','<<(depth[i].pass?1:0)<<']'; }
        out<<"]}";
    }
};
inline thread_local GsFragmentProbe* gsFragmentProbe{};
inline void observeGsFragment(GsFragmentProbe::Stage stage) {
    if(gsFragmentProbe)++gsFragmentProbe->counts[stage];
}
}
