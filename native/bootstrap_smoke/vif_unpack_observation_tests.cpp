#include "vif_unpack_observation.h"
#include <array>
#include <sstream>
#include <string>

int main() {
    drakengard::VifUnpackObservation observation;
    std::array<uint8_t,20> source{};
    for(unsigned i=0;i<source.size();++i)source[i]=uint8_t(i+1);
    std::array<uint8_t,16> before{},after{};
    after[4]=255;
    // Masked unsigned V2-16, MODE3 and raw CL0/WL0 must survive observation.
    const uint32_t command=(0x75u<<24)|0x4000u;
    observation.observe(command,0,3,true,source,before,after);
    if(observation.calls!=1 || observation.completeCommands!=1 || observation.formatCalls[5]!=1 ||
       observation.modeCalls[3]!=1 || observation.formatModeCalls[5][3]!=1 || observation.declaredVectors[5]!=256 ||
       observation.clZeroCalls!=1 || observation.wlZeroCalls!=1 || observation.maskedCalls!=1 ||
       observation.unsignedCalls!=1) return 1;
    const auto& sample=observation.examples[0];
    if(sample.command!=command || sample.cycle!=0 || sample.mode!=3 || !sample.complete ||
       sample.sourceBytes!=16 || sample.source[15]!=16 || sample.after[4]!=255 || sample.before[4]!=0) return 2;
    // Saturating bounded samples must not saturate cumulative call/vector counts.
    for(unsigned i=0;i<100;++i)observation.observe(command,0x0101,3,false);
    if(observation.exampleCount!=2 || observation.calls!=101 || observation.incompleteCommands!=100 ||
       observation.declaredVectors[5]!=101u*256u) return 3;
    observation.clearExamples();
    observation.observe(0x6c030000u,0x0404,0,true,std::span<const uint8_t>(source.data(),3));
    if(observation.exampleCount!=1 || observation.examples[0].sourceBytes!=3 ||
       observation.examples[0].source[3]!=0 || observation.examples[0].beforeBytes ||
       observation.examples[0].afterBytes || observation.declaredVectors[12]!=3 || observation.calls!=102) return 4;
    observation.observe(0x50000001,0x0101,0,true);
    if(observation.invalidCommands!=1 || observation.calls!=102) return 5;
    // Every format remains bounded independently, without losing its MODE cross-bin.
    observation.clearExamples();
    for(unsigned f=0;f<16;++f)
        for(unsigned j=0;j<3;++j)observation.observe(((0x60u|f)<<24)|0x00010000u,0x0101,f&3,true);
    if(observation.exampleCount!=32) return 6;
    for(unsigned f=0;f<16;++f)if(observation.examplesPerFormat[f]!=2 || !observation.formatModeCalls[f][f&3])return 7;
    std::ostringstream json; observation.writeJson(json);
    if(json.str().find("format_mode_calls")==std::string::npos || json.str().find("raw_cycle")==std::string::npos) return 8;
}
