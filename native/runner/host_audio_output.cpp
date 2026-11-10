#include "host_audio_output.h"
#include "iop_bridge.h"
#include "raylib.h"
#include <iostream>
#include <stdexcept>

namespace drakengard {
namespace {
std::atomic<HostAudioQueue<>*> callbackQueue{};
void audioCallback(void* buffer,unsigned frames) noexcept {
    if(!buffer || !frames) return;
    auto* pcm=static_cast<int16_t*>(buffer);
    if(auto* queue=callbackQueue.load(std::memory_order_acquire)) queue->consume(pcm,frames);
    else for(size_t i=0;i<size_t(frames)*2;++i) pcm[i]=0;
}
}
struct HostAudioOutput::Impl {
    // A copied producer closure can keep the closed queue alive during error
    // cleanup; it never retains this object or a backend stream pointer.
    std::shared_ptr<HostAudioQueue<>> queue=std::make_shared<HostAudioQueue<>>();
    AudioStream stream{};
    bool loaded{},ownsCallback{},callbackInstalled{},sourceBound{};
    bool reportedProduction{};
    uint64_t nextNonzeroReport{1};
    Impl() {
        if(!IsAudioDeviceReady()) throw std::runtime_error("SPU host audio device is unavailable");
        try {
            stream=LoadAudioStream(HostAudioQueue<>::SampleRate,16,2);loaded=stream.buffer!=nullptr;
            if(!IsAudioStreamValid(stream)) throw std::runtime_error("Cannot create SPU PCM16 stereo stream");
            HostAudioQueue<>* empty=nullptr;
            if(!callbackQueue.compare_exchange_strong(empty,queue.get()))
                throw std::runtime_error("SPU host audio already has an output owner");
            ownsCallback=true;
            SetAudioStreamCallback(stream,audioCallback);callbackInstalled=true;
            setCooperativeAudioSink([shared=queue](std::array<int32_t,2> pcm) noexcept { shared->push(pcm); });
            sourceBound=true;
            PlayAudioStream(stream);
            if(!IsAudioStreamPlaying(stream)) throw std::runtime_error("Cannot start SPU host audio stream");
            std::cerr<<"[host-spu-audio] rate=48000 sample_bits=16 channels=2 capacity_frames=16384\n";
        } catch(...) { close();throw; }
    }
    void close() noexcept {
        if(sourceBound) {
            try {setCooperativeAudioSink({});}
            catch(const std::exception& e) {std::cerr<<"[host-spu-audio-detach-error] "<<e.what()<<'\n';}
            sourceBound=false;
        }
        queue->close();
        if(loaded) {
            StopAudioStream(stream);
            // In the pinned raylib backend both device callbacks and callback
            // replacement hold AUDIO.System.lock. This is the in-flight barrier:
            // after it returns no callback can still use the borrowed queue.
            if(callbackInstalled) {SetAudioStreamCallback(stream,nullptr);callbackInstalled=false;}
        }
        if(ownsCallback) {callbackQueue.store(nullptr,std::memory_order_release);ownsCallback=false;}
        if(loaded) {UnloadAudioStream(stream);loaded=false;}
    }
    ~Impl() {
        close();
        const auto s=queue->stats();
        std::cerr<<"[host-spu-audio-result] produced="<<s.produced<<" callback_consumed="<<s.consumed
            <<" nonzero_produced="<<s.nonzeroProduced<<" nonzero_consumed="<<s.nonzeroConsumed
            <<" overflow_dropped="<<s.overflow<<" underrun_silence="<<s.underrun
            <<" writes_after_close="<<s.closedWrites<<" pending_at_stop="<<s.pending<<'\n';
    }
};
HostAudioOutput::HostAudioOutput():impl(std::make_unique<Impl>()) {}
HostAudioOutput::~HostAudioOutput()=default;
HostAudioQueue<>::Stats HostAudioOutput::stats() const noexcept {return impl->queue->stats();}
void HostAudioOutput::reportProgress() {
    const auto s=impl->queue->stats();
    if((!impl->reportedProduction && s.produced) || s.nonzeroConsumed>=impl->nextNonzeroReport) {
        impl->reportedProduction=true;
        if(s.nonzeroConsumed) impl->nextNonzeroReport=s.nonzeroConsumed>UINT64_MAX/2?UINT64_MAX:s.nonzeroConsumed*2;
        std::cerr<<"[host-spu-audio-progress] produced="<<s.produced<<" callback_consumed="<<s.consumed
            <<" nonzero_produced="<<s.nonzeroProduced<<" nonzero_consumed="<<s.nonzeroConsumed
            <<" overflow_dropped="<<s.overflow<<" underrun_silence="<<s.underrun<<'\n';
    }
}
}
