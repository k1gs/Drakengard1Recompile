#include "ee_spr_vu0.h"
#include <array>
#include <iostream>
#include <source_location>
#include <string>

namespace {
void check(bool value,const std::source_location at=std::source_location::current()) {
    if (!value) throw std::runtime_error("VU0 toSPR regression line "+std::to_string(at.line()));
}
struct Rig {
    std::array<uint8_t,4096> vu{};
    std::array<uint8_t,16384> spr{};
    drakengard::EeToSprVu0State state{0x101,0x110049e0,0x60,0};
    Rig() {
        for (unsigned n=0;n<vu.size();++n) vu[n]=uint8_t(n*37u+(n>>5)+13);
        spr.fill(0xcd);
    }
    bool run(bool enabled=true) { return drakengard::transferEeVu0ToSpr(state,vu,spr,enabled); }
};
void measuredTransferAndSharedSource() {
    Rig r;
    // A producer writes after binding the same RAM; do not copy a stale buffer.
    r.vu[0x9e0]=0x7b; r.vu[0xfdf]=0x91;
    const auto source=r.vu;
    check(r.run());
    check(r.state.control==1 && r.state.address==0x11004fe0 && r.state.count==0 && r.state.scratch==0x600);
    for (unsigned n=0;n<r.spr.size();++n) check(r.spr[n]==(n<1536?source[0x9e0+n]:0xcd));
    check(r.vu==source);
    const auto after=r.spr;
    check(!r.run() && r.spr==after); // No repeated completion edge on service.
}
void deferredStartAndWrap() {
    Rig r; r.state.scratch=0x70003ff0;
    const auto before=r.spr;
    check(!r.run(false) && r.spr==before && r.state.control==0x101 && r.state.scratch==0x70003ff0);
    check(r.run() && r.state.scratch==0x5f0);
    for (unsigned n=0;n<r.spr.size();++n) {
        const unsigned input=(n+16)&0x3fff;
        check(r.spr[n]==(input<1536?r.vu[0x9e0+input]:0xcd));
    }
    Rig last; last.state.address=0x11004ff0; last.state.count=1; last.state.scratch=0x3ff0;
    check(last.run() && last.state.address==0x11005000 && last.state.scratch==0);
    for (unsigned n=0;n<16;++n) check(last.spr[0x3ff0+n]==last.vu[0xff0+n]);
    Rig empty; empty.state.count=0;
    const auto untouched=empty.spr;
    check(empty.run() && empty.state.address==0x110049e0 && empty.spr==untouched && empty.state.control==1);
}
void refusedBoundaries() {
    for (auto address:{0x110049e1u,0x11003ff0u,0x11005000u,0x11008000u,0x910049e0u,0x8000u}) {
        Rig r; r.state.address=address; const auto before=r.spr;
        bool refused=false; try { r.run(); } catch (const std::runtime_error&) { refused=true; }
        check(refused && r.spr==before && r.state.control==0x101 && r.state.count==0x60);
    }
    for (auto control:{0x100u,0x105u,0x109u,0x141u,0x181u}) {
        Rig r; r.state.control=control; const auto before=r.spr;
        bool refused=false; try { r.run(); } catch (const std::runtime_error&) { refused=true; }
        check(refused && r.spr==before && r.state.control==control);
    }
    Rig crossing; crossing.state.address=0x11004ff0; crossing.state.count=2;
    const auto before=crossing.spr;
    bool refused=false; try { crossing.run(); } catch (const std::runtime_error&) { refused=true; }
    check(refused && crossing.spr==before && crossing.state.address==0x11004ff0);
}
}
int main() {
    try {
        measuredTransferAndSharedSource(); deferredStartAndWrap(); refusedBoundaries();
        std::cout << "VU0-data toSPR behavioral checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
