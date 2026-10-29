#include "ee_body_probe.h"
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
        for(auto stage:{0x19dda8u,0x19de48u,0x19de5cu}) {
            state.stage=state.pc=stage;
            for(unsigned i=0;i<20;++i)probe.observe(ram.data(),state,output.c_str());
        }
        check(ram==before);
        std::ifstream input(output);std::string data((std::istreambuf_iterator<char>(input)),{});
        check(std::count(data.begin(),data.end(),'\n')==12);
        check(data.find("\"output_prefix_bits\":[1065353216,")!=std::string::npos);
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
        std::filesystem::remove(directory);
        std::cout<<"Body observation bounds, filtering, null result and read-only contract passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
