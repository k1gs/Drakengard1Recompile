#pragma once
#include "runtime/gs/gs_frontend.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <charconv>
#include <string_view>

// Observe the GS presentation buffer, excluding host UI. A nonuniform capture
// after a real GS draw is only a candidate until visually inspected.
class GuestFrameCapture {
    std::filesystem::path output;
    bool drawn{}, saved{}, followupSaved{};
    unsigned framesAfterFirst{};
    unsigned observations{};
    unsigned interval{},frameObservations{},periodicSaved{};
    void savePeriodic(GS& gs) {
        std::vector<uint8_t> rgba; uint32_t width{},height{};
        if (!gs.copyLatchedHostPresentationFrame(rgba,width,height) || !width || !height ||
            width>4096 || height>4096 || uint64_t(width)*height*4!=rgba.size()) return;
        const auto target=output.parent_path()/("periodic-guest-frame-"+std::to_string(frameObservations)+".ppm");
        if (std::filesystem::exists(target)) throw std::runtime_error("Periodic GS output already exists");
        std::ofstream file(target,std::ios::binary);
        file << "P6\n" << width << ' ' << height << "\n255\n";
        for (size_t at=0;at<rgba.size();at+=4) file.write(reinterpret_cast<const char*>(rgba.data()+at),3);
        if (!file) throw std::runtime_error("Could not save periodic GS frame");
        ++periodicSaved;
        std::cerr << "[guest-frame-periodic] observations=" << frameObservations << " path=" << target.string()
                  << "; actual GS pixels, not independent host-window inspection\n";
    }
public:
    GuestFrameCapture() {
        if (const auto* path=std::getenv("DRAKENGARD_FRAME_OUTPUT")) output=path;
        if (const auto* value=std::getenv("DRAKENGARD_FRAME_INTERVAL")) {
            const std::string_view text(value);
            const auto parsed=std::from_chars(text.data(),text.data()+text.size(),interval);
            if (parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || interval<60 || interval>3600)
                throw std::runtime_error("GS capture interval must be 60..3600 observations");
        }
    }
    void enable(GS& gs) { if (!output.empty()) gs.setDebugHistoryPaused(false); }
    void saveFinal(GS& gs) {
        if (output.empty()) return;
        std::vector<uint8_t> rgba; uint32_t width{},height{};
        if (!gs.copyLatchedHostPresentationFrame(rgba,width,height) || !width || !height ||
            uint64_t(width)*height*4!=rgba.size()) return;
        const auto target=output.parent_path()/"last-guest-frame.ppm";
        if (std::filesystem::exists(target)) throw std::runtime_error("Final frame output already exists");
        std::ofstream file(target,std::ios::binary);
        file << "P6\n" << width << ' ' << height << "\n255\n";
        for (size_t at=0;at<rgba.size();at+=4) file.write(reinterpret_cast<const char*>(rgba.data()+at),3);
        if (!file) throw std::runtime_error("Could not save final GS frame");
        std::cerr << "[guest-frame-final] actual latched GS pixels, visual review required\n";
    }
    void observe(GS& gs) {
        if (output.empty()) return;
        if (interval && ++frameObservations%interval==0 && periodicSaved<32) savePeriodic(gs);
        if (followupSaved) return;
        if (saved && ++framesAfterFirst<120) return;
        if (!drawn) for (const auto& event:gs.getDebugHistory())
            if (event.kind==GSDebugEventKind::Draw && event.vertexCount) drawn=true;
        if (!drawn) return;
        std::vector<uint8_t> rgba; uint32_t width{},height{};
        if (!gs.copyLatchedHostPresentationFrame(rgba,width,height) || !width || !height ||
            width>4096 || height>4096 || uint64_t(width)*height*4!=rgba.size()) return;
        unsigned low=255, high=0, visible=0;
        for (size_t at=0;at<rgba.size();at+=4) {
            const unsigned intensity=(std::max)({rgba[at],rgba[at+1],rgba[at+2]});
            low=(std::min)(low,intensity); high=(std::max)(high,intensity);
            visible+=intensity>=32;
        }
        if (++observations%16==0) {
            const auto state=gs.getDebugSnapshot();
            std::cerr << "[guest-frame-levels] low=" << low << " high=" << high
                      << " visible=" << visible << " display_fbp=" << state.hostPresentationDisplayFbp
                      << " source_fbp=" << state.hostPresentationSourceFbp
                      << " preferred=" << state.hostPresentationUsedPreferred << '\n';
        }
        // Reject dark clears and interlace differences as image candidates.
        // This heuristic is only capture selection; it never changes GS pixels.
        if (high-low<16 || visible<64) return;
        const auto target=saved ? output.parent_path()/"review-guest-frame.ppm" : output;
        if (std::filesystem::exists(target)) throw std::runtime_error("Frame candidate output already exists");
        std::ofstream file(target,std::ios::binary);
        file << "P6\n" << width << ' ' << height << "\n255\n";
        for (size_t at=0;at<rgba.size();at+=4) file.write(reinterpret_cast<const char*>(rgba.data()+at),3);
        file.flush();
        if (!file) throw std::runtime_error("Could not save GS frame candidate");
        auto historyPath=target; historyPath.replace_extension(".gs.txt");
        if (std::filesystem::exists(historyPath)) throw std::runtime_error("GS history output already exists");
        std::ofstream history(historyPath);
        for (const auto& e:gs.getDebugHistory()) {
            if (e.kind==GSDebugEventKind::Register && e.reg==1)
                history << "rgbaq 0x" << std::hex << e.regValue << std::dec << '\n';
            if (e.kind==GSDebugEventKind::Draw)
                history << "draw frame=" << e.frame.fbp << " psm=" << unsigned(e.frame.psm)
                        << " tme=" << unsigned(e.prim.tme) << " abe=" << unsigned(e.prim.abe)
                        << " tfx=" << unsigned(e.tex0.tfx) << " alpha=0x" << std::hex << e.alpha << std::dec
                        << " a=" << unsigned(e.aMin) << ',' << unsigned(e.aMax)
                        << " y=" << e.yMin << ',' << e.yMax << '\n';
        }
        if (saved) followupSaved=true; else saved=true;
        std::cerr << "[guest-frame-candidate] width=" << width << " height=" << height
                  << " path=" << target.string() << "; actual GS presentation, visual review required\n";
    }
};
