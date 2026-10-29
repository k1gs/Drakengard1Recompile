#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace drakengard {
struct EeGeometryProbeState { uint32_t stage,pc,sp,resource,partOffset,groupOrdinal; };
class EeGeometryProbe {
    std::array<std::array<std::array<unsigned,2>,3>,2> records{};
public:
    void observe(const uint8_t* ram,const EeGeometryProbeState& state,const char* output) {
        if(!output || !*output)return;
        unsigned slot;
        if(state.stage==0x29e1a0)slot=0;
        else if(state.stage==0x29e21c)slot=1;
        else throw std::runtime_error("Unknown geometry observation boundary");
        const auto range=[](uint32_t a,uint32_t n) {
            if(a>0x2000000u || n>0x2000000u-a)
                throw std::runtime_error("Geometry observation outside EE RAM");
        };
        const auto word=[&](uint32_t a) { range(a,4);uint32_t v;std::memcpy(&v,ram+a,4);return v; };
        range(state.sp,0x200);
        // This function saved its caller's S1 descriptor and RA in its own
        // existing frame. Validate that caller before assigning model identity.
        if(word(state.sp+0x1f8)!=0x19de5c || state.resource!=word(0x40e234))return;
        const auto descriptor=word(state.sp+0x1b8);range(descriptor,16);
        if(word(descriptor)!=0x3b0620 || (word(descriptor+4)&0xffff)!=1 ||
           word(descriptor+12)!=0x3b0700)return;
        if(state.partOffset%16 || state.partOffset/16>=3 || !state.groupOrdinal)
            throw std::runtime_error("Unexpected body geometry part or group");
        const unsigned part=state.partOffset/16;
        const std::array<uint32_t,3> fader{word(0x48a42c),word(0x48a430),word(0x48a434)};
        const unsigned phase=fader==std::array<uint32_t,3>{}?1:0;
        if(records[slot][part][phase])return;
        const auto mesh=word(state.sp+0x14);range(mesh,0x40);
        const auto skeleton=word(state.resource+0xc8);range(skeleton,0x28);
        const auto bones=word(skeleton+0x24),poses=word(state.sp+0x190);
        if(bones>128)throw std::runtime_error("Geometry bone count exceeds caller buffer");
        range(poses,bones*64);
        std::ofstream file(output,std::ios::app);
        file<<"{\"stage\":"<<state.stage<<",\"pc\":"<<state.pc<<",\"sp\":"<<state.sp
            <<",\"resource\":"<<state.resource<<",\"part\":"<<part
            <<",\"group\":"<<state.groupOrdinal-1<<",\"phase\":"<<phase
            <<",\"mesh\":"<<mesh<<",\"mesh_flags\":"<<word(mesh+8)
            <<",\"poses\":"<<poses<<",\"bone_count\":"<<bones;
        const auto words=[&](const char* name,uint32_t a,unsigned n) {
            range(a,n*4);file<<",\""<<name<<"\":[";
            for(unsigned i=0;i<n;++i) { if(i)file<<',';file<<word(a+4*i); }file<<']';
        };
        words("fader",0x48a42c,3);
        words("mesh_descriptor_words",state.sp+0x10,12);
        words("output_matrix_bits",poses,bones*16);
        words("packet_builder_words",state.sp+0xd0,16);
        // The verified part0/flags1 handler builds this shared VIF1 DMA chain.
        // Capture only after that handler returns. This is a bounded prefix,
        // not a claim that every referenced DMA payload has been captured.
        if(slot==1 && part==0 && word(mesh+8)==1)
            words("vif1_chain_prefix_words",0x665bc0,1024);
        file<<"}\n";file.flush();
        if(!file)throw std::runtime_error("Cannot write bounded geometry observation");
        ++records[slot][part][phase];
    }
};
inline void observeEeGeometryProbe(const uint8_t* ram,const EeGeometryProbeState& state) {
    static EeGeometryProbe probe;
    static const char* output=std::getenv("DRAKENGARD_EE_GEOMETRY_TRACE");
    probe.observe(ram,state,output);
}
}
