#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <span>

namespace drakengard {
struct VifUnpackExample {
    uint32_t command{}, cycle{}, mode{};
    bool complete{};
    std::array<uint8_t,16> source{}, before{}, after{};
    unsigned sourceBytes{}, beforeBytes{}, afterBytes{};
};

// Observation only: raw STCYCL is preserved, never normalized or used to write VU memory.
struct VifUnpackObservation {
    uint64_t calls{}, completeCommands{}, incompleteCommands{}, invalidCommands{};
    uint64_t clZeroCalls{}, wlZeroCalls{}, maskedCalls{}, unsignedCalls{};
    std::array<uint64_t,16> formatCalls{}, declaredVectors{};
    std::array<uint64_t,4> modeCalls{};
    std::array<std::array<uint64_t,4>,16> formatModeCalls{};
    std::array<VifUnpackExample,32> examples{};
    std::array<unsigned,16> examplesPerFormat{};
    std::size_t exampleCount{};

    static unsigned copyPrefix(std::array<uint8_t,16>& target,std::span<const uint8_t> source) {
        const std::size_t count=source.size()<target.size()?source.size():target.size();
        if(count)std::memcpy(target.data(),source.data(),count);
        return unsigned(count);
    }
    void observe(uint32_t command,uint32_t rawCycle,uint32_t mode,bool complete,
                 std::span<const uint8_t> source={},std::span<const uint8_t> before={},
                 std::span<const uint8_t> after={}) {
        const auto opcode=(command>>24)&0x7fu;
        if((opcode&0x60u)!=0x60u) { ++invalidCommands; return; }
        const auto format=opcode&0x0fu;
        const auto num=(command>>16)&0xffu;
        ++calls; ++formatCalls[format]; declaredVectors[format]+=num?num:256u;
        ++modeCalls[mode&3u];
        ++formatModeCalls[format][mode&3u];
        if(complete)++completeCommands; else ++incompleteCommands;
        if((rawCycle&0xffu)==0)++clZeroCalls;
        if((rawCycle&0xff00u)==0)++wlZeroCalls;
        if(opcode&0x10u)++maskedCalls;
        if(command&0x4000u)++unsignedCalls;
        if(examplesPerFormat[format]>=2) return;
        ++examplesPerFormat[format];
        auto& sample=examples[exampleCount++];
        sample={}; sample.command=command; sample.cycle=rawCycle; sample.mode=mode; sample.complete=complete;
        sample.sourceBytes=copyPrefix(sample.source,source);
        sample.beforeBytes=copyPrefix(sample.before,before);
        sample.afterBytes=copyPrefix(sample.after,after);
    }
    void clearExamples() { exampleCount=0; examplesPerFormat={}; }
    template<class Array> static void writeArray(std::ostream& output,const Array& values) {
        output<<'[';
        for(std::size_t i=0;i<values.size();++i) { if(i)output<<','; output<<uint64_t(values[i]); }
        output<<']';
    }
    void writeJson(std::ostream& output) const {
        output<<"{\"calls\":"<<calls<<",\"complete_commands\":"<<completeCommands
              <<",\"incomplete_commands\":"<<incompleteCommands<<",\"invalid_commands\":"<<invalidCommands
              <<",\"raw_cl_zero_calls\":"<<clZeroCalls<<",\"raw_wl_zero_calls\":"<<wlZeroCalls
              <<",\"masked_calls\":"<<maskedCalls<<",\"unsigned_calls\":"<<unsignedCalls
              <<",\"format_calls\":"; writeArray(output,formatCalls);
        output<<",\"declared_vectors\":"; writeArray(output,declaredVectors);
        output<<",\"mode_calls\":"; writeArray(output,modeCalls);
        output<<",\"format_mode_calls\":[";
        for(std::size_t i=0;i<formatModeCalls.size();++i) { if(i)output<<','; writeArray(output,formatModeCalls[i]); }
        output<<"],\"examples\":[";
        for(std::size_t i=0;i<exampleCount;++i) {
            const auto& s=examples[i]; if(i)output<<',';
            output<<"{\"command\":"<<s.command<<",\"raw_cycle\":"<<s.cycle<<",\"mode\":"<<s.mode
                  <<",\"complete\":"<<(s.complete?"true":"false")<<",\"source_bytes\":"<<s.sourceBytes
                  <<",\"before_bytes\":"<<s.beforeBytes<<",\"after_bytes\":"<<s.afterBytes
                  <<",\"source\":"; writeArray(output,s.source);
            output<<",\"before\":"; writeArray(output,s.before);
            output<<",\"after\":"; writeArray(output,s.after);
            output<<'}';
        }
        output<<"]}";
    }
};
}
