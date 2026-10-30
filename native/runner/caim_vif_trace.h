#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <exception>
#include <stdexcept>

namespace drakengard {
// Serialized EE executor only. Observes original buffers; never writes guest state.
class CaimVifTrace {
    bool owner{}, selected{}, consuming{}, completed{}, microOpen{};
    unsigned phase{}, generation{}, unpacks{}, micros{}, dmaRecords{};
    std::array<unsigned,2> captured{};
    const uint8_t* queued{};
    uint32_t queuedBytes{}, channel{}, control{};
    std::array<uint8_t,48> sourceTags{};
    std::array<uint8_t,128> exported{};
    uint32_t exportedAddress{}, exportedGeneration{}, exportedPhase{};
    bool exportValid{}, gifMatch{}, gifActive{}, vifMatch{}, outputVifActive{}, vu1Running{}, outputLoaded{};
    std::array<unsigned,2> exportRecords{}, gifRecords{}, vifRecords{};
    const uint8_t* gifQueued{};
    const uint8_t* vifQueued{};
    uint32_t gifBytes{}, vifBytes{}, vifMatchOffset{}, vifTargetOffset{};
    unsigned gifDraws{}, queuedGifPath=3, outputUnpacks{}, vu1Runs{}, kickRecords{};
    const char* gifProvenance="packet_contains_caim_spr_output";
    const char* path{};
    static uint32_t word(const uint8_t* p) { uint32_t v;std::memcpy(&v,p,4);return v; }
    template<class Action> void record(const char* event,Action action) {
        if(!path || !*path)return;
        std::ofstream out(path,std::ios::app);
        out<<"{\"event\":\""<<event<<"\",\"generation\":"<<generation<<",\"phase\":"<<phase
           <<",\"resource\":"<<0x5c5e60u<<",\"mesh\":"<<0x795250u<<",\"channel\":"<<channel;
        action(out);out<<"}\n";out.flush();
        if(!out)throw std::runtime_error("Cannot write Caim VIF observation");
    }
    static void bytes(std::ostream& out,const char* key,const uint8_t* data,unsigned count) {
        out<<",\""<<key<<"\":[";
        for(unsigned i=0;i<count;++i) { if(i)out<<',';out<<unsigned(data[i]); }out<<']';
    }
public:
    void boundary(bool before,unsigned part,unsigned fadePhase,const char* output) {
        path=output;
        if(!output || !*output)return;
        if(fadePhase>1)throw std::runtime_error("Invalid Caim fade phase");
        if(before) {
            owner=part==0;selected=consuming=completed=false;queued=nullptr;
            phase=fadePhase;
        } else owner=false;
    }
    bool begin(uint32_t ch,uint32_t tag,uint32_t chcr,const uint8_t* tags) {
        if(ch==0x1000a000u)gifMatch=false;
        if(ch==0x10009000u)vifMatch=false;
        if(!owner || !path || !*path || captured[phase] || tag!=0x665bc0u ||
           (ch!=0x10008000u && ch!=0x10009000u))return false;
        if(!tags)return false;
        const std::array<uint32_t,3> refs{0x798a30u,0x798ee0u,0x799210u};
        const std::array<uint32_t,3> commands{0x7908000eu,0x7908003eu,0x6e08406eu};
        for(unsigned i=0;i<3;++i) {
            const auto header=word(tags+i*16);
            if((header&0x7000ffffu)!=(i==2?2u:0x30000003u) ||
               word(tags+i*16+4)!=refs[i] || word(tags+i*16+8)!=0 ||
               word(tags+i*16+12)!=commands[i])return false;
        }
        std::memcpy(sourceTags.data(),tags,sourceTags.size());
        selected=true;channel=ch;control=chcr;unpacks=micros=dmaRecords=0;++generation;
        return true;
    }
    void queue(bool match,const uint8_t* data,uint32_t size) {
        if(!match)return;
        // The exact three-tag stream must be present, not merely a common UNPACK opcode.
        if(size!=152 || word(data+4)!=0x7908000eu || word(data+60)!=0x7908003eu ||
           word(data+116)!=0x6e08406eu) {
            selected=false;return; // A different batch must not consume this capture.
        }
        queued=data;queuedBytes=size;++captured[phase];
        record("dma_queue",[&](auto& out) { out<<",\"tadr\":"<<0x665bc0u<<",\"chcr\":"<<control;bytes(out,"tags",sourceTags.data(),sourceTags.size());bytes(out,"stream",data,size); });
    }
    bool enter(const uint8_t* data,uint32_t size,unsigned vif) {
        consuming=selected && data==queued && size==queuedBytes &&
            channel==(vif?0x10009000u:0x10008000u);
        if(consuming)record("consumer_begin",[&](auto& out) { out<<",\"vif\":"<<vif; });
        return consuming;
    }
    void leave(bool match) {
        if(!match)return;
        record("consumer_end",[&](auto& out) { out<<",\"unpack_count\":"<<unpacks; });
        consuming=false;completed=true;queued=nullptr;
    }
    bool active() const { return consuming; }
    template<class Registers> void unpack(bool after,uint32_t command,const Registers& regs,
                                         const uint8_t* source,uint32_t count,const uint8_t* vu,
                                         uint32_t vuBytes,bool complete) {
        if(!consuming && !outputVifActive)return;
        if(outputVifActive && after && complete && !outputLoaded) {
            const auto base=reinterpret_cast<uintptr_t>(vifQueued),begin=reinterpret_cast<uintptr_t>(source);
            if(begin>=base && begin-base<=vifTargetOffset &&
               uint64_t(begin-base)+count>=uint64_t(vifTargetOffset)+exported.size()) {
                outputLoaded=true;
                record("vif1_target_loaded",[&](auto& out) {
                    out<<",\"command\":"<<command<<",\"stream_offset\":"<<vifTargetOffset;
                });
            }
        }
        unsigned ordinal;
        if(consuming) {
            if(!after && ++unpacks>3)throw std::runtime_error("Caim UNPACK diagnostic bound exceeded");
            ordinal=unpacks;
        } else {
            if(!after)++outputUnpacks;
            if(outputUnpacks>16)return;
            ordinal=outputUnpacks;
        }
        record(consuming?(after?"unpack_after":"unpack_before"):(after?"vif1_unpack_after":"vif1_unpack_before"),[&](auto& out) {
            out<<",\"ordinal\":"<<ordinal<<",\"command\":"<<command<<",\"cycle\":"<<regs.cycle
               <<",\"cl\":"<<(regs.cycle&255u)<<",\"wl\":"<<((regs.cycle>>8)&255u)
               <<",\"mask\":"<<regs.mask<<",\"mode\":"<<regs.mode
               <<",\"complete_input\":"<<(complete?"true":"false");
            bytes(out,"row",reinterpret_cast<const uint8_t*>(regs.row),16);
            bytes(out,"col",reinterpret_cast<const uint8_t*>(regs.col),16);
            if(complete)bytes(out,"source",source,count);
            if(vu && vuBytes<=16384)bytes(out,"vu",vu,vuBytes);
        });
    }
    void micro(bool after,uint32_t pc,const uint8_t* code,const uint8_t* vu,uint32_t vi1) {
        if(!owner || !completed || (!after && micros>=4))return;
        if(after && !microOpen)return;
        microOpen=!after;
        if(!after)++micros;
        record(after?"micro_after":"micro_before",[&](auto& out) {
            out<<",\"ordinal\":"<<micros<<",\"pc\":"<<pc<<",\"vi1\":"<<vi1;
            bytes(out,"vu",vu,4096);if(!after)bytes(out,"code",code,4096);
        });
    }
    bool following() const { return owner && completed && dmaRecords<12; }
    void dma(uint32_t ch,uint32_t chcr,uint32_t madr,uint32_t qwc,uint32_t tadr,uint32_t sadr) {
        if(!owner || !completed || dmaRecords++>=12)return;
        record("subsequent_dma",[&](auto& out) {
            out<<",\"dma_channel\":"<<ch<<",\"chcr\":"<<chcr<<",\"madr\":"<<madr
               <<",\"qwc\":"<<qwc<<",\"tadr\":"<<tadr<<",\"sadr\":"<<sadr;
        });
    }
    // Capture the actual SPR->EE copy, not a later live-RAM sample.
    void output(uint32_t address,const uint8_t* data,uint32_t count,uint32_t scratch) {
        if(exportValid && uint64_t(address)<uint64_t(exportedAddress)+exported.size() &&
           uint64_t(exportedAddress)<uint64_t(address)+count)exportValid=false;
        if(!owner || !completed || !micros || count>4096 || exportRecords[phase]>=4)return;
        ++exportRecords[phase];
        record("spr_to_ee",[&](auto& out) {
            out<<",\"address\":"<<address<<",\"scratch\":"<<scratch<<",\"bytes\":"<<count;
            bytes(out,"data",data,count);
        });
        // The selected chain contains8 vectors; retain that complete output batch.
        if(count==exported.size()) {
            std::memcpy(exported.data(),data,count);
            exportedAddress=address;exportedGeneration=generation;exportedPhase=phase;exportValid=true;
        }
    }
    void outputSource(uint32_t ch,uint32_t address,uint32_t count,const uint8_t* data,uint32_t streamOffset) {
        if(!exportValid || (ch!=0x10009000u && ch!=0x1000a000u) ||
           (ch==0x10009000u?vifRecords[exportedPhase]:gifRecords[exportedPhase]) ||
           address>exportedAddress || uint64_t(address)+count<uint64_t(exportedAddress)+exported.size())return;
        if(std::memcmp(data+(exportedAddress-address),exported.data(),exported.size())) {
            exportValid=false;return;
        }
        if(ch==0x10009000u) {
            vifMatch=true;vifMatchOffset=streamOffset+(exportedAddress-address);
        } else gifMatch=true;
        record("output_source",[&](auto& out) {
            out<<",\"dma_channel\":"<<ch;
            out<<",\"output_generation\":"<<exportedGeneration<<",\"output_phase\":"<<exportedPhase
               <<",\"address\":"<<exportedAddress<<",\"bytes\":"<<exported.size()
               <<",\"stream_offset\":"<<uint64_t(streamOffset)+(exportedAddress-address);
        });
    }
    void queueOutput(uint32_t ch,const uint8_t* data,uint32_t count) {
        if(ch==0x10009000u && vifMatch && !vifRecords[exportedPhase]) {
            ++vifRecords[exportedPhase];vifQueued=data;vifBytes=count;outputUnpacks=vu1Runs=kickRecords=0;vifMatch=false;
            vifTargetOffset=vifMatchOffset;outputLoaded=false;
            record("vif1_output_queue",[&](auto& out) {
                out<<",\"output_generation\":"<<exportedGeneration<<",\"output_phase\":"<<exportedPhase
                   <<",\"bytes\":"<<count<<",\"captured_bytes\":"<<(count<65536?count:65536);
                bytes(out,"data",data,count<65536?count:65536);
            });
        }
        if(ch!=0x1000a000u || !gifMatch || gifRecords[exportedPhase])return;
        ++gifRecords[exportedPhase];gifQueued=data;gifBytes=count;gifDraws=0;gifMatch=false;
        queuedGifPath=3;gifProvenance="packet_contains_caim_spr_output";
        record("gif_queue",[&](auto& out) {
            out<<",\"output_generation\":"<<exportedGeneration<<",\"output_phase\":"<<exportedPhase
               <<",\"bytes\":"<<count<<",\"captured_bytes\":"<<(count<65536?count:65536);
            bytes(out,"data",data,count<65536?count:65536);
        });
    }
    void relayGif(unsigned gifPath,const uint8_t* from,uint32_t count,const uint8_t* to) {
        if(gifPath==queuedGifPath && gifQueued && from==gifQueued && count==gifBytes)gifQueued=to;
    }
    bool enterOutputVif(const uint8_t* data,uint32_t count) {
        const bool previous=outputVifActive;
        outputVifActive=vifQueued && data==vifQueued && count==vifBytes;
        if(outputVifActive)record("vif1_output_consumer",[](auto&){});
        return previous;
    }
    void leaveOutputVif(bool previous) {
        if(outputVifActive)vifQueued=nullptr;
        outputVifActive=previous;
    }
    void vu1(bool after,uint32_t command,uint32_t top,uint32_t itop,const uint8_t* code,const uint8_t* vu) {
        if(!outputVifActive || !outputLoaded)return;
        if(!after && vu1Runs>=4) { vu1Running=false;return; }
        if(after && !vu1Running)return;
        vu1Running=!after;if(!after)++vu1Runs;
        record(after?"vu1_after":"vu1_before",[&](auto& out) {
            out<<",\"ordinal\":"<<vu1Runs<<",\"command\":"<<command<<",\"top\":"<<top<<",\"itop\":"<<itop;
            bytes(out,"vu",vu,16384);if(!after)bytes(out,"code",code,16384);
        });
    }
    void kick(uint32_t pc,uint32_t address,const uint8_t* data,uint32_t count) {
        if(!vu1Running || kickRecords>=4)return;
        ++kickRecords;gifQueued=data;gifBytes=count;gifDraws=0;queuedGifPath=1;
        gifProvenance="vu1_run_after_caim_output_unpack";
        record("xgkick",[&](auto& out) {
            out<<",\"pc\":"<<pc<<",\"vu_address\":"<<address<<",\"bytes\":"<<count
               <<",\"captured_bytes\":"<<(count<65536?count:65536);
            bytes(out,"data",data,count<65536?count:65536);
        });
    }
    bool enterGif(const uint8_t* data,uint32_t count) {
        const bool previous=gifActive;
        gifActive=gifQueued && data==gifQueued && count==gifBytes;
        return previous;
    }
    void leaveGif(bool previous) { if(gifActive)gifQueued=nullptr;gifActive=previous; }
    template<class Batch> bool draw(const Batch& batch) {
        if(!gifActive || gifDraws++>=8)return false;
        record("gs_primitive",[&](auto& out) {
            out<<",\"output_generation\":"<<exportedGeneration<<",\"output_phase\":"<<exportedPhase
               <<",\"primitive\":"<<unsigned(batch.state.prim.type)<<",\"vertex_count\":"<<unsigned(batch.vertexCount)
               <<",\"provenance\":\""<<gifProvenance<<"\",\"vertices_xyz_bits_rgba\":[";
            for(unsigned i=0;i<batch.vertexCount && i<batch.vertices.size();++i) {
                if(i)out<<',';const auto& v=batch.vertices[i];
                const float x=v.x,y=v.y,z=v.z;
                out<<'['<<word(reinterpret_cast<const uint8_t*>(&x))<<','
                   <<word(reinterpret_cast<const uint8_t*>(&y))<<','
                   <<word(reinterpret_cast<const uint8_t*>(&z))<<','
                   <<unsigned(v.r)<<','<<unsigned(v.g)<<','<<unsigned(v.b)<<','<<unsigned(v.a)<<']';
            }out<<']';
        });
        return true;
    }
};
inline CaimVifTrace caimVifTrace;
struct CaimVifConsumerScope {
    bool match;
    CaimVifConsumerScope(const uint8_t* p,uint32_t n,unsigned vif):match(caimVifTrace.enter(p,n,vif)){}
    ~CaimVifConsumerScope() noexcept(false) {
        if(!std::uncaught_exceptions())caimVifTrace.leave(match);
    }
};
struct CaimGifConsumerScope {
    bool previous;
    CaimGifConsumerScope(const uint8_t* data,uint32_t count):previous(caimVifTrace.enterGif(data,count)){}
    ~CaimGifConsumerScope() { caimVifTrace.leaveGif(previous); }
};
struct CaimOutputVifScope {
    bool previous;
    CaimOutputVifScope(const uint8_t* data,uint32_t count):previous(caimVifTrace.enterOutputVif(data,count)){}
    ~CaimOutputVifScope() { caimVifTrace.leaveOutputVif(previous); }
};
} // namespace drakengard
