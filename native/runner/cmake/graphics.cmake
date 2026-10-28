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
    "    const auto metricStart = drakengard::beginGsDrawMetric();\n    drakengard::GsFragmentScope fragments(batch);\n    DrawPrimitive(batch);\n    fragments.complete();\n    drakengard::completeGsDrawMetric(metricStart, batch);"
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
include("${CMAKE_CURRENT_LIST_DIR}/pinned_patch.cmake")
string(FIND "${graphics_clut_measured}" "void GSCpuBackend::WritePixel(" fragment_begin)
string(FIND "${graphics_clut_measured}" "\nuint32_t GSCpuBackend::LookupCLUT" fragment_end)
if(fragment_begin LESS 0 OR fragment_end LESS fragment_begin)
    message(FATAL_ERROR "Pinned GS pixel observation boundary changed")
endif()
math(EXPR fragment_length "${fragment_end} - ${fragment_begin}")
string(SUBSTRING "${graphics_clut_measured}" ${fragment_begin} ${fragment_length} fragment_body)
string(SHA256 fragment_hash "${fragment_body}")
if(NOT fragment_hash STREQUAL "aa63f006cd83bab547b5cd80bfe8da09823b02df87584efdee433db251408a02")
    message(FATAL_ERROR "Pinned GS pixel body changed")
endif()
set(fragment_observed "${fragment_body}")
drakengard_replace_pinned(fragment_observed
    "    const auto &ctx = state.context;"
    "    drakengard::observeGsFragment(drakengard::GsFragmentProbe::Enter);\n    const auto &ctx = state.context;" "GS fragment stage 0")
drakengard_replace_pinned(fragment_observed
    "    if (x < ctx.scissor.x0 || x > ctx.scissor.x1 || y < ctx.scissor.y0 || y > ctx.scissor.y1)\n        return;"
    "    if (x < ctx.scissor.x0 || x > ctx.scissor.x1 || y < ctx.scissor.y0 || y > ctx.scissor.y1) {\n        drakengard::observeGsFragment(drakengard::GsFragmentProbe::ScissorReject);\n        return;\n    }" "GS fragment stage 1")
drakengard_replace_pinned(fragment_observed
    "    if (!writeMask.writesAnything())"
    "    if(drakengard::gsFragmentProbe) drakengard::gsFragmentProbe->mask(writeMask.writeRgb,writeMask.writeAlpha,writeMask.writeDepth);\n    if (!writeMask.writesAnything())" "GS fragment stage 2")
drakengard_replace_pinned(fragment_observed
    "    if (!writeMask.writesAnything())\n    {"
    "    if (!writeMask.writesAnything())\n    {\n        drakengard::observeGsFragment(drakengard::GsFragmentProbe::AlphaReject);" "GS fragment stage 3")
drakengard_replace_pinned(fragment_observed
    "    if (!passesDestinationAlphaTest(ctx.test, static_cast<uint8_t>(fpsm), rawFramebufferPixel))\n    {"
    "    if (!passesDestinationAlphaTest(ctx.test, static_cast<uint8_t>(fpsm), rawFramebufferPixel))\n    {\n        drakengard::observeGsFragment(drakengard::GsFragmentProbe::DestinationReject);" "GS fragment stage 4")
drakengard_replace_pinned(fragment_observed
    "    if (!zpass)\n    {"
    "    if(drakengard::gsFragmentProbe) drakengard::gsFragmentProbe->testDepth(static_cast<uint32_t>(z),storedZ,zpass);\n    if (!zpass)\n    {\n        drakengard::observeGsFragment(drakengard::GsFragmentProbe::DepthReject);" "GS fragment stage 5")
drakengard_replace_pinned(fragment_observed
    "        WriteVramUnlocked(fpsm, fbp, fbw, x, y, pixel);"
    "        WriteVramUnlocked(fpsm, fbp, fbw, x, y, pixel);\n        drakengard::observeGsFragment(drakengard::GsFragmentProbe::FrameWrite);" "GS fragment stage 6")
drakengard_replace_pinned(fragment_observed
    "        WriteVramUnlocked(zpsm, zbp, fbw, x, y, z);"
    "        WriteVramUnlocked(zpsm, zbp, fbw, x, y, z);\n        drakengard::observeGsFragment(drakengard::GsFragmentProbe::DepthWrite);" "GS fragment stage 7")
