#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace drakengard {
struct EeBodyProbeState {
    uint32_t stage, pc, descriptor, sp, returnedResource, savedResource;
};

// Bound only by the verified Europe preparer. Observe existing post-call frames;
// the caller has already allocated its 0x20c0-byte stack frame.
class EeBodyProbe {
    std::array<unsigned,3> records{};
public:
    void observe(const uint8_t* ram, const EeBodyProbeState& state, const char* output) {
        if(!output || !*output)return;
        constexpr std::array<uint32_t,3> stages{0x19dda8,0x19de48,0x19de5c};
        unsigned slot=0;while(slot<stages.size() && stages[slot]!=state.stage)++slot;
        if(slot==stages.size())throw std::runtime_error("Unknown body observation boundary");
        if(records[slot]>=4)return;
        const auto range=[](uint32_t address,uint32_t size) {
            if(address>0x2000000u || size>0x2000000u-address)
                throw std::runtime_error("Body observation outside EE RAM");
        };
        const auto word=[&](uint32_t address) {
            range(address,4);uint32_t value;std::memcpy(&value,ram+address,4);return value;
        };
        range(state.descriptor,16);
        // Match the body descriptor, not other models sharing this renderer.
        if(word(state.descriptor)!=0x3b0620 ||
           (word(state.descriptor+4)&0xffff)!=1 ||
           word(state.descriptor+12)!=0x3b0700)return;
        range(state.sp,512);
        const uint32_t resource=slot==0?state.returnedResource:state.savedResource;
        if(resource)range(resource,256);
        std::ofstream file(output,std::ios::app);
        file<<"{\"stage\":"<<state.stage<<",\"pc\":"<<state.pc
            <<",\"descriptor\":"<<state.descriptor<<",\"sp\":"<<state.sp
            <<",\"v0\":"<<state.returnedResource
            <<",\"resolved_resource\":"<<resource<<",\"ui\":"<<word(0x4fbd30);
        const auto words=[&](const char* name,uint32_t address,unsigned count) {
            file<<",\""<<name<<"\":[";
            for(unsigned i=0;i<count;++i) { if(i)file<<',';file<<word(address+4*i); }
            file<<']';
        };
        words("descriptor_words",state.descriptor,4);
        words("actor_world_matrix_bits",0x3b0640,16);
        // A bounded prefix, not an asserted count of valid bone matrices.
        words("output_prefix_bits",state.sp,128);
        if(resource)words("resource_prefix_words",resource,64);
        file<<"}\n";file.flush();
        if(!file)throw std::runtime_error("Cannot write bounded body observation");
        ++records[slot];
    }
};
inline void observeEeBodyProbe(const uint8_t* ram,const EeBodyProbeState& state) {
    static EeBodyProbe probe;
    static const char* output=std::getenv("DRAKENGARD_EE_BODY_TRACE");
    probe.observe(ram,state,output);
}
}
