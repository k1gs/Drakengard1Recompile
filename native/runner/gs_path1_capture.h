#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>

namespace drakengard {
struct GsKickOrigin {
    bool known{};
    uint32_t submitPc{}, vuAddress{}, bytes{};
    uint64_t issueCycle{}, submitCycle{};
};
// Serialized EE owner only. Metadata follows the actual arbiter-owned copy,
// including delayed/reordered delivery. No guest or packet bytes are modified.
struct GsKickOrigins {
    struct Offer { const uint8_t* data{}; uint32_t bytes{}; GsKickOrigin origin; } offer;
    struct Copy { const uint8_t* data{}; uint32_t bytes{}; GsKickOrigin origin; };
    std::array<Copy,64> copies{};
    uint64_t dropped{};
    bool relay(const uint8_t* source,uint32_t bytes,const uint8_t* copy) {
        Copy* slot=nullptr;
        // A new queue allocation may reuse a dropped packet's address. Unknown
        // origins must invalidate the old association too, never inherit it.
        if(copy)for(auto& item:copies)if(item.data==copy) { slot=&item;item={};break; }
        if (!copy || bytes<16 || source!=offer.data || bytes!=offer.bytes ||
            !offer.origin.known || offer.origin.bytes!=bytes) return false;
        if(!slot)for(auto& item:copies)if(!item.data) { slot=&item;break; }
        if(slot) { *slot={copy,bytes,offer.origin};return true; }
        ++dropped;return false;
    }
    GsKickOrigin take(const uint8_t* data,uint32_t bytes) {
        for(auto& item:copies) if(item.data==data) {
            const auto result=item.bytes==bytes?item.origin:GsKickOrigin{};
            item={};return result;
        }
        return {};
    }
};
inline thread_local GsKickOrigins gsKickOrigins;
inline thread_local GsKickOrigin gsCurrentKickOrigin;
inline const char* gsPath1CaptureDirectory() {
    static const char* path=std::getenv("DRAKENGARD_PATH1_CAPTURE_OUTPUT");
    return path && *path?path:nullptr;
}
class GsKickOfferScope {
    GsKickOrigins::Offer previous=gsKickOrigins.offer;
public:
    GsKickOfferScope(const uint8_t* data,uint32_t bytes,GsKickOrigin origin) {
        if(gsPath1CaptureDirectory())gsKickOrigins.offer={data,bytes,origin};
    }
    ~GsKickOfferScope() { gsKickOrigins.offer=previous; }
};
inline void relayGsKickOrigin(const uint8_t* data,uint32_t bytes,const uint8_t* copy) {
    if(gsPath1CaptureDirectory())gsKickOrigins.relay(data,bytes,copy);
}
inline GsKickOrigin takeGsKickOrigin(const uint8_t* data,uint32_t bytes) {
    return gsPath1CaptureDirectory()?gsKickOrigins.take(data,bytes):GsKickOrigin{};
}
inline void writeGsKickOrigin(std::ostream& out,const GsKickOrigin& origin) {
    out<<"{\"known\":"<<(origin.known?"true":"false")
       <<",\"submit_pc_bytes\":"<<origin.submitPc<<",\"vu_data_address\":"<<origin.vuAddress
       <<",\"bytes\":"<<origin.bytes<<",\"issue_cycle\":"<<origin.issueCycle
       <<",\"submit_cycle\":"<<origin.submitCycle<<'}';
}
struct GsPath1CaptureKey {
    std::array<uint64_t,7> words{};
    unsigned bucket{};
    bool operator==(const GsPath1CaptureKey&) const = default;
};
// 0: fully fogged, 1: partial/mixed fog, 2: full vertex color, 3: fog disabled.
inline unsigned gsPath1FogBucket(bool enabled,unsigned minimum,unsigned maximum) {
    if(!enabled)return 3;
    if(maximum==0)return 0;
    if(minimum==255 && maximum==255)return 2;
    return 1;
}
class GsPath1Captures {
    std::array<GsPath1CaptureKey,8> keys{};
    std::array<uint64_t,8> packets{};
    unsigned count{};
    std::array<unsigned,4> bucketCounts{};
public:
    static constexpr uint32_t MaxBytes=1024u*1024u;
    unsigned saved() const { return count; }
    bool accepts(unsigned path,uint64_t packet,size_t bytes,const GsPath1CaptureKey& key) const {
        if(path!=1 || !packet || bytes<16 || bytes>MaxBytes || count==keys.size() ||
           key.bucket>=bucketCounts.size() || bucketCounts[key.bucket]>=2)return false;
        for(unsigned i=0;i<count;++i)if(keys[i]==key || packets[i]==packet)return false;
        return true;
    }
    template<class Metadata>
    bool capture(const std::filesystem::path& directory,unsigned path,uint64_t packet,
                 std::span<const uint8_t> data,const GsPath1CaptureKey& key,Metadata metadata) {
        if(!accepts(path,packet,data.size(),key))return false;
        const auto base=directory/("packet-"+std::to_string(packet));
        auto binary=base;binary+=".bin";auto manifest=base;manifest+=".json";
        if(std::filesystem::exists(binary)||std::filesystem::exists(manifest))
            throw std::runtime_error("PATH1 capture already exists");
        std::ofstream bytes(binary,std::ios::binary);bytes.write(reinterpret_cast<const char*>(data.data()),data.size());
        std::ofstream json(manifest);metadata(json);bytes.flush();json.flush();
        if(!bytes || !json)throw std::runtime_error("Cannot write bounded PATH1 capture");
        ++bucketCounts[key.bucket];keys[count]=key;packets[count++]=packet;return true;
    }
};
}