string(REPLACE "${fragment_body}" "${fragment_observed}" graphics_clut_measured "${graphics_clut_measured}")
drakengard_replace_pinned(graphics_clut_measured
    "                const TextureCombineResult color = combineTexture(tex, shadeR, shadeG, shadeB, shadeA, tr, tg, tb, ta);"
    "                const TextureCombineResult color = combineTexture(tex, shadeR, shadeG, shadeB, shadeA, tr, tg, tb, ta);\n                if(drakengard::gsFragmentProbe) drakengard::gsFragmentProbe->texture(ta,color.a);" "GS triangle texture alpha observation")
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
drakengard_replace_pinned(frontend_source
    "        recordGifTagDebugEventUnlocked(sizeBytes, nloop, flg, nreg);"
    "        recordGifTagDebugEventUnlocked(sizeBytes, nloop, flg, nreg);\n        drakengard::observeGsGifTag(offset-16u,tagLo,tagHi);"
    "active GIF tag sample")
drakengard_replace_pinned(frontend_source
    "                    writeRegisterPacked(regs[r], lo, hi);"
    "                    drakengard::gsCurrentGifDataOffset=offset-16u;\n                    writeRegisterPacked(regs[r], lo, hi);"
    "packed GIF data offset")
drakengard_replace_pinned(frontend_source
    "                    writeRegisterUnlocked(regs[r], loadLE64(data + offset));"
    "                    drakengard::gsCurrentGifDataOffset=offset;\n                    writeRegisterUnlocked(regs[r], loadLE64(data + offset));"
    "register GIF data offset")
set(frontend_file "${CMAKE_BINARY_DIR}/private_sampled_GsFrontend.cpp")
file(CONFIGURE OUTPUT "${frontend_file}"
    CONTENT "#include \"gs_production_metrics.h\"\n${frontend_source}" @ONLY)
set_source_files_properties("${frontend_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/gs")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/gs_frontend[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${frontend_file}")

file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/gs/ps2_gif_arbiter.cpp" arbiter_source)
drakengard_replace_pinned(arbiter_source
    "            m_processFn(pkt.data.data(), static_cast<uint32_t>(pkt.data.size()));"
    "            drakengard::GsGifPacketScope packetScope(unsigned(pkt.pathId),pkt.data.data(),static_cast<uint32_t>(pkt.data.size()));\n            m_processFn(pkt.data.data(), static_cast<uint32_t>(pkt.data.size()));"
    "GIF packet source scope")
set(arbiter_file "${CMAKE_BINARY_DIR}/private_observed_GifArbiter.cpp")
drakengard_replace_pinned(arbiter_source
    "    m_queue.push_back(std::move(pkt));"
    "    if(unsigned(pathId)==3u) drakengard::relayGsDmaSource(data,sizeBytes,pkt.data.data());\n    if(unsigned(pathId)==1u) drakengard::relayGsKickOrigin(data,sizeBytes,pkt.data.data());\n    m_queue.push_back(std::move(pkt));"
    "GIF source copied buffer identity")
file(CONFIGURE OUTPUT "${arbiter_file}"
    CONTENT "#include \"gs_production_metrics.h\"\n${arbiter_source}" @ONLY)
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/ps2_gif_arbiter[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${arbiter_file}")

file(READ "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/ps2_vif1_interpreter.cpp" vif_source)
drakengard_replace_pinned(vif_source
    "            if (m_vu1Data && totalBytes > 0 && pos + totalBytes <= sizeBytes)"
    "            if (drakengard::gsMetricPath()) { const bool complete=m_vu1Data && totalBytes>0 && pos+totalBytes<=sizeBytes; drakengard::observeVifUnpack(cmd,vif1_regs.cycle,vif1_regs.mode,complete,data+pos,complete?totalBytes:0u); }\n            if (m_vu1Data && totalBytes > 0 && pos + totalBytes <= sizeBytes)"
    "VIF1 UNPACK command observation")
set(vif_file "${CMAKE_BINARY_DIR}/private_observed_Vif1.cpp")
file(CONFIGURE OUTPUT "${vif_file}" CONTENT "#include \"gs_production_metrics.h\"\n${vif_source}" @ONLY)
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/ps2_vif1_interpreter[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${vif_file}")
