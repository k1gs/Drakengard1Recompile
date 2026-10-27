#include "gs_path1_capture.h"
#include <chrono>
#include <iostream>
#include <vector>

void check(bool value) { if(!value)throw std::runtime_error("PATH1 capture regression"); }
int main() {
    try {
        drakengard::GsKickOrigins origins;
        std::array<uint8_t,16> producer{},a{},b{};
        origins.offer={producer.data(),16,{true,0x80,0x100,16,10,20}};
        check(origins.relay(producer.data(),16,a.data()));
        origins.offer.origin.submitPc=0x88;
        check(origins.relay(producer.data(),16,b.data()));
        check(origins.take(b.data(),16).submitPc==0x88);
        check(origins.take(a.data(),16).submitPc==0x80);
        check(!origins.take(a.data(),16).known);
        origins.offer={};check(!origins.relay(producer.data(),16,a.data()));
        std::array<std::array<uint8_t,16>,65> copies{};
        origins.offer={producer.data(),16,{true,0x90,0x200,16,30,40}};
        for(unsigned i=0;i<64;++i)check(origins.relay(producer.data(),16,copies[i].data()));
        check(!origins.relay(producer.data(),16,copies[64].data()) && origins.dropped==1);
        check(!origins.take(copies[64].data(),16).known);
        check(!origins.take(copies[0].data(),8).known); // Mismatch clears stale metadata.
        check(!origins.take(copies[0].data(),16).known);
        origins.offer={};
        check(!origins.relay(producer.data(),16,copies[1].data()));
        check(!origins.take(copies[1].data(),16).known); // Unknown copy cannot inherit old origin.
        check(drakengard::gsPath1FogBucket(true,0,0)==0);
        check(drakengard::gsPath1FogBucket(true,0,255)==1);
        check(drakengard::gsPath1FogBucket(true,255,255)==2);
        check(drakengard::gsPath1FogBucket(false,0,0)==3);

        const auto temp=std::filesystem::temp_directory_path();
        std::filesystem::path directory;
        for(unsigned i=0;i<100;++i) {
            auto candidate=temp/("d1-path1-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) { directory=candidate;break; }
        }
        check(!directory.empty());
        drakengard::GsPath1Captures captures;
        drakengard::GsPath1CaptureKey key{};
        check(!captures.accepts(1,1,drakengard::GsPath1Captures::MaxBytes+1u,key));
        auto metadata=[](std::ostream& out){out<<"{\"synthetic\":true}";};
        check(!captures.capture(directory,3,1,producer,key,metadata));
        check(!captures.capture(directory,1,1,std::span<const uint8_t>(producer.data(),8),key,metadata));
        check(captures.capture(directory,1,1,producer,key,metadata));
        check(std::filesystem::file_size(directory/"packet-1.bin")==16);
        check(!captures.capture(directory,1,2,producer,key,metadata));
        key.words[0]=1;check(!captures.capture(directory,1,1,producer,key,metadata));
        key.words[0]=2;check(captures.capture(directory,1,2,producer,key,metadata));
        key.words[0]=99;check(!captures.capture(directory,1,99,producer,key,metadata));
        for(unsigned i=3;i<=8;++i) { key.words[0]=i;key.bucket=(i-1)/2;check(captures.capture(directory,1,i,producer,key,metadata)); }
        key.words[0]=9;check(!captures.capture(directory,1,9,producer,key,metadata) && captures.saved()==8);
        drakengard::GsPath1Captures fresh;
        bool refused=false;
        try { fresh.capture(directory,1,1,producer,key,metadata); }
        catch(const std::runtime_error&) { refused=true; }
        check(refused && fresh.saved()==0); // Never overwrite an existing capture.
        // Remove only the exact files owned by this newly acquired fixture directory.
        for(unsigned i=1;i<=8;++i) {
            std::filesystem::remove(directory/("packet-"+std::to_string(i)+".bin"));
            std::filesystem::remove(directory/("packet-"+std::to_string(i)+".json"));
        }
        std::filesystem::remove(directory);
        std::cout<<"PATH1 origin relay, delayed delivery, reuse, overflow and capture bounds passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
