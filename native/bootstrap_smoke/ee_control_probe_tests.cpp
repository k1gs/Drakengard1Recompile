#include "ee_control_probe.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <vector>
namespace {
void check(bool condition) { if(!condition)throw std::runtime_error("Control observation regression"); }
void put(std::vector<uint8_t>& ram,uint32_t address,uint32_t value) { std::memcpy(ram.data()+address,&value,4); }
}
int main() {
    std::filesystem::path directory;
    try {
        for(unsigned i=0;i<100;++i) {
            auto candidate=std::filesystem::temp_directory_path()/("d1-control-"+
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) { directory=candidate;break; }
        }
        check(!directory.empty());const auto path=(directory/"trace.jsonl").string();
        drakengard::EeControlProbe probe;
        const drakengard::EeControlProbeState state{0x143698,0x143698,0x3b4ce4,3,7,0x140924,0x1000};
        probe.observe(nullptr,state,nullptr);
        std::vector<uint8_t> ram(0x2000000);
        put(ram,0x3b4ce4,0x12345678);
        for(unsigned phase=0;phase<3;++phase) {
            put(ram,0x4fbd30,phase?4:0);put(ram,0x48a42c,phase==1?255:0);
            const auto before=ram;
            for(unsigned call=0;call<100;++call) {
                auto s=state;s.a1=call;probe.observe(ram.data(),s,path.c_str());
            }
            check(ram==before);
        }
        std::ifstream input(path);std::string text((std::istreambuf_iterator<char>(input)),{});input.close();
        check(std::count(text.begin(),text.end(),'\n')==72);
        check(text.find("\"phase\":2")!=std::string::npos);
        check(text.find("\"argument_words\":[305419896")!=std::string::npos);
        // Unknown boundaries and invalid stack ranges fail before emitting data.
        drakengard::EeControlProbe strict;
        auto invalid=state;invalid.point=0;
        bool refused=false;try { strict.observe(ram.data(),invalid,path.c_str()); }catch(const std::runtime_error&) { refused=true; }check(refused);
        invalid=state;invalid.sp=0x1fffff0;refused=false;
        try { strict.observe(ram.data(),invalid,path.c_str()); }catch(const std::runtime_error&) { refused=true; }check(refused);
        std::filesystem::remove(directory/"trace.jsonl");std::filesystem::remove(directory);
        std::cout<<"Control observation preserves RAM, actual embedded argument, phase history and bounded output\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
