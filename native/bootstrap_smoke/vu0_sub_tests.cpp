#include "ee_vu0_sub.h"
#include <array>
#include <iostream>
#include <source_location>
#include <string>

namespace {
void check(bool value,const std::source_location at=std::source_location::current()) {
    if (!value) throw std::runtime_error("Macro-VU SUB regression line "+std::to_string(at.line()));
}
struct Rig {
    __m128 vu0_vf[32]{};
    uint32_t vu0_mac_flags{};
    uint16_t vu0_status{};
    Rig() { vu0_vf[0]=_mm_set_ps(1,0,0,0); }
};
std::array<float,4> lanes(__m128 value) {
    std::array<float,4> result; _mm_storeu_ps(result.data(),value); return result;
}
void discardedDestinationStillSetsFlags() {
    Rig r; const auto constant=lanes(r.vu0_vf[0]);
    // Shape of a flag-only projection range test. Old emission wrote this
    // result into VF0, corrupting the following reciprocal numerator.
    const __m128 point=_mm_set_ps(4,-3,0,-2);
    drakengard::eeVu0Sub(&r,0,15,point,_mm_setzero_ps());
    check(lanes(r.vu0_vf[0])==constant);
    check(r.vu0_mac_flags==0xa4 && r.vu0_status==0xc3);
    const float q=lanes(r.vu0_vf[0])[3]/8.0f;
    check(q==0.125f);
    // Demonstrate the original destination-zero assignment changes Q to .5.
    check(lanes(_mm_sub_ps(point,_mm_setzero_ps()))[3]/8.0f==0.5f);
}
void masksInactiveLanesAndStickyStatus() {
    Rig r; r.vu0_vf[7]=_mm_set_ps(40,30,20,10);
    r.vu0_mac_flags=0xffff; r.vu0_status=0x930;
    // Only X and Z participate; invalid inactive operands must not be read.
    const float invalid=std::bit_cast<float>(0x7fc00000u);
    drakengard::eeVu0Sub(&r,7,10,_mm_set_ps(invalid,2,invalid,-5),_mm_set_ps(invalid,2,invalid,1));
    check(lanes(r.vu0_vf[7])==std::array<float,4>{-6,20,0,40});
    check(r.vu0_mac_flags==0x82 && r.vu0_status==0x9f3);
    drakengard::eeVu0Sub(&r,7,1,_mm_set_ps(7,0,0,0),_mm_set_ps(2,0,0,0));
    check(r.vu0_mac_flags==0 && r.vu0_status==0x9f0);
    drakengard::eeVu0Sub(&r,7,0,_mm_set1_ps(invalid),_mm_set1_ps(invalid));
    check(r.vu0_mac_flags==0 && r.vu0_status==0x9f0);
}
void signedZeroAndAliasing() {
    Rig r; r.vu0_vf[2]=_mm_set_ps(2,1,0,-0.0f);
    drakengard::eeVu0Sub(&r,2,15,r.vu0_vf[2],_mm_set_ps(1,1,0,0));
    check(std::bit_cast<uint32_t>(lanes(r.vu0_vf[2])[0])==0x80000000u);
    check(r.vu0_mac_flags==0x8e && r.vu0_status==0xc3);
    // SUBx uses a caller-supplied broadcast, and destination can alias Fs.
    r.vu0_vf[3]=_mm_set_ps(8,6,4,2);
    const auto input=r.vu0_vf[3];
    drakengard::eeVu0Sub(&r,3,15,input,_mm_shuffle_ps(input,input,_MM_SHUFFLE(0,0,0,0)));
    check(lanes(r.vu0_vf[3])==std::array<float,4>{0,2,4,6});
    check(r.vu0_mac_flags==8);
}
void refusedCasesAreAtomic() {
    {
        Rig r; r.vu0_vf[1]=_mm_set1_ps(9); r.vu0_status=0x321; r.vu0_mac_flags=0x456;
        bool refused=false;
        try {
            drakengard::eeVu0Sub(&r,1,15,
                _mm_set_ps(std::bit_cast<float>(0x7fc00000u),3,2,1),_mm_setzero_ps());
        } catch (const std::runtime_error&) { refused=true; }
        check(refused && lanes(r.vu0_vf[1])==std::array<float,4>{9,9,9,9});
        check(r.vu0_status==0x321 && r.vu0_mac_flags==0x456);
    }
    for (auto value:{0x00000001u,0x80000001u,0x7f800000u,0x7fc00000u}) {
        Rig r; r.vu0_vf[1]=_mm_set1_ps(9); r.vu0_status=0x321; r.vu0_mac_flags=0x456;
        bool refused=false;
        try { drakengard::eeVu0Sub(&r,1,15,_mm_set1_ps(std::bit_cast<float>(value)),_mm_setzero_ps()); }
        catch (const std::runtime_error&) { refused=true; }
        check(refused && lanes(r.vu0_vf[1])==std::array<float,4>{9,9,9,9});
        check(r.vu0_status==0x321 && r.vu0_mac_flags==0x456);
    }
    for (bool overflow:{false,true}) {
        Rig r; r.vu0_vf[1]=_mm_set1_ps(9); r.vu0_status=0x321; r.vu0_mac_flags=0x456;
        const float a=overflow?std::numeric_limits<float>::max():std::numeric_limits<float>::min();
        const float b=overflow?-a:std::bit_cast<float>(0x00800001u);
        bool refused=false;
        try { drakengard::eeVu0Sub(&r,1,15,_mm_set1_ps(a),_mm_set1_ps(b)); }
        catch (const std::runtime_error&) { refused=true; }
        check(refused && lanes(r.vu0_vf[1])==std::array<float,4>{9,9,9,9});
        check(r.vu0_status==0x321 && r.vu0_mac_flags==0x456);
    }
}
void discardedAssignmentsEvaluateInput() {
    unsigned calls=0;
    auto input=[&]() { ++calls; return _mm_set_ps(4,3,2,1); };
    drakengard::discardVu0Write(input());
    check(calls==1);
}
}
int main() {
    try {
        discardedDestinationStillSetsFlags(); masksInactiveLanesAndStickyStatus();
        signedZeroAndAliasing(); refusedCasesAreAtomic(); discardedAssignmentsEvaluateInput();
        std::cout<<"Macro-VU SUB finite flag/constant checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
