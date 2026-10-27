#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>

namespace drakengard {
// Observation only. The interpreter owns the code buffer during run/resume;
// nested calls restore the previous context. Sampling never changes VU state.
struct MicroVf0Context {
    const uint8_t* code{};
    uint32_t bytes{};
    unsigned unit{};
    bool lower{};
};
inline thread_local MicroVf0Context microVf0Context;
struct MicroVf0Key {
    uint32_t pc{}, upper{}, lower{}, dest{}, changed{};
    bool lowerPhase{};
    bool operator==(const MicroVf0Key&) const = default;
};
struct MicroVf0Trace {
    static constexpr unsigned Budget = 32;
    std::mutex mutex;
    std::ofstream file;
    std::array<std::array<MicroVf0Key,Budget>,2> seen{};
    std::array<unsigned,2> count{};
    std::array<bool,2> activated{};
    MicroVf0Trace() {
        const char* path=std::getenv("DRAKENGARD_MICRO_VF0_TRACE");
        if (!path || !*path) return;
        if (std::filesystem::exists(path))
            throw std::runtime_error("Micro VF0 observation output already exists");
        file.open(path);
        if (!file) throw std::runtime_error("Cannot open micro VF0 observation output");
    }
};
inline MicroVf0Trace& microVf0Trace() { static MicroVf0Trace store; return store; }
class MicroVf0RunScope {
    MicroVf0Context previous=microVf0Context;
public:
    MicroVf0RunScope(unsigned unit,const uint8_t* code,uint32_t bytes) {
        auto& store=microVf0Trace();
        if (!store.file.is_open()) { microVf0Context={}; return; }
        if (unit>1) throw std::runtime_error("Unsupported micro VU unit identity");
        microVf0Context={code,bytes,unit,false};
        std::lock_guard lock(store.mutex);
        if (!store.activated[unit]) {
            store.file << "{\"kind\":\"activated\",\"unit\":" << unit
                       << ",\"code_bytes\":" << bytes
                       << ",\"unique_pair_budget\":" << MicroVf0Trace::Budget << "}\n";
            store.file.flush();
            if (!store.file) throw std::runtime_error("Cannot write micro VF0 observation");
            store.activated[unit]=true;
        }
    }
    ~MicroVf0RunScope() { microVf0Context=previous; }
};
class MicroVf0LowerScope {
    bool previous=microVf0Context.lower;
public:
    MicroVf0LowerScope() { microVf0Context.lower=true; }
    ~MicroVf0LowerScope() { microVf0Context.lower=previous; }
};

template<class LowerReads>
void observeMicroVf0Discard(uint32_t pc,uint8_t dest,const float* result,LowerReads lowerReads) {
    const auto& context=microVf0Context;
    if (!context.code || !dest || pc>context.bytes || context.bytes-pc<8) return;
    MicroVf0Key key{pc,0,0,dest,0,context.lower};
    std::memcpy(&key.lower,context.code+pc,4);
    std::memcpy(&key.upper,context.code+pc+4,4);
    std::array<uint32_t,4> bits{};
    for(unsigned lane=0;lane<4;++lane) {
        const unsigned mask=1u<<(3u-lane);
        if (!(dest&mask)) continue; // Inactive result lanes may be uninitialized.
        bits[lane]=std::bit_cast<uint32_t>(result[lane]);
        if (bits[lane]!=(lane==3?0x3f800000u:0u)) key.changed|=mask;
    }
    auto& store=microVf0Trace(); std::lock_guard lock(store.mutex);
    auto& count=store.count[context.unit];
    for(unsigned i=0;i<count;++i) if(store.seen[context.unit][i]==key) return;
    if(count==MicroVf0Trace::Budget) return;
    store.seen[context.unit][count++]=key;
    // I-bit replaces the lower instruction with a scalar immediate.
    const auto reads=(key.upper&0x80000000u)?0u:unsigned(lowerReads(key.lower));
    store.file << "{\"kind\":\"discard\",\"unit\":" << context.unit
               << ",\"pc_bytes\":" << pc << ",\"upper\":" << key.upper
               << ",\"lower\":" << key.lower << ",\"lower_phase\":" << (context.lower?"true":"false")
               << ",\"dest_lanes\":" << unsigned(dest) << ",\"changed_lanes\":" << key.changed
               << ",\"lower_vf0_read_lanes\":" << reads
               << ",\"would_affect_lower\":" << (!context.lower && (key.changed&reads)?"true":"false")
               << ",\"attempt_bits\":[";
    for(unsigned i=0;i<4;++i) { if(i)store.file<<',';store.file<<bits[i]; }
    store.file << "]}\n"; store.file.flush();
    if (!store.file) throw std::runtime_error("Cannot write micro VF0 observation");
}
}
