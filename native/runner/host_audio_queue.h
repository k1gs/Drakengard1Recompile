#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace drakengard {
// One EE producer, one host audio consumer. No blocking, allocation, guest RAM,
// clock adjustment or sample repetition. Dropped host presentation frames do
// not change SPU execution; counters distinguish them from consumed PCM.
template<size_t Capacity=16384>
class HostAudioQueue {
    static_assert(Capacity>=2 && !(Capacity&(Capacity-1)));
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    using Frame=std::array<int16_t,2>;
    std::array<Frame,Capacity> frames{};
    alignas(64) std::atomic<uint64_t> written{};
    alignas(64) std::atomic<uint64_t> read{};
    std::atomic<bool> accepting{true};
    std::atomic<uint64_t> produced{},consumed{},nonzeroProduced{},nonzeroConsumed{},overflow{},underrun{},closedWrites{};
public:
    static constexpr unsigned SampleRate=48000;
    struct Stats {
        uint64_t produced,consumed,nonzeroProduced,nonzeroConsumed,overflow,underrun,closedWrites,pending;
    };
    bool push(std::array<int32_t,2> pcm) noexcept {
        if(!accepting.load(std::memory_order_acquire)) {
            closedWrites.fetch_add(1,std::memory_order_relaxed);return false;
        }
        produced.fetch_add(1,std::memory_order_relaxed);
        if(pcm[0] || pcm[1]) nonzeroProduced.fetch_add(1,std::memory_order_relaxed);
        const auto w=written.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)==Capacity) {
            overflow.fetch_add(1,std::memory_order_relaxed);return false; // drop newest, keep queued order
        }
        frames[w&(Capacity-1)]={int16_t(std::clamp(pcm[0],-32768,32767)),int16_t(std::clamp(pcm[1],-32768,32767))};
        written.store(w+1,std::memory_order_release);return true;
    }
    // destination contains interleaved PCM16 stereo. Padding is host underrun
    // silence, never counted as a produced or consumed guest sample.
    size_t consume(int16_t* destination,size_t requested) noexcept {
        if(!requested) return 0;
        const auto r=read.load(std::memory_order_relaxed);
        const size_t take=size_t(std::min<uint64_t>(requested,written.load(std::memory_order_acquire)-r));
        uint64_t nonzero=0;
        for(size_t i=0;i<take;++i) {
            const auto& f=frames[(r+i)&(Capacity-1)];
            destination[i*2]=f[0];destination[i*2+1]=f[1];nonzero+=f[0]!=0 || f[1]!=0;
        }
        for(size_t i=take;i<requested;++i) destination[i*2]=destination[i*2+1]=0;
        read.store(r+take,std::memory_order_release);
        consumed.fetch_add(take,std::memory_order_relaxed);
        nonzeroConsumed.fetch_add(nonzero,std::memory_order_relaxed);
        underrun.fetch_add(requested-take,std::memory_order_relaxed);
        return take;
    }
    // Disconnect the producer before closing during normal shutdown. A queue
    // remains alive until the backend has finished all in-flight callbacks.
    void close() noexcept { accepting.store(false,std::memory_order_release); }
    Stats stats() const noexcept {
        const auto r=read.load(std::memory_order_acquire);
        const auto w=written.load(std::memory_order_acquire);
        return {produced.load(),consumed.load(),nonzeroProduced.load(),nonzeroConsumed.load(),
                overflow.load(),underrun.load(),closedWrites.load(),w-r};
    }
};
}
