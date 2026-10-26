# Correct PCRTC framebuffer reads in a build-local copy of the pinned backend.
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp" graphics_source)
set(copy_begin "        selected = displayFrame;\n        pixels.clear();")
string(REPLACE "${copy_begin}"
    "        const uint32_t readHeight=gsScanoutReadHeight(height,request.smode2);\n${copy_begin}"
    graphics_next "${graphics_source}")
if(graphics_next STREQUAL graphics_source)
    message(FATAL_ERROR "Pinned GS scanout source contract changed")
endif()
set(graphics_source "${graphics_next}")
foreach(frame IN ITEMS "request.preferredSource" "displayFrame" "candidate")
    string(REPLACE "CopyFrameToHostRgba(${frame}, width, height,"
        "CopyFrameToHostRgba(${frame}, width, readHeight," graphics_next "${graphics_source}")
    if(graphics_next STREQUAL graphics_source)
        message(FATAL_ERROR "Pinned GS framebuffer read changed: ${frame}")
    endif()
    set(graphics_source "${graphics_next}")
endforeach()
set(copy_end "        return true;\n    };\n\n    if (valid1 && valid2)")
string(REPLACE "${copy_end}"
    "        expandGsFrameScanout(pixels,kHostFrameWidth*4u,height,request.smode2);\n${copy_end}"
    graphics_next "${graphics_source}")
if(graphics_next STREQUAL graphics_source)
    message(FATAL_ERROR "Pinned GS scanout completion changed")
endif()
set(graphics_file "${CMAKE_BINARY_DIR}/private_scanout_GsCpuBackend.cpp")
set(color_anchor "                const TextureCombineResult color = combineTexture(tex, r, g, b, a, tr, tg, tb, ta);")
string(REPLACE "${color_anchor}"
    "${color_anchor}\n                if ((x%16)==0 && (y%4)==0) observeGsColor(state,r,g,b,a,texel,color.r,color.g,color.b,color.a);"
    graphics_diagnostic "${graphics_next}")
if(graphics_diagnostic STREQUAL graphics_next)
    message(FATAL_ERROR "Pinned GS sprite color diagnostic point changed")
endif()
# Pin the complete old four-channel arithmetic before replacing its body.
string(FIND "${graphics_diagnostic}" "    const uint8_t r = lerpChannel(" bilinear_begin)
string(FIND "${graphics_diagnostic}" "\n}\n\nvoid GSCpuBackend::DrawSprite" bilinear_end)
if(bilinear_begin LESS 0 OR bilinear_end LESS bilinear_begin)
    message(FATAL_ERROR "Pinned GS bilinear function boundary changed")
endif()
math(EXPR bilinear_length "${bilinear_end} - ${bilinear_begin}")
string(SUBSTRING "${graphics_diagnostic}" ${bilinear_begin} ${bilinear_length} bilinear_channels)
string(SHA256 bilinear_hash "${bilinear_channels}")
if(NOT bilinear_hash STREQUAL "bd835128f1eb8ffbf87d5094768ff304dde8fc22da0858aec8310216ad478d55")
    message(FATAL_ERROR "Pinned GS bilinear interpolation contract changed")
endif()
string(REPLACE "${bilinear_channels}"
    "    return drakengard::filterGsBilinearRgba(c00, c10, c01, c11, fx, fy);"
    graphics_filtered "${graphics_diagnostic}")
# The observed T8/CT32/CSA0 path indexes the already loaded CLUT directly.
# Read both current halfword banks on every sample; no palette cache is added.
set(clut_lookup_anchor "            return LookupCLUT(state, static_cast<u8>(out), tex.cpsm, tex.csm, tex.csa, tex.psm);")
string(REPLACE "${clut_lookup_anchor}"
    "            if (tex.psm == GS_PSM_T8 && tex.cpsm == GS_PSM_CT32 && tex.csa == 0u) {\n                const auto index = static_cast<u8>(out);\n                return uint32_t(m_clut[index]) | (uint32_t(m_clut[index + 256u]) << 16u);\n            }\n${clut_lookup_anchor}"
    graphics_clut_fast "${graphics_filtered}")
