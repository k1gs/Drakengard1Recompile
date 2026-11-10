#pragma once
#include "host_audio_queue.h"
#include <memory>

namespace drakengard {
// Main-thread owner. Construct after runtime audio initialization; destroy
// before PS2Runtime closes its device. Only the queue crosses thread boundaries.
class HostAudioOutput {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    HostAudioOutput();
    ~HostAudioOutput();
    HostAudioOutput(const HostAudioOutput&)=delete;
    HostAudioOutput& operator=(const HostAudioOutput&)=delete;
    HostAudioQueue<>::Stats stats() const noexcept;
    void reportProgress(); // Main thread only; bounded milestones, not per-sample logging.
};
}
