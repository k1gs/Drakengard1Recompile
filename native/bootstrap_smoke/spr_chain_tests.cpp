#include "ee_spr_chain.h"
#include <array>
#include <iostream>
#include <vector>

namespace {
using drakengard::EeToSprChainState;
using drakengard::transferEeToSprChain;
void check(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Action> void refuses(Action action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("unsupported toSPR transfer completed");
}
struct Rig {
    std::vector<uint8_t> ram=std::vector<uint8_t>(0x20000);
    std::vector<uint8_t> scratch=std::vector<uint8_t>(0x4000,0xcd);
    EeToSprChainState state{0x105,0,0,0x100,0x70000000};
    void word(unsigned address,uint32_t value) {
        for (unsigned i=0;i<4;++i) ram[address+i]=uint8_t(value>>(8*i));
    }
    void tag(unsigned at,unsigned id,unsigned qwc,uint32_t address=0,bool irq=false) {
        word(at,(id<<28)|qwc|(irq?0x80000000u:0)); word(at+4,address);
        word(at+8,0x12345678); word(at+12,0x90abcdef);
    }
    void payload(unsigned at,unsigned bytes,unsigned seed) {
        for (unsigned i=0;i<bytes;++i) ram[at+i]=uint8_t(seed+i*17+(i>>8));
    }
    auto run(bool enabled=true) { return transferEeToSprChain(state,ram,scratch,enabled); }
};

void testReferencedPayloadAndCompletion() {
    Rig r;
    r.tag(0x100,3,2,0x1000); r.tag(0x110,0,1,0x2000);
    r.payload(0x1000,32,3); r.payload(0x2000,16,109);
    const auto original=r.ram;
    const auto result=r.run();
    check(result.completed && result.tags==2 && result.quadwords==3,"REF/REFE count mismatch");
    check(!(r.state.control&0x100) && r.state.count==0,"chain STR/QWC not completed");
    check(r.state.address==0x2010 && r.state.tag==0x120 && r.state.scratch==48,
          "REFE final MADR/TADR/SADR mismatch");
    for (unsigned i=0;i<r.scratch.size();++i)
        check(r.scratch[i]==(i<32?original[0x1000+i]:i<48?original[0x2000+i-32]:0xcd),
              "referenced payload/tag exclusion or scratch guard mismatch");
    check(r.ram==original,"toSPR changed source RAM");
    const auto again=r.run();
    check(!again.completed && again.tags==0 && again.quadwords==0,"completion edge repeated");
}

void testInlineCntNextEnd() {
    Rig r;
    r.tag(0x100,1,2); r.payload(0x110,32,5);
    r.tag(0x130,2,1,0x500); r.payload(0x140,16,71);
    r.tag(0x500,7,1); r.payload(0x510,16,131);
    const auto result=r.run();
    check(result.completed && result.tags==3 && result.quadwords==4,"CNT/NEXT/END traversal mismatch");
    check(r.state.address==0x520 && r.state.tag==0x500 && r.state.scratch==64,
          "END must retain tag address and advance MADR past payload");
    check(r.state.control==0x70000005u,"last tag was not published in upper CHCR");
    for (unsigned i=0;i<64;++i) {
        const unsigned source=i<32?0x110+i:i<48?0x140+i-32:0x510+i-48;
        check(r.scratch[i]==r.ram[source],"inline chain selected wrong data or next tag");
    }
    check(r.scratch[64]==0xcd,"inline chain wrote past payload");
}

void testTagTransferAndIrqStop() {
    Rig r;
    r.state.control=0x1c5; // TTE + TIE
    r.tag(0x100,3,1,0x1000,true);
    r.tag(0x110,7,1); r.payload(0x120,16,91); r.payload(0x1000,16,37);
    const auto result=r.run();
    check(result.completed && result.tags==1 && result.quadwords==2,"TIE did not stop after IRQ payload");
    check(r.state.tag==0x110 && r.state.address==0x1010 && r.state.scratch==32,
          "IRQ stop pointers mismatch");
    for (unsigned i=0;i<32;++i)
        check(r.scratch[i]==r.ram[i<16?0x100+i:0x1000+i-16],"TTE did not copy all128 tag bits before payload");
    check(r.scratch[32]==0xcd,"TIE copied a later tag/payload");

    Rig noTie;
    noTie.tag(0x100,3,0,0,true); noTie.tag(0x110,7,1); noTie.payload(0x120,16,119);
    const auto ignored=noTie.run();
    check(ignored.tags==2 && ignored.completed,"tag IRQ incorrectly terminated a TIE=0 chain");
    for (unsigned i=0;i<16;++i) check(noTie.scratch[i]==noTie.ram[0x120+i],"IRQ without TIE lost following data");
}

void testRingWrapAndDeferredStart() {
    Rig r;
    r.state.scratch=0x70003ff0;
    r.tag(0x100,0,0x805,0x1000); // more than two full scratchpad revolutions
    r.payload(0x1000,0x8050,53);
    const auto before=r.scratch;
    const auto blocked=r.run(false);
    check(!blocked.completed && blocked.tags==0 && r.scratch==before && r.state.control==0x105 &&
          r.state.scratch==0x70003ff0 && r.state.tag==0x100,"disabled DMA consumed state/data");
    const auto result=r.run();
    check(result.completed && result.quadwords==0x805 && r.state.scratch==0x40,
          "ring-wrap QWC/SADR mismatch");
    auto expected=before;
    for (unsigned i=0;i<0x8050;++i) expected[(0x3ff0+i)&0x3fff]=r.ram[0x1000+i];
    check(r.scratch==expected,"scratch wrap/multiple revolutions corrupted bytes");
    check(r.state.address==0x9050 && r.state.tag==0x110,"wrapped transfer final registers mismatch");
}

void testZeroCountFetchAndUnsupportedBoundaries() {
    Rig empty;
    empty.tag(0x100,1,0); empty.tag(0x110,7,0);
    const auto prior=empty.scratch;
    const auto result=empty.run();
    check(result.completed && result.tags==2 && result.quadwords==0 && empty.scratch==prior,
          "zero-QWC source chain fabricated payload instead of fetching tags");
    for (unsigned id:{4u,5u,6u}) {
        Rig r; r.tag(0x100,id,1,0x1000);
        refuses([&] { r.run(); });
        check((r.state.control&0x100) && r.scratch==prior,"unsupported tag fabricated completion/output");
    }
    for (uint32_t source:{0x80001000u,0x70000000u,0x11000000u,0x1001u,0x1fff0u}) {
        Rig r; r.tag(0x100,0,2,source);
        refuses([&] { r.run(); });
        check((r.state.control&0x100) && r.scratch==prior,"invalid source aliased RAM or wrote partial payload");
    }
    Rig badTag; badTag.state.tag=0x1fff0; badTag.tag(0x1fff0,1,1);
    refuses([&] { badTag.run(); });
    check(badTag.scratch==prior && (badTag.state.control&0x100),"inline out-of-range payload modified scratch");
    Rig restarted; restarted.state.count=1;
    refuses([&] { restarted.run(); });
    check((restarted.state.control&0x100) && restarted.scratch==prior,"nonzero-QWC restart silently completed");
    Rig cycle; cycle.tag(0x100,2,0,0x100);
    refuses([&] { cycle.run(); });
    check((cycle.state.control&0x100) && cycle.scratch==prior,"cyclic chain fabricated completion");
}
}
int main() {
    try {
        testReferencedPayloadAndCompletion(); testInlineCntNextEnd();
        testTagTransferAndIrqStop(); testRingWrapAndDeferredStart();
        testZeroCountFetchAndUnsupportedBoundaries();
        std::cout << "toSPR source-chain behavior passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
