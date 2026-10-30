#include "vif0_unpack.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
struct Registers { uint32_t cycle=0x101,mode=0,mask=0,row[4]{},col[4]{}; };
void check(bool value) { if(!value)throw std::runtime_error("VIF0 compressed UNPACK regression"); }
uint32_t word(const std::array<uint8_t,4096>& vu,unsigned address) { uint32_t value;std::memcpy(&value,vu.data()+address,4);return value; }
void put(std::array<uint8_t,4096>& vu,unsigned address,uint32_t value) { std::memcpy(vu.data()+address,&value,4); }
template<class Source> void unpack(uint32_t command,Registers& regs,const Source& source,std::array<uint8_t,4096>& vu) {
    uint32_t pos=0;
    drakengard::unpackVif0(command,regs,reinterpret_cast<const uint8_t*>(source.data()),unsigned(sizeof(source)),pos,vu.data(),[](bool,uint32_t){});
    check(pos==sizeof(source));
}
}
int main() {
    try {
        std::array<uint8_t,4096> vu{};
        Registers regs;
        // Compressed signed XYZ with ROW-controlled W: the old VIF0 skipped all lanes.
        regs.mask=0x40404040;regs.row[3]=0x3f800000;
        const std::array<int16_t,6> xyz{-1,2,-32768,32767,-2,3};
        unpack(0x7902000e,regs,xyz,vu);
        check(word(vu,14*16)==0xffffffff && word(vu,14*16+4)==2 && word(vu,14*16+8)==0xffff8000);
        check(word(vu,14*16+12)==0x3f800000 && word(vu,15*16)==32767 && word(vu,15*16+4)==0xfffffffe);
        // USN applies to bytes as well as halfwords.
        regs={};const std::array<uint8_t,8> rgba{255,0,128,1,1,2,3,254};
        unpack(0x6e02406e,regs,rgba,vu);
        check(word(vu,110*16)==255 && word(vu,110*16+8)==128 && word(vu,111*16+12)==254);
        // Every mask selection is distinct; protected data survives, ROW updates
        // only the data-selected component in difference mode.
        regs={};regs.cycle=0x404;regs.mask=0xe4e4e4e4;regs.mode=2;
        regs.row[0]=100;regs.row[1]=200;regs.row[2]=300;regs.row[3]=400;
        regs.col[0]=10;regs.col[1]=20;
        put(vu,4*16+12,0xabcdef01);put(vu,5*16+12,0x12345678);
        const std::array<uint8_t,8> masked{1,2,3,4,5,6,7,8};
        unpack(0x7e020004,regs,masked,vu);
        check(word(vu,4*16)==101 && word(vu,5*16)==106 && regs.row[0]==106);
        check(word(vu,4*16+4)==200 && word(vu,5*16+4)==200 && regs.row[1]==200);
        check(word(vu,4*16+8)==10 && word(vu,5*16+8)==20);
        check(word(vu,4*16+12)==0xabcdef01 && word(vu,5*16+12)==0x12345678);
        // Skip cycle writes leave skipped vectors intact.
        regs={};regs.cycle=0x103;vu.fill(0xa5);
        const std::array<int16_t,4> scalar{-1,2,-3,4};
        unpack(0x61040000,regs,scalar,vu);
        check(word(vu,0)==0xffffffff && word(vu,3*16)==2 && word(vu,6*16)==0xfffffffd && word(vu,9*16)==4);
        check(word(vu,16)==0xa5a5a5a5 && word(vu,2*16)==0xa5a5a5a5);
        // Fill cycles consume fewer source vectors; remaining lanes use ROW.
        regs={};regs.cycle=0x301;
        regs.row[0]=2;regs.row[1]=3;regs.row[2]=4;regs.row[3]=5;
        const std::array<uint32_t,1> fill{7};
        unpack(0x60030000,regs,fill,vu);
        check(word(vu,0)==7 && word(vu,12)==7 && word(vu,16)==2 && word(vu,20)==3);
        check(word(vu,24)==4 && word(vu,28)==5 && word(vu,32)==2 && word(vu,44)==5);
        // VIF0 wraps within its own4KiB; FLG is VIF1-only.
        regs={};const std::array<uint32_t,2> wrapped{0x11111111,0x22222222};
        unpack(0x600280ff,regs,wrapped,vu);
        check(word(vu,255*16)==0x11111111 && word(vu,0)==0x22222222);
        const auto original=vu;uint32_t pos=0;bool refused=false;
        try { drakengard::unpackVif0(0x63010000,regs,reinterpret_cast<const uint8_t*>(wrapped.data()),sizeof(wrapped),pos,vu.data(),[](bool,uint32_t){}); }
        catch(const std::runtime_error&) { refused=true; }
        check(refused && vu==original && pos==0);
        std::cout<<"VIF0 signed16, unsigned8, mask/mode, cycles, address wrap and reserved-format refusal passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
