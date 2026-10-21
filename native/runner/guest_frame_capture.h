#pragma once
#include "runtime/gs/gs_frontend.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

// Observe the GS presentation buffer, excluding host UI. A nonuniform capture
// after a real GS draw is only a candidate until visually inspected.
class GuestFrameCapture {
    std::filesystem::path output;
    bool drawn{}, saved{}, followupSaved{};
    unsigned framesAfterFirst{};
    unsigned observations{};
public:
    GuestFrameCapture() {
        if (const auto* path=std::getenv("DRAKENGARD_FRAME_OUTPUT")) output=path;
    }
    void enable(GS& gs) { if (!output.empty()) gs.setDebugHistoryPaused(false); }
    void observe(GS& gs) {
        if (output.empty() || followupSaved) return;
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
        if (saved) followupSaved=true; else saved=true;
        std::cerr << "[guest-frame-candidate] width=" << width << " height=" << height
                  << " path=" << target.string() << "; actual GS presentation, visual review required\n";
    }
};