if(graphics_clut_fast STREQUAL graphics_filtered)
    message(FATAL_ERROR "Pinned GS indexed texture sample point changed")
endif()
string(REPLACE "    DrawPrimitive(batch);"
    "    const auto metricStart = drakengard::beginGsDrawMetric();\n    DrawPrimitive(batch);\n    drakengard::completeGsDrawMetric(metricStart, batch);"
    graphics_measured "${graphics_clut_fast}")
if(graphics_measured STREQUAL graphics_clut_fast)
    message(FATAL_ERROR "Pinned GS completed-draw metric point changed")
endif()
string(REPLACE "void GSCpuBackend::LoadClutUnlocked(const GSTex0Reg &tex0, const GSTexClutReg &texclut)\n{"
    "void GSCpuBackend::LoadClutUnlocked(const GSTex0Reg &tex0, const GSTexClutReg &texclut)\n{\n    drakengard::observeGsClutLoad();"
    graphics_clut_measured "${graphics_measured}")
if(graphics_clut_measured STREQUAL graphics_measured)
    message(FATAL_ERROR "Pinned GS CLUT-load metric point changed")
endif()
file(CONFIGURE OUTPUT "${graphics_file}"
    CONTENT "#include \"gs_production_metrics.h\"\n#include \"gs_bilinear.h\"\n#include \"gs_scanout.h\"\n#include \"capture/gs_color_diagnostic.h\"\n${graphics_clut_measured}" @ONLY)
set_source_files_properties("${graphics_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/gs")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/gs_cpu_backend[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${graphics_file}")

# Observe the original XYZ packets immediately before vertexKick; preserve all decoding.
include("${CMAKE_CURRENT_LIST_DIR}/pinned_patch.cmake")
file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/gs/gs_frontend.cpp" frontend_source)
drakengard_replace_pinned(frontend_source
    "        vtx.fog = f;\n        vertexKick(!adk);"
    "        vtx.fog = f;\n        drakengard::observeGsRawVertex(0x104u,lo,hi);\n        vertexKick(!adk);" "packed XYZF2 sample")
drakengard_replace_pinned(frontend_source
    "        vtx.fog = m_curFog;\n        vertexKick(!adk);"
    "        vtx.fog = m_curFog;\n        drakengard::observeGsRawVertex(0x105u,lo,hi);\n        vertexKick(!adk);" "packed XYZ2 sample")
drakengard_replace_pinned(frontend_source
    "        vtx.fog = static_cast<uint8_t>((hi >> 36) & 0xFF);\n        vertexKick(false);"
    "        vtx.fog = static_cast<uint8_t>((hi >> 36) & 0xFF);\n        drakengard::observeGsRawVertex(0x10cu,lo,hi);\n        vertexKick(false);" "packed XYZF3 sample")
drakengard_replace_pinned(frontend_source
    "        vtx.fog = m_curFog;\n        vertexKick(false);"
    "        vtx.fog = m_curFog;\n        drakengard::observeGsRawVertex(0x10du,lo,hi);\n        vertexKick(false);" "packed XYZ3 sample")
foreach(vertex_register IN ITEMS XYZF2 XYZ2)
    drakengard_replace_pinned(frontend_source
        "        vertexKick(regAddr == GS_REG_${vertex_register});"
        "        drakengard::observeGsRawVertex(0x200u|regAddr,value,0);\n        vertexKick(regAddr == GS_REG_${vertex_register});"
        "register ${vertex_register} sample")
endforeach()
set(frontend_file "${CMAKE_BINARY_DIR}/private_sampled_GsFrontend.cpp")
file(CONFIGURE OUTPUT "${frontend_file}"
    CONTENT "#include \"gs_production_metrics.h\"\n${frontend_source}" @ONLY)
set_source_files_properties("${frontend_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/gs")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/gs_frontend[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${frontend_file}")
