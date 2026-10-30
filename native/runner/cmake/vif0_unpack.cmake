include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/pinned_patch.cmake")

# Reuse the pinned VIF1 decompression algorithm, not its channel-specific parser.
# The generated helper is shared by the actual runtime and behavioral fixtures.
function(drakengard_prepare_vif0_unpack upstream output)
    file(READ "${upstream}/ps2xRuntime/src/lib/ps2_vif1_interpreter.cpp" source)
    set(begin "            uint8_t vn = (opcode >> 2) & 0x3;")
    string(FIND "${source}" "${begin}" first)
    if(first LESS 0)
        message(FATAL_ERROR "Pinned VIF1 UNPACK start changed")
    endif()
    string(SUBSTRING "${source}" ${first} -1 remainder)
    string(FIND "${remainder}" "\n        }\n        else\n" last)
    if(last LESS 0)
        message(FATAL_ERROR "Pinned VIF1 UNPACK end changed")
    endif()
    string(SUBSTRING "${remainder}" 0 ${last} body)
    string(SHA256 digest "${body}")
    if(NOT digest STREQUAL "1bb7d5152849803116eb97aa8c5cfcb608600273805b95d7760f78ba072328e4")
        message(FATAL_ERROR "Pinned VIF1 UNPACK algorithm changed")
    endif()
    drakengard_replace_pinned(body
        "            if ((imm & 0x8000u) != 0u)\n                vuAddr = (vuAddr + (vif1_regs.tops & 0x3FFu)) & 0x3FFu;"
        "            // VIF0 has no TOPS-relative double buffering."
        "VIF0 absolute destination")
    string(REPLACE "vif1_regs" "registers" body "${body}")
    string(REPLACE "m_vu1Data" "vuData" body "${body}")
    string(REPLACE "PS2_VU1_DATA_SIZE" "4096u" body "${body}")
    string(REPLACE "0x3FFu" "0xFFu" body "${body}")
    # Reserved formats must never enter the pinned decoder's legacy raw fallback.
    drakengard_replace_pinned(body "            const bool maskEnable = (opcode & 0x10u) != 0u;"
        "            if(vl==3u && vn!=3u)throw std::runtime_error(\"Reserved VIF0 UNPACK format\");\n            const bool maskEnable = (opcode & 0x10u) != 0u;"
        "VIF0 reserved format rejection")
    drakengard_replace_pinned(body "            if (vuData && totalBytes > 0 && pos + totalBytes <= sizeBytes)"
        "            observer(false,totalBytes);\n            if (vuData && totalBytes > 0 && pos + totalBytes <= sizeBytes)"
        "VIF0 pre unpack callback")
    drakengard_replace_pinned(body "            pos += totalBytes;"
        "            observer(true,totalBytes);\n            pos += totalBytes;"
        "VIF0 post unpack callback")
    file(CONFIGURE OUTPUT "${output}" CONTENT "#pragma once\n#include <cstdint>\n#include <cstring>\n#include <stdexcept>\nnamespace drakengard {\ntemplate<class Registers,class Observer> inline void unpackVif0(uint32_t cmd,Registers& registers,const uint8_t* data,uint32_t sizeBytes,uint32_t& pos,uint8_t* vuData,Observer observer) {\n    const uint8_t opcode=(cmd>>24)&0x7fu,num=(cmd>>16)&0xffu;\n    const uint16_t imm=cmd&0xffffu;\n    do {\n${body}\n    } while(false);\n}\n}\n" @ONLY)
endfunction()

function(drakengard_adapt_vif0_unpack variable)
    set(text "${${variable}}")
    set(begin "            const uint8_t vn = static_cast<uint8_t>((opcode >> 2) & 0x3u);")
    string(FIND "${text}" "${begin}" first)
    if(first LESS 0)
        message(FATAL_ERROR "Pinned VIF0 UNPACK start changed")
    endif()
    string(SUBSTRING "${text}" ${first} -1 remainder)
    string(FIND "${remainder}" "\n        }\n        else\n" last)
    if(last LESS 0)
        message(FATAL_ERROR "Pinned VIF0 UNPACK end changed")
    endif()
    string(SUBSTRING "${remainder}" 0 ${last} body)
    string(SHA256 digest "${body}")
    if(NOT digest STREQUAL "4b8a20c7458a40489f7fd361b32ad76baf80ae4b0807e7c02535e5fe43f56d78")
        message(FATAL_ERROR "Pinned VIF0 skipped-format algorithm changed")
    endif()
    string(REPLACE "${body}"
        "            drakengard::unpackVif0(cmd,vif0_regs,data,sizeBytes,pos,m_vu0Data,[](bool,uint32_t){});\n            continue;"
        text "${text}")
    set(${variable} "${text}" PARENT_SCOPE)
endfunction()
