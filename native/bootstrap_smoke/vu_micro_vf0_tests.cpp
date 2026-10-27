#include "runtime/ps2_vu1.h"
#include "runtime/gs/gs_frontend.h"
#include "runtime/ps2_memory.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

// These programs contain no GIF instructions. Trap unexpected collaborators.
GS::GS() = default;
void GS::processGIFPacket(const uint8_t*,uint32_t) { std::abort(); }
void PS2Memory::submitGifPacket(GifPathId,const uint8_t*,uint32_t,bool,bool) { std::abort(); }
namespace {
void check(bool value) { if(!value)throw std::runtime_error("Micro VU VF0 regression"); }
struct Result { std::array<float,4> stored,vf1,acc; uint32_t mac,status; };
Result run(uint32_t upper,VU1Interpreter::Unit unit,std::array<float,4> input={2,3,4,5}) {
    auto vu=std::make_unique<VU1Interpreter>(unit); auto gs=std::make_unique<GS>();
    std::array<uint8_t,16384> data{};
    // Lower SQ.xyzw VF0,0(VI0), E-bit upper, then a NOP delay slot.
    const std::array<uint32_t,4> words{0x03e00000u,upper|0x40000000u,0x8000033cu,0x000002ffu};
    std::array<uint8_t,16> code{}; std::memcpy(code.data(),words.data(),code.size());
    std::memcpy(vu->state().vf[1],input.data(),16);
    vu->execute(code.data(),code.size(),data.data(),data.size(),*gs,nullptr,0,0,0,32);
    Result r{};std::memcpy(r.stored.data(),data.data(),16);
    std::memcpy(r.vf1.data(),vu->state().vf[1],16);std::memcpy(r.acc.data(),vu->state().acc,16);
    r.mac=vu->state().mac;r.status=vu->state().status;return r;
}
}
int main() {
  try {
    const std::array<float,4> constant{0,0,0,1};
    for(auto unit:{VU1Interpreter::Unit::VU0,VU1Interpreter::Unit::VU1}) {
        check(run(0x000002ffu,unit).stored==constant); // NOP control.
        auto nonzero=run(0x01e10868u,unit); // ADD VF1,VF1,VF1.
        check(nonzero.stored==constant && nonzero.vf1==std::array<float,4>{4,6,8,10});
        check(run(0x01e10828u,unit).stored==constant); // Same-pair ADD VF0 + SQ VF0.
        auto flags=run(0x01e10828u,unit,{-2,0,3,0.5f});
        check(flags.stored==constant && flags.mac==0x84u && (flags.status&0xc3u)==0xc3u);
        auto masked=run(0x01810828u,unit,{-2,0,3,0.5f}); // Only X/Y participate.
        check(masked.stored==constant && masked.mac==0x84u);
        auto acc=run(0x01e1083cu,unit); // ADDAX ACC,VF1,VF1x.
        check(acc.stored==constant && acc.acc==std::array<float,4>{4,5,6,7});
    }
    std::cout<<"VU0/VU1 same-pair VF0 store, flags, masks, VF1 and ACC controls passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr<<error.what()<<'\n'; return 1;
  }
}
