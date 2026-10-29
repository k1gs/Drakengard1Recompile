#include "ee_body_probe.h"
#include "ee_geometry_probe.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <vector>

namespace {
void check(bool value) { if(!value)throw std::runtime_error("Body observation regression"); }
void store(std::vector<uint8_t>& ram,uint32_t address,uint32_t value) {
    std::memcpy(ram.data()+address,&value,4);
}
}
int main() {
    std::filesystem::path directory;
    try {
        std::vector<uint8_t> ram(0x2000000);
        const uint32_t descriptor=0x1000,sp=0x2000,resource=0x4000;
        store(ram,descriptor,0x3b0620);store(ram,descriptor+4,1);
        store(ram,descriptor+12,0x3b0700);store(ram,0x4fbd30,4);
        store(ram,sp,0x3f800000);store(ram,resource,0x12345678);
        store(ram,resource+0xc8,0x5000);store(ram,0x5024,18);
        store(ram,sp+17*64,0x3f012345);
        const auto before=ram;
        for(unsigned i=0;i<100;++i) {
            auto candidate=std::filesystem::temp_directory_path()/
                ("d1-body-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) { directory=candidate;break; }
        }
        check(!directory.empty());const auto output=(directory/"trace.jsonl").string();
        drakengard::EeBodyProbe probe;
        // Disabled diagnostics may not dereference guest pointers.
        probe.observe(nullptr,{},nullptr);
        auto state=drakengard::EeBodyProbeState{0x19dda8,0x19dda8,descriptor,sp,resource,resource};
        // Other models cannot consume the Caim history bound.
        store(ram,descriptor+4,2);
        for(unsigned i=0;i<20;++i)probe.observe(ram.data(),state,output.c_str());
        check(!std::filesystem::exists(output));store(ram,descriptor+4,1);
        for(auto fade:{1u,0u}) {
            store(ram,0x48a42c,fade);
            for(auto stage:{0x19dda8u,0x19de48u,0x19de5cu}) {
                state.stage=state.pc=stage;
                for(unsigned i=0;i<20;++i)probe.observe(ram.data(),state,output.c_str());
            }
        }
        check(ram==before);
        std::ifstream input(output);std::string data((std::istreambuf_iterator<char>(input)),{});
        check(std::count(data.begin(),data.end(),'\n')==12);
        check(data.find("\"output_matrix_bits\":[1065353216,")!=std::string::npos);
        const auto matrixStart=data.find("\"output_matrix_bits\":[")+22;
        const auto matrixEnd=data.find(']',matrixStart);
        check(std::count(data.begin()+matrixStart,data.begin()+matrixEnd,',')==18*16-1);
        check(data.substr(matrixStart,matrixEnd-matrixStart).find(std::to_string(0x3f012345))!=std::string::npos);
        check(data.find("\"bone_count\":18")!=std::string::npos);
        check(data.find("\"resource_prefix_words\":[305419896,")!=std::string::npos);
        drakengard::EeBodyProbe invalid;
        bool refused=false;state.stage=0x19dda8;state.sp=0x1ffff00;
        try { invalid.observe(ram.data(),state,output.c_str()); }
        catch(const std::runtime_error&) { refused=true; }
        check(refused);
        // A genuine null preparation result remains observable, without reading
        // a fake resource or claiming that the guest preparation succeeded.
        state.sp=sp;state.returnedResource=0;invalid.observe(ram.data(),state,output.c_str());
        check(ram==before);
        input.close();std::filesystem::remove(directory/"trace.jsonl");

        const uint32_t geometrySp=0x8000,mesh=0x6000;
        store(ram,0x40e234,resource);store(ram,geometrySp+0x1f8,0x19de5c);
        store(ram,geometrySp+0x1b8,descriptor);store(ram,geometrySp+0x14,mesh);
        store(ram,geometrySp+0x190,sp);store(ram,mesh+8,1);
        const auto geometryBefore=ram;
        drakengard::EeGeometryProbe geometryProbe;
        geometryProbe.observe(nullptr,{},nullptr);
        auto geometry=drakengard::EeGeometryProbeState{0x29e1a0,0x29e1a0,geometrySp,resource,0,1};
        store(ram,geometrySp+0x1f8,0x1234);
        geometryProbe.observe(ram.data(),geometry,output.c_str());
        check(!std::filesystem::exists(output));store(ram,geometrySp+0x1f8,0x19de5c);
        for(auto fade:{1u,0u}) {
            store(ram,0x48a42c,fade);
            for(unsigned part=0;part<3;++part) {
                geometry.partOffset=part*16;store(ram,mesh+8,part==0?1:part==1?0x1800:8);
                for(auto stage:{0x29e1a0u,0x29e21cu}) {
                    geometry.stage=geometry.pc=stage;
                    for(unsigned i=0;i<20;++i)geometryProbe.observe(ram.data(),geometry,output.c_str());
                }
            }
        }
        store(ram,mesh+8,1);check(ram==geometryBefore);
        std::ifstream geometryInput(output);
        const std::string geometryData((std::istreambuf_iterator<char>(geometryInput)),{});
        check(std::count(geometryData.begin(),geometryData.end(),'\n')==12);
        const std::string chainKey="\"vif1_chain_prefix_words\"";
        auto first=geometryData.find(chainKey);check(first!=std::string::npos);
        auto second=geometryData.find(chainKey,first+1);check(second!=std::string::npos);
        check(geometryData.find(chainKey,second+1)==std::string::npos);
        geometryInput.close();std::filesystem::remove(directory/"trace.jsonl");
        std::filesystem::remove(directory);
        std::cout<<"Body/geometry read-only bounds, full skeleton and fade history passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
