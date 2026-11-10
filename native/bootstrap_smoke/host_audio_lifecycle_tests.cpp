#include "host_audio_output.h"
#include "iop_bridge.h"
#include "raylib.h"
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Stream {AudioCallback callback{};bool playing{};};
std::map<void*,Stream> streams;std::mutex backendLock;
std::function<void(std::array<int32_t,2>)> producer;
std::vector<std::string> events;
uintptr_t nextId{};bool deviceReady=true,loadFails{},playFails{},bindFails{};
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
void reset() {
    check(streams.empty() && !producer,"Stream or producer survived cleanup");
    events.clear();deviceReady=true;loadFails=playFails=bindFails=false;
}
template<class F> void rejects(F f) {bool failed=false;try {f();} catch(const std::runtime_error&) {failed=true;}check(failed,"Backend failure was hidden");}
}
bool IsAudioDeviceReady() {return deviceReady;}
AudioStream LoadAudioStream(unsigned rate,unsigned bits,unsigned channels) {
    check(rate==48000 && bits==16 && channels==2,"Wrong PCM format/rate");
    if(loadFails) return {};
    auto* id=reinterpret_cast<void*>(++nextId);streams[id]={};events.push_back("load");return {id,rate,bits,channels};
}
bool IsAudioStreamValid(AudioStream s) {return s.buffer!=nullptr;}
void UnloadAudioStream(AudioStream s) {std::lock_guard lock(backendLock);check(!streams.at(s.buffer).callback,"Freed a live callback");streams.erase(s.buffer);events.push_back("unload");}
void SetAudioStreamCallback(AudioStream s,AudioCallback callback) {std::lock_guard lock(backendLock);streams.at(s.buffer).callback=callback;events.push_back(callback?"callback_on":"callback_off");}
void PlayAudioStream(AudioStream s) {std::lock_guard lock(backendLock);streams.at(s.buffer).playing=!playFails;events.push_back("play");}
void StopAudioStream(AudioStream s) {std::lock_guard lock(backendLock);streams.at(s.buffer).playing=false;events.push_back("stop");}
bool IsAudioStreamPlaying(AudioStream s) {std::lock_guard lock(backendLock);return streams.at(s.buffer).playing;}
namespace drakengard {
void setCooperativeAudioSink(std::function<void(std::array<int32_t,2>)> sink) {
    if(sink && bindFails) throw std::runtime_error("Synthetic producer bind failure");
    producer=std::move(sink);events.push_back(producer?"bind":"detach");
}
}
int main() {
    try {
        std::function<void(std::array<int32_t,2>)> retained;
        AudioCallback retiredCallback{};
        {
            drakengard::HostAudioOutput audio;
            retained=producer;retiredCallback=streams.begin()->second.callback;
            producer({1000,-1000});producer({40000,-40000});
            std::array<int16_t,8> buffer{};
            {std::lock_guard lock(backendLock);retiredCallback(buffer.data(),4);}
            check(buffer==std::array<int16_t,8>{1000,-1000,32767,-32768,0,0,0,0},"Callback changed PCM or padded with stale data");
            rejects([] {drakengard::HostAudioOutput duplicate;});
            producer({3,-3});
            {std::lock_guard lock(backendLock);retiredCallback(buffer.data(),1);}
            check(buffer[0]==3 && audio.stats().consumed==3,"Second owner damaged existing stream");
        }
        check(events.size()>=4 && std::vector<std::string>(events.end()-4,events.end())==
              std::vector<std::string>{"detach","stop","callback_off","unload"},"Wrong shutdown order");
        // A copied producer and retired callback cannot access a destroyed sink.
        retained({4,-4});std::array<int16_t,2> tail{99,99};retiredCallback(tail.data(),1);
        check(tail==std::array<int16_t,2>{},"Retired callback accessed freed output");retained={};
        reset();deviceReady=false;rejects([] {drakengard::HostAudioOutput audio;});
        reset();loadFails=true;rejects([] {drakengard::HostAudioOutput audio;});
        reset();bindFails=true;rejects([] {drakengard::HostAudioOutput audio;});
        reset();playFails=true;rejects([] {drakengard::HostAudioOutput audio;});
        reset();{drakengard::HostAudioOutput audio;}reset();
        std::cout<<"PASS actual sink PCM/lifecycle with synthetic backend, failures and owner exclusion\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
