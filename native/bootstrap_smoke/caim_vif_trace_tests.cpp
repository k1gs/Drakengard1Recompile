#include "caim_vif_trace.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <vector>
namespace {
void check(bool value) { if(!value)throw std::runtime_error("Caim VIF identity regression"); }
void store(std::array<uint8_t,152>& data,unsigned offset,uint32_t value) { std::memcpy(data.data()+offset,&value,4); }
struct Registers { uint32_t cycle=0x101,mask=0,mode=0,row[4]{},col[4]{}; };
struct Batch {
    struct Vertex { float x{},y{},z{};uint8_t r{},g{},b{},a{}; };
    struct State { struct Prim { unsigned type=3; } prim; } state;
    std::array<Vertex,3> vertices{};
    uint8_t vertexCount=3;
};
}
int main() {
    std::filesystem::path directory;
    try {
        for(unsigned i=0;i<100;++i) {
            auto candidate=std::filesystem::temp_directory_path()/("d1-caim-vif-"+
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) { directory=candidate;break; }
        }
        check(!directory.empty());const auto output=(directory/"trace.jsonl").string();
        drakengard::CaimVifTrace trace;
        const std::array<uint32_t,12> tags{
            0x30000003,0x798a30,0,0x7908000e,
            0x30000003,0x798ee0,0,0x7908003e,
            2,0x799210,0,0x6e08406e};
        const auto tagBytes=reinterpret_cast<const uint8_t*>(tags.data());
        trace.boundary(true,0,0,nullptr);
        check(!trace.begin(0x10008000,0x665bc0,0x145,nullptr));
        std::array<uint8_t,152> stream{};
        store(stream,4,0x7908000e);store(stream,60,0x7908003e);store(stream,116,0x6e08406e);
        const auto original=stream;
        std::array<uint8_t,4096> vu{},code{};const auto vuOriginal=vu;
        Registers regs;
        for(unsigned phase=0;phase<2;++phase) {
            trace.boundary(true,1,phase,output.c_str());
            check(!trace.begin(0x10008000,0x665bc0,0x145,tagBytes));
            trace.boundary(true,0,phase,output.c_str());
            check(!trace.begin(0x1000a000,0x665bc0,0x145,tagBytes));
            check(!trace.begin(0x10008000,0x665bd0,0x145,tagBytes));
            auto wrongTags=tags;wrongTags[1]^=16;
            check(!trace.begin(0x10008000,0x665bc0,0x145,
                               reinterpret_cast<const uint8_t*>(wrongTags.data())));
            check(trace.begin(0x10008000,0x665bc0,0x145,tagBytes));
            auto different=stream;different[4]=0;
            trace.queue(true,different.data(),different.size());
            check(!trace.enter(different.data(),different.size(),0));
            check(trace.begin(0x10008000,0x665bc0,0x145,tagBytes));
            trace.queue(true,stream.data(),stream.size());
            const auto copy=stream;
            check(!trace.enter(copy.data(),copy.size(),0));
            check(!trace.enter(stream.data(),stream.size(),1));
            check(trace.enter(stream.data(),stream.size(),0));
            trace.unpack(false,0x7908000e,regs,stream.data(),48,vu.data(),vu.size(),true);
            trace.unpack(true,0x7908000e,regs,stream.data(),48,vu.data(),vu.size(),true);
            trace.leave(true);check(!trace.active());check(trace.following());
            check(!trace.enter(stream.data(),stream.size(),0));
            for(unsigned i=0;i<8;++i) {
                trace.micro(false,0,code.data(),vu.data(),8);trace.micro(true,0,code.data(),vu.data(),0);
            }
            for(unsigned i=0;i<20;++i)trace.dma(0x1000d000,0x100,0x2000,8,0,0);
            check(!trace.following());
            trace.boundary(false,0,phase,output.c_str());
            trace.boundary(true,0,phase,output.c_str());
            check(!trace.begin(0x10008000,0x665bc0,0x145,tagBytes));
        }
        check(stream==original && vu==vuOriginal);
        std::ifstream input(output);std::string text((std::istreambuf_iterator<char>(input)),{});
        // Later calls cannot exhaust the other fade phase or grow the finite DMA history.
        check(std::count(text.begin(),text.end(),'\n')==50);
        input.close();
        drakengard::CaimVifTrace provenance;
        provenance.boundary(true,0,0,output.c_str());
        check(provenance.begin(0x10008000,0x665bc0,0x145,tagBytes));
        provenance.queue(true,stream.data(),stream.size());
        check(provenance.enter(stream.data(),stream.size(),0));
        provenance.leave(true);
        provenance.micro(false,0,code.data(),vu.data(),8);
        provenance.micro(true,0,code.data(),vu.data(),0);
        std::array<uint8_t,128> payload{},packet{},copiedPacket{};
        payload.fill(0x42);packet=payload;copiedPacket=packet;
        Batch batch;
        provenance.output(0x2000,payload.data(),payload.size(),0);
        auto wrong=payload;wrong[17]^=1;
        provenance.begin(0x1000a000,0x1000,0x105,nullptr);
        provenance.outputSource(0x1000a000,0x2000,wrong.size(),wrong.data(),0);
        provenance.queueOutput(0x1000a000,packet.data(),packet.size());
        auto previous=provenance.enterGif(packet.data(),packet.size());
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        provenance.output(0x2000,payload.data(),payload.size(),0);
        provenance.outputSource(0x1000a000,0x2000,payload.size(),payload.data(),0);
        provenance.queueOutput(0x1000a000,packet.data(),packet.size());
        // A byte-identical copy has no provenance until the actual queue relay.
        previous=provenance.enterGif(copiedPacket.data(),copiedPacket.size());
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        provenance.relayGif(2,packet.data(),packet.size(),copiedPacket.data());
        previous=provenance.enterGif(copiedPacket.data(),copiedPacket.size());
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        provenance.relayGif(3,packet.data(),packet.size(),copiedPacket.data());
        previous=provenance.enterGif(copiedPacket.data(),copiedPacket.size());
        for(unsigned i=0;i<8;++i)check(provenance.draw(batch));
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        // The consumed pointer cannot confer identity on a reused allocation.
        previous=provenance.enterGif(copiedPacket.data(),copiedPacket.size());
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        check(payload==packet && packet==copiedPacket);
        provenance.output(0x2000,payload.data(),payload.size(),0);
        provenance.outputSource(0x10009000,0x2000,payload.size(),payload.data(),0);
        provenance.queueOutput(0x10009000,packet.data(),packet.size());
        std::array<uint8_t,16384> vu1{},vu1Code{};
        auto outer=provenance.enterOutputVif(copiedPacket.data(),copiedPacket.size());
        provenance.vu1(false,0x14000000,0,0,vu1Code.data(),vu1.data());
        provenance.kick(8,0,packet.data(),packet.size());
        previous=provenance.enterGif(packet.data(),packet.size());
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        provenance.leaveOutputVif(outer);
        outer=provenance.enterOutputVif(packet.data(),packet.size());
        // A setup microprogram before the target UNPACK cannot inherit Caim.
        provenance.vu1(false,0x14000000,0,0,vu1Code.data(),vu1.data());
        provenance.kick(8,0,packet.data(),packet.size());
        previous=provenance.enterGif(packet.data(),packet.size());
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        provenance.unpack(false,0x6c080000,regs,packet.data(),packet.size(),vu1.data(),vu1.size(),true);
        provenance.unpack(true,0x6c080000,regs,packet.data(),packet.size(),vu1.data(),vu1.size(),true);
        provenance.vu1(false,0x14000000,0,0,vu1Code.data(),vu1.data());
        provenance.kick(8,0,packet.data(),packet.size());
        provenance.relayGif(3,packet.data(),packet.size(),copiedPacket.data());
        previous=provenance.enterGif(copiedPacket.data(),copiedPacket.size());
        check(!provenance.draw(batch));provenance.leaveGif(previous);
        provenance.relayGif(1,packet.data(),packet.size(),copiedPacket.data());
        previous=provenance.enterGif(copiedPacket.data(),copiedPacket.size());
        check(provenance.draw(batch));provenance.leaveGif(previous);
        provenance.vu1(true,0x14000000,0,0,vu1Code.data(),vu1.data());
        provenance.leaveOutputVif(outer);
        std::filesystem::remove_all(directory);
        std::cout<<"Caim producer/queue/channel identity, fade reservation and read-only bounds passed\n";
    } catch(const std::exception& error) {
        if(!directory.empty()) { std::error_code ignored;std::filesystem::remove_all(directory,ignored); }
        std::cerr<<error.what()<<'\n';return 1;
    }
}
