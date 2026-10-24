#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include "ipu_vlc.h"
#include "ipu_bdec.h"
#include "ipu_csc.h"

namespace drakengard {
class EeIpu {
    using Quadword=std::array<uint8_t,16>;
    std::array<Quadword,8> fifo{};
    std::array<Quadword,2> window{};
    unsigned fifoHead{},fifoCount{},windowHead{},buffered{},bitPosition{};
    std::array<std::array<uint8_t,64>,2> matrices{};
    std::array<uint8_t,32> clut{};
    std::array<uint16_t,2> thresholds{};
    uint32_t configuration{},result{},top{},command{};
    unsigned tablePosition{};
    bool busy{},interrupt{},topBusy{},vlcDecoded{},errorCode{};
    std::array<Quadword,8> outputFifo{};
    unsigned outputHead{},outputSize{},outputPosition{},blockNumber{},coefficientIndex{};
    std::array<uint8_t,1024> macroblock{};
    std::array<uint8_t,384> cscInput{};
    unsigned cscRemaining{};
    bool cscConverted{};
    std::array<int16_t,64> coefficients{};
    std::array<int,3> predictors{};
    enum class BlockStage { Dc, Ac, Output, Drain, Check, Prefix, Top };
    BlockStage blockStage{BlockStage::Dc};
    bool startCode{};
    unsigned codedPattern{};
    bool dataCommand() const { return (command>>28)==3 || (command>>28)==4; }
    bool pull() {
        if (!fifoCount || buffered==2) return false;
        window[(windowHead+buffered)&1u]=fifo[fifoHead];
        fifoHead=(fifoHead+1)&7u; --fifoCount; ++buffered;
        return true;
    }
    bool fill(unsigned bits) {
        while (buffered*128<bitPosition+bits) if (!pull()) return false;
        return true;
    }
    uint8_t byte(unsigned index) const { return window[(windowHead+index/16)&1u][index%16]; }
    bool peek(std::span<uint8_t> bytes) {
        if (!fill(static_cast<unsigned>(bytes.size()*8))) return false;
        const unsigned shift=bitPosition&7u, first=bitPosition/8;
        for (unsigned i=0;i<bytes.size();++i) {
            unsigned value=unsigned(byte(first+i))<<shift;
            if (shift) value|=byte(first+i+1)>>(8-shift);
            bytes[i]=static_cast<uint8_t>(value);
        }
        return true;
    }
    bool peekWord(uint32_t& value) {
        std::array<uint8_t,4> bytes;
        if (!peek(bytes)) return false;
        value=(uint32_t(bytes[0])<<24)|(uint32_t(bytes[1])<<16)|(uint32_t(bytes[2])<<8)|bytes[3];
        return true;
    }
    void advance(unsigned bits) {
        fill(bits);
        bitPosition+=bits;
        if (bitPosition>=128) {
            bitPosition-=128;
            if (buffered) { windowHead^=1u; --buffered; }
            if (!buffered) pull();
        }
    }
    void complete() { busy=false; interrupt=true; }
    void processBlock() {
        while (true) {
            uint32_t bits{};
            if (blockStage==BlockStage::Dc) {
                if (!peekWord(bits)) return;
                const auto dc=decodeIpuDcSize(bits,blockNumber>=4);
                if (!dc.length) throw std::runtime_error("Malformed BDEC DC prefix");
                unsigned size=dc.value;
                const unsigned raw=size?((bits<<(dc.length))>>(32-size)):0;
                const int difference=size && !(raw&(1u<<(size-1)))?int(raw)-int((1u<<size)-1):int(raw);
                const auto component=blockNumber<4?0:blockNumber-3;
                const int predictor=predictors[component]+difference;
                const int dcValue=predictor*int(1u<<(3-((configuration>>16)&3)));
                if (dcValue<-32768 || dcValue>32767) throw std::runtime_error("BDEC DC predictor overflow");
                advance(dc.length+size);
                predictors[component]=predictor;
                coefficients={}; coefficients[0]=static_cast<int16_t>(dcValue);
                coefficientIndex=0; blockStage=BlockStage::Ac;
            }
            if (blockStage==BlockStage::Ac) {
                if (!peekWord(bits)) return;
                const auto ac=decodeIpuIntraAc(bits);
                if (!ac.length) throw std::runtime_error("Malformed BDEC AC prefix");
                if (ac.end) {
                    advance(ac.length);
                    const auto pixels=ipuInverseDct(coefficients);
                    const bool luma=blockNumber<4;
                    const unsigned stride=luma?((command&0x02000000u)?32:16):8;
                    const unsigned base=luma?(blockNumber%2)*8+(blockNumber/2)*((command&0x02000000u)?16:128):
                        256+(blockNumber-4)*64;
                    for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x) {
                        const unsigned destination=(base+y*stride+x)*2;
                        macroblock[destination]=pixels[y*8+x]; macroblock[destination+1]=0;
                    }
                    if (++blockNumber<6) { blockStage=BlockStage::Dc; continue; }
                    codedPattern=63; blockStage=BlockStage::Output; continue;
                }
                unsigned run=ac.run, consumed=ac.length+1;
                int level=static_cast<int>(ac.level);
                if (ac.escape) {
                    // Prefix6 + run6 + signed level12 are one atomic read.
                    run=(bits>>20)&63; const unsigned raw=(bits>>8)&4095;
                    level=raw&2048?int(raw)-4096:int(raw); consumed=24;
                    if (!level || level==-2048) throw std::runtime_error("Forbidden MPEG2 BDEC escape level");
                } else if ((bits>>(31-ac.length))&1) level=-level;
                const unsigned next=coefficientIndex+run+1;
                if (next>=64) throw std::runtime_error("BDEC coefficient run exceeds block");
                const unsigned scale=ipuQuantizerScale((command>>16)&31,(configuration&0x00400000u)!=0);
                const int product=level*int(scale)*int(matrices[0][next]);
                // Normal VLC sign is applied after magnitude truncation. Escape
                // levels use the IPU signed arithmetic shift, including negatives.
                const int quantized=ac.escape?(product>=0?product/16:-((-product+15)/16)):product/16;
                coefficients[ipuZigzag(next)]=static_cast<int16_t>(std::clamp(quantized,-2048,2047));
                coefficientIndex=next; advance(consumed); continue;
            }
            if (blockStage==BlockStage::Output) {
                while (outputPosition<48 && outputSize<8) {
                    auto& destination=outputFifo[(outputHead+outputSize)&7u];
                    for (unsigned i=0;i<16;++i) destination[i]=macroblock[outputPosition*16+i];
                    ++outputPosition; ++outputSize;
                }
                if (outputPosition<48) return;
                blockStage=BlockStage::Drain;
            }
            if (blockStage==BlockStage::Drain) {
                if (outputSize) return;
                blockStage=BlockStage::Check;
            }
            if (blockStage==BlockStage::Check) {
                if (!peekWord(bits)) return;
                if ((bits>>24)==0) { advance((8-(bitPosition&7u))&7u); blockStage=BlockStage::Prefix; }
                else blockStage=BlockStage::Top;
            }
            if (blockStage==BlockStage::Prefix) {
                if (!peekWord(bits)) return;
                const unsigned prefix=bits>>8;
                if (!prefix) { advance(8); continue; }
                startCode=prefix==1; errorCode=!startCode; blockStage=BlockStage::Top;
            }
            if (blockStage==BlockStage::Top) {
                if (!peekWord(top)) return;
                topBusy=false; complete(); return;
            }
        }
    }
    void processCsc() {
        while (cscRemaining) {
            if (!cscConverted) {
                while (tablePosition<cscInput.size()) {
                    if (!peek({cscInput.data()+tablePosition,8})) return;
                    advance(64); tablePosition+=8;
                }
                convertIpuCscRgb32(cscInput,macroblock,thresholds);
                cscConverted=true; outputPosition=0;
            }
            while (outputPosition<64 && outputSize<8) {
                auto& destination=outputFifo[(outputHead+outputSize)&7u];
                for (unsigned i=0;i<16;++i) destination[i]=macroblock[outputPosition*16+i];
                ++outputPosition; ++outputSize;
            }
            if (outputPosition<64) return;
            --cscRemaining; tablePosition=0; cscConverted=false;
        }
        // CSC completes once the final QW is accepted by the output FIFO.
        // Those final QWs may still be pending CPU/DMA reads.
        complete();
    }
    void process() {
        if (!busy) return;
        if ((command>>28)==2) { processBlock(); return; }
        if ((command>>28)==7) { processCsc(); return; }
        if ((command>>28)==3) {
            if (!vlcDecoded) {
                uint32_t bits{};
                if (!peekWord(bits)) return;
                IpuVlcResult decoded;
                if (((command>>26)&3u)==0) {
                    decoded=decodeIpuMacroblockIncrement(bits,(configuration&0x00800000u)!=0);
                    result=(decoded.length<<16)|decoded.value;
                } else if (((command>>26)&3u)==2) {
                    const auto motion=decodeIpuMotionCode(bits);
                    decoded.length=motion.length;
                    result=motion.data;
                } else {
                    // B-MBT adds consumed length; I/P return modes only.
                    const unsigned picture=(configuration>>24)&7u;
                    if (picture==3)
                        decoded=decodeIpuBidirectionalMacroblockType(bits);
                    else if (picture==2)
                        decoded=decodeIpuPredictiveMacroblockType(bits);
                    else if (bits&0x80000000u) decoded={1,1};
                    else if (bits&0x40000000u) decoded={17,2};
                    result=decoded.value;
                    if (picture==3) result|=decoded.length<<16;
                }
                errorCode=!decoded.length;
                advance(decoded.length);
                vlcDecoded=true;
            }
            if (!peekWord(top)) return;
            topBusy=false; complete(); return;
        }
        if ((command>>28)==4) {
            if (!peekWord(result)) return;
            top=result; topBusy=false; complete(); return;
        }
        const bool quantization=(command>>28)==5;
        const unsigned size=quantization?64u:32u;
        while (tablePosition<size) {
            std::array<uint8_t,8> bytes;
            if (!peek(bytes)) return;
            auto* destination=quantization?matrices[(command>>27)&1u].data():clut.data();
            for (unsigned i=0;i<bytes.size();++i) destination[tablePosition+i]=bytes[i];
            advance(64); tablePosition+=8;
        }
        complete();
    }
public:
    static constexpr uint32_t Command=0x10002000u,Control=0x10002010u;
    static constexpr uint32_t Position=0x10002020u,Top=0x10002030u,Input=0x10007010u,Output=0x10007000u;
    static bool ownsRegisters(uint32_t address) { return address>=Command && address<Top+8; }
    bool canAcceptInput() const { return fifoCount<8; }
    unsigned outputCount() const { return outputSize; }
    bool pop(std::span<uint8_t,16> bytes) {
        if (!outputSize) return false;
        for (unsigned i=0;i<16;++i) bytes[i]=outputFifo[outputHead][i];
        outputHead=(outputHead+1)&7u; --outputSize; process(); return true;
    }
    bool push(std::span<const uint8_t,16> bytes) {
        if (!canAcceptInput()) return false;
        auto& destination=fifo[(fifoHead+fifoCount)&7u];
        for (unsigned i=0;i<16;++i) destination[i]=bytes[i];
        ++fifoCount; process(); return true;
    }
    bool takeInterrupt() { return std::exchange(interrupt,false); }
    uint64_t diagnosticCommand() const {
        return uint64_t(result)|((busy && dataCommand())?0x8000000000000000ull:0ull);
    }
    std::span<const uint8_t,64> quantization(bool nonIntra) const { return matrices[nonIntra?1:0]; }
    std::span<const uint8_t,32> palette() const { return clut; }
    const std::array<uint16_t,2>& alphaThresholds() const { return thresholds; }
    void write(uint32_t address,uint32_t value) {
        if (address==Control) {
            configuration=value&0x07f30000u;
            if ((configuration&0x30000u)==0x30000u) throw std::runtime_error("Unsupported IPU intra DC precision");
            if (value&0x40000000u) {
                fifoHead=fifoCount=windowHead=buffered=bitPosition=tablePosition=0;
                result=top=command=0; thresholds={}; busy=topBusy=vlcDecoded=errorCode=false; interrupt=true;
                outputHead=outputSize=outputPosition=blockNumber=coefficientIndex=codedPattern=0;
                predictors={}; startCode=false; blockStage=BlockStage::Dc;
                cscRemaining=0; cscConverted=false;
            }
            return;
        }
        if (address!=Command) throw std::runtime_error("Unsupported IPU register write");
        const auto opcode=value>>28;
        if (opcode!=0 && opcode!=2 && opcode!=3 && opcode!=4 && opcode!=5 && opcode!=6 && opcode!=7 && opcode!=9)
            throw std::runtime_error("Unsupported IPU decode/conversion command: opcode="+
                                     std::to_string(opcode)+" value="+std::to_string(value));
        if (opcode==3 && (((value>>26)&3u)>2 ||
                         (((value>>26)&3u)==1 && ((configuration>>24)&7u)>3)))
            throw std::runtime_error("Unsupported IPU VDEC table: value="+std::to_string(value)+
                                     " CTRL="+std::to_string(read(Control))+
                                     " PCT="+std::to_string((configuration>>24)&7u));
        if (opcode==2 && (!(value&0x08000000u) || (configuration&0x00900000u) || !(configuration&0x00200000u)))
            throw std::runtime_error("Unsupported BDEC mode (requires MPEG2 intra, IVF1, normal scan): value="+
                                     std::to_string(value)+" CTRL="+std::to_string(read(Control))+
                                     " PCT="+std::to_string((configuration>>24)&7u));
        if (opcode==7 && (value&0x08000000u))
            throw std::runtime_error("Unsupported CSC RGB16 output: value="+std::to_string(value));
        const uint32_t options=opcode==0?0x7fu:opcode==2?0x0e1f003fu:opcode==3?0x0c00003fu:opcode==4?0x3fu:opcode==5?0x0800003fu:opcode==7?0x040007ffu:opcode==9?0x01ff01ffu:0u;
        if (value&~(0xf0000000u|options)) throw std::runtime_error("Unsupported IPU command option bits");
        if (busy && opcode!=0) throw std::runtime_error("IPU command replaced an unfinished operation");
        command=value; tablePosition=0; vlcDecoded=errorCode=startCode=false;
        if (opcode==2) {
            if (outputSize) throw std::runtime_error("BDEC replaced undrained output");
            blockNumber=coefficientIndex=outputPosition=0; blockStage=BlockStage::Dc;
            if (value&0x04000000u) predictors.fill(128<<((configuration>>16)&3));
            top=0; topBusy=true;
        }
        if (opcode==7) {
            if (outputSize) throw std::runtime_error("CSC replaced undrained output");
            cscRemaining=value&0x7ffu; cscConverted=false; outputPosition=0;
        }
        if (opcode==0) {
            fifoHead=fifoCount=windowHead=buffered=0; bitPosition=value&0x7f;
            complete(); return;
        }
        if (opcode==9) {
            thresholds={static_cast<uint16_t>(value&0x1ff),static_cast<uint16_t>((value>>16)&0x1ff)};
            complete(); return;
        }
        if (opcode==2 || opcode==3 || opcode==4 || opcode==5) advance(value&0x3f);
        if (opcode==3 || opcode==4) topBusy=true;
        busy=true; process();
    }
    uint32_t read(uint32_t address) {
        switch (address) {
        case Command:
            if (!dataCommand()) peekWord(result);
            return result;
        case Command+4: return busy && dataCommand()?0x80000000u:0u;
        case Control: return configuration|fifoCount|(outputSize<<4)|(codedPattern<<8)|
            (busy?0x80000000u:0u)|(errorCode?0x4000u:0u)|(startCode?0x8000u:0u);
        case Position: return (bitPosition&0x7fu)|(fifoCount<<8)|(buffered<<16);
        case Top: return top;
        case Top+4: return topBusy?0x80000000u:0u;
        default: throw std::runtime_error("Unsupported IPU register read: address="+std::to_string(address));
        }
    }
    uint64_t read64(uint32_t address) {
        if (address==Command || address==Top) {
            const uint64_t low=read(address); return low|(uint64_t(read(address+4))<<32);
        }
        if (address==Control || address==Position) return read(address);
        throw std::runtime_error("Unsupported IPU 64-bit register read");
    }
};
}
