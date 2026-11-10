#include "host_audio_output.h"
#include "iop_bridge.h"
#include "raylib.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
std::function<void(std::array<int32_t,2>)> source;
struct Device {
    Device() {
        InitAudioDevice();
        if(!IsAudioDeviceReady()) throw std::runtime_error("Real audio output device unavailable");
        SetMasterVolume(0.0f); // This process's stream is intentionally inaudible.
    }
    ~Device() {CloseAudioDevice();}
};
}
namespace drakengard {
void setCooperativeAudioSink(std::function<void(std::array<int32_t,2>)> sink) {source=std::move(sink);}
}
int main() {
    try {
        Device device;
        {
            drakengard::HostAudioOutput output;
            for(int i=0;i<4800;++i) source({1000+i%100,-1000-i%100});
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            while(output.stats().consumed<4800 && std::chrono::steady_clock::now()<end)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            const auto s=output.stats();
            if(s.produced!=4800 || s.consumed!=4800 || s.nonzeroConsumed!=4800 || s.overflow || s.pending)
                throw std::runtime_error("Real backend did not consume the complete synthetic PCM sequence");
            std::cout<<"PASS real raylib device callback consumed4800/4800 nonzero synthetic frames; muted, NOT game audio proof\n";
        }
        if(source) throw std::runtime_error("Producer remained bound after output shutdown");
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
