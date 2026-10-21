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
file(CONFIGURE OUTPUT "${graphics_file}"
    CONTENT "#include \"gs_scanout.h\"\n#include \"gs_color_diagnostic.h\"\n${graphics_diagnostic}" @ONLY)
set_source_files_properties("${graphics_file}" TARGET_DIRECTORY ps2_runtime PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/ps2xRuntime/src/lib/gs")
get_target_property(runtime_sources ps2_runtime SOURCES)
list(FILTER runtime_sources EXCLUDE REGEX "/gs_cpu_backend[.]cpp$")
set_property(TARGET ps2_runtime PROPERTY SOURCES "${runtime_sources}")
target_sources(ps2_runtime PRIVATE "${graphics_file}")
