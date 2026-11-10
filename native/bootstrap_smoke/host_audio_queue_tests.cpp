#include "host_audio_queue.h"
#include <iostream>
#include <stdexcept>
#include <thread>

void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
int main() {
    try {
        drakengard::HostAudioQueue<4> queue;
        for(int i=0;i<4;++i) check(queue.push({i+1,-i-1}),"Queue lost usable capacity");
        check(!queue.push({99,99}),"Overflow overwrote queued PCM");
        std::array<int16_t,16> out{};
        check(queue.consume(out.data(),3)==3 && out[0]==1 && out[5]==-3,"FIFO order differs");
        queue.push({5,-5});queue.push({50000,-50000});
        check(queue.consume(out.data(),5)==3,"Wraparound lost frames");
        check(out[0]==4 && out[2]==5 && out[4]==32767 && out[5]==-32768,"Stereo order or PCM16 clipping differs");
        check(out[6]==0 && out[7]==0 && out[8]==0 && out[9]==0,"Underrun repeated stale samples");
        check(queue.consume(out.data(),2)==0 && out[0]==0 && out[1]==0,"Empty callback did not produce host silence");
        queue.close();check(!queue.push({1,2}),"Closed queue admitted producer writes");
        const auto stats=queue.stats();
        check(stats.produced==7 && stats.consumed==6 && stats.overflow==1 && stats.underrun==4 &&
              stats.closedWrites==1 && stats.pending==0,"Presentation loss counters are inconsistent");
        drakengard::HostAudioQueue<256> concurrent;
        std::atomic<bool> done{};bool ordered=true;
        std::thread producer([&] {
            for(int i=1;i<=20000;++i) {concurrent.push({i,-i});if(!(i&63)) std::this_thread::yield();}
            done.store(true,std::memory_order_release);
        });
        std::thread consumer([&] {
            int previous=0;std::array<int16_t,128> buffer{};
            while(!done.load(std::memory_order_acquire) || concurrent.stats().pending) {
                const auto n=concurrent.consume(buffer.data(),64);
                for(size_t i=0;i<n;++i) {
                    ordered=ordered && buffer[i*2]>previous && buffer[i*2+1]==-buffer[i*2];previous=buffer[i*2];
                }
                for(size_t i=n*2;i<buffer.size();++i) ordered=ordered && buffer[i]==0;
                if(!n) std::this_thread::yield();
            }
        });
        producer.join();consumer.join();const auto s=concurrent.stats();
        check(ordered && s.produced==20000 && s.consumed+s.overflow==20000 && s.pending==0 &&
              s.nonzeroConsumed==s.consumed,"Concurrent queue lost conservation or sample order");
        std::cout<<"PASS PCM order/clipping, wraparound, underrun/overflow, close and SPSC conservation\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
