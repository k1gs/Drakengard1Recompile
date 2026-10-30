#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace drakengard {
struct EeControlProbeState { uint32_t point,pc,a0,a1,v0,ra,sp; };
class EeControlProbe {
    static constexpr std::array<uint32_t,22> points{
        0x1441e4,0x14421c,0x1408a0,0x140924,0x144884,0x1448c8,
        0x1448dc,0x1448ec,0x19a808,0x19b2b4,0x14c350,0x14c39c,
        0x14c3ac,0x14c3d4,0x143698,0x143598,0x143600,0x143610,
        0x143634,0x14365c,0x14caf0,0x14cb54};
    struct History { uint64_t calls{};unsigned records{};uint32_t a0{},a1{},flags{},result{}; };
    std::array<std::array<History,3>,points.size()> history{};
public:
    void observe(const uint8_t* ram,const EeControlProbeState& s,const char* path) {
        if(!path || !*path)return;
        unsigned slot=0;while(slot<points.size() && points[slot]!=s.point)++slot;
        if(slot==points.size())throw std::runtime_error("Unknown control observation point");
        const auto range=[](uint32_t a,uint32_t n) { return a<=0x2000000u && n<=0x2000000u-a; };
        const auto word=[&](uint32_t a) { if(!range(a,4))throw std::runtime_error("Control observation outside EE RAM");uint32_t v;std::memcpy(&v,ram+a,4);return v; };
        const auto ui=word(0x4fbd30),flags=word(0x3b4c9c);
        const unsigned phase=ui!=4?0:((word(0x48a42c)|word(0x48a430)|word(0x48a434))?1:2);
        auto& h=history[slot][phase];++h.calls;
        const bool changed=s.a0!=h.a0 || s.a1!=h.a1 || flags!=h.flags || s.v0!=h.result;
        h.a0=s.a0;h.a1=s.a1;h.flags=flags;h.result=s.v0;
        if(h.records>=24 || (h.records>=4 && !changed && h.calls%256))return;
        if(!range(s.sp,64))throw std::runtime_error("Control observation stack outside EE RAM");
        ++h.records;
        std::ofstream out(path,std::ios::app);
        out<<"{\"point\":"<<s.point<<",\"pc\":"<<s.pc<<",\"a0\":"<<s.a0
           <<",\"a1\":"<<s.a1<<",\"v0\":"<<s.v0<<",\"ra\":"<<s.ra<<",\"sp\":"<<s.sp
           <<",\"ui\":"<<ui<<",\"phase\":"<<phase<<",\"calls\":"<<h.calls<<",\"manager_flags\":"<<flags;
        const auto words=[&](const char* key,uint32_t address,unsigned count) {
            out<<",\""<<key<<"\":[";for(unsigned i=0;i<count;++i) { if(i)out<<',';out<<word(address+i*4); }out<<']';
        };
        words("manager_words",0x3b4c20,48);
        words("event_storage_words",0x3b4ce0,81);
        words("stack_words",s.sp,16);
        // Record the actual argument address, including embedded-object offsets.
        if(range(s.a0,0x144))words("argument_words",s.a0,81);
        words("caim_command_words",0x3b0c54,14);
        out<<"}\n";out.flush();
        if(!out)throw std::runtime_error("Cannot write control observation");
    }
};
inline EeControlProbe eeControlProbe;
}
