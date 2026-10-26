#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ostream>

namespace drakengard {
struct GsDmaCpuContext {
    bool known{},stackKnown{};
    uint32_t pc{}, ra{}, sp{};
    std::array<uint32_t,16> stack{};
};
inline thread_local GsDmaCpuContext gsDmaCpuContext;
struct GsDmaSpan { uint32_t offset{}, source{}, bytes{}, tag{}; bool scratch{}; };
struct GsDmaSource {
    bool active{}, overflow{};
    const void* key{};
    size_t bytes{}, count{};
    uint32_t tadr{}, chcr{}, currentTag{};
    GsDmaCpuContext cpu;
    std::array<GsDmaSpan,4096> spans{};
};
struct GsDmaSourceStore {
    GsDmaSource builder;
    std::array<GsDmaSource,8> queued{};
    size_t next{};
};
inline thread_local std::unique_ptr<GsDmaSourceStore> gsDmaSourceStore;
inline thread_local const GsDmaSource* gsCurrentDmaSource{};
inline thread_local uint32_t gsCurrentGifDataOffset{};
inline void beginGsDmaSource(bool enabled,uint32_t tadr,uint32_t chcr) {
    if (!enabled) return;
    if (!gsDmaSourceStore) gsDmaSourceStore=std::make_unique<GsDmaSourceStore>();
    auto& b=gsDmaSourceStore->builder;
    b.active=true; b.overflow=false; b.count=0; b.bytes=0; b.tadr=tadr; b.chcr=chcr;
    b.cpu=gsDmaCpuContext;
}
inline void markGsDmaTag(uint32_t tag) {
    if (gsDmaSourceStore) gsDmaSourceStore->builder.currentTag=tag;
}
inline void appendGsDmaSource(size_t offset,uint32_t source,uint32_t bytes,bool scratch) {
    if (!gsDmaSourceStore) return;
    auto& b=gsDmaSourceStore->builder;
    if (!b.active) return;
    b.bytes=offset+bytes;
    if (b.count==b.spans.size() || offset>UINT32_MAX) { b.overflow=true; return; }
    b.spans[b.count++]={uint32_t(offset),source,bytes,b.currentTag,scratch};
}
inline void queueGsDmaSource(const void* key,size_t bytes) {
    if (!gsDmaSourceStore) return;
    auto& store=*gsDmaSourceStore; auto& b=store.builder;
    if (!b.active) return;
    auto& slot=store.queued[store.next++%store.queued.size()];
    slot=b; slot.key=key; slot.bytes=bytes; b.active=false;
}
inline GsDmaSource* findGsDmaSource(const void* key,size_t bytes) {
    if (!gsDmaSourceStore) return nullptr;
    GsDmaSource* result=nullptr;
    for (auto& slot:gsDmaSourceStore->queued)
        if (slot.active && slot.key==key && slot.bytes==bytes) {
            if (result) return nullptr;
            result=&slot;
        }
    return result;
}
inline void relayGsDmaSource(const void* original,size_t bytes,const void* copied) {
    if (auto* source=findGsDmaSource(original,bytes)) source->key=copied;
}
struct GsDmaAddress { bool known{},scratch{}; uint32_t address{},tag{}; };
inline GsDmaAddress resolveGsDmaAddress(const GsDmaSource* source,uint32_t offset,uint32_t bytes) {
    if (!source) return {};
    for (size_t i=0;i<source->count;++i) {
        const auto& s=source->spans[i];
        if (s.offset<=offset && uint64_t(offset)+bytes<=uint64_t(s.offset)+s.bytes)
            return {true,s.scratch,s.source+(offset-s.offset),s.tag};
    }
    return {};
}
inline void writeGsDmaSourceJson(std::ostream& out,const GsDmaSource* source) {
    if (!source) { out<<"null"; return; }
    const auto& c=source->cpu;
    out<<"{\"tadr\":"<<source->tadr<<",\"chcr\":"<<source->chcr
       <<",\"overflow\":"<<(source->overflow?"true":"false")<<",\"cpu_known\":"<<(c.known?"true":"false")
       <<",\"stack_known\":"<<(c.stackKnown?"true":"false")
       <<",\"pc\":"<<c.pc<<",\"ra\":"<<c.ra<<",\"sp\":"<<c.sp<<",\"stack\":[";
    for(size_t i=0;i<c.stack.size();++i) { if(i)out<<','; out<<c.stack[i]; }
    out<<"],\"spans\":[";
    for(size_t i=0;i<source->count;++i) {
        const auto& s=source->spans[i]; if(i)out<<',';
        out<<'['<<s.offset<<','<<s.source<<','<<s.bytes<<','<<s.tag<<','<<(s.scratch?1:0)<<']';
    }
    out<<"]}";
}
}
