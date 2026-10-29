#include "host_pad_input.h"
#include "iop_bridge.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <array>
#include <iostream>
#include <chrono>
#include "pad_control_file.h"

// UI thread samples raylib; bridge serializes immutable snapshots to the IOP owner.
// Keyboard is an explicit virtual device, even without a physical gamepad.
bool configureHostPadWindow() {
    if (!std::getenv("DRAKENGARD_PAD_CONTROL")) return false;
    unsigned flags=FLAG_WINDOW_ALWAYS_RUN;
    if(hostPadWindowHiddenRequested())flags|=FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNFOCUSED;
    SetConfigFlags(flags);
    return true;
}
bool hostPadWindowRunsAlways() { return IsWindowState(FLAG_WINDOW_ALWAYS_RUN); }
bool hostPadWindowHiddenRequested() {
    const auto value=std::getenv("DRAKENGARD_DIAGNOSTIC_HIDDEN");
    return std::getenv("DRAKENGARD_PAD_CONTROL") && value && std::strcmp(value,"1")==0;
}
bool hostPadWindowIsHidden() { return IsWindowState(FLAG_WINDOW_HIDDEN); }
void finishHostPadWindowConfiguration() {
    // InitWindow installs its default Escape exit key. A hidden file-driven
    // diagnostic has no keyboard control; its process runner owns the stop cap.
    if(hostPadWindowHiddenRequested())SetExitKey(KEY_NULL);
}
void pollVirtualPadInput() {
    if (!std::getenv("DRAKENGARD_VIRTUAL_PAD")) return;
    if (const char* commandPath=std::getenv("DRAKENGARD_PAD_CONTROL")) {
        static bool wasMinimized=false;
        const bool minimized=IsWindowState(FLAG_WINDOW_MINIMIZED);
        if (minimized!=wasMinimized) {
            std::cerr << "[diagnostic-host] minimized=" << minimized
                      << " always_run=" << hostPadWindowRunsAlways() << '\n';
            wasMinimized=minimized;
        }
        // Explicit diagnostic controller source. This feeds the existing host
        // snapshot/SIO2 path, never guest memory or a Windows input API.
        drakengard::PadControlFileBuffer bytes;
        const auto size=drakengard::readPadControlFile(commandPath,bytes);
        static drakengard::PadControl controller;
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        VirtualPadInput input; input.connected=true;
        const auto command=controller.sampleInput({bytes.data(),size},now);
        input.buttons=command.buttons;input.axes=command.axes;
        constexpr unsigned pressureBits[]{5,7,4,6,12,13,14,15,10,11,8,9};
        for (unsigned i=0;i<12;++i) input.pressure[i]=(input.buttons&(1u<<pressureBits[i]))?0:255;
        static uint16_t previous=0xffff;
        if (input.buttons!=previous) {
            std::cerr << "[host-pad-control] buttons=0x" << std::hex << input.buttons << std::dec << '\n';
            previous=input.buttons;
        }
        drakengard::publishVirtualPadInput(input);
        return;
    }
    const bool focused=IsWindowFocused();
    VirtualPadInput input; input.connected=true; input.cancelPending=!focused;
    input.buttons&=uint16_t(~pollHostArrowInput(focused));
    // GLFW may deliver down and up between two rendered frames. Consume its
    // real key-down queue so a short host tap survives for one input snapshot.
    std::array<bool,512> pressed{};
    for (int key=GetKeyPressed();key;key=GetKeyPressed()) {
        if (key>0 && key<int(pressed.size())) pressed[key]=true;
        static unsigned traced=0;
        if (traced++<32) std::cerr << "[host-pad-key] raylib_key=" << key << '\n';
    }
    if (focused) {
        const bool gamepad=IsGamepadAvailable(0);
        const auto button=[&](unsigned bit,int key,int pad) {
            if (IsKeyDown(key) || pressed.at(key) || (gamepad && IsGamepadButtonDown(0,pad)))
                input.buttons&=uint16_t(~(1u<<bit));
        };
        button(0,KEY_BACKSPACE,GAMEPAD_BUTTON_MIDDLE_LEFT);
        button(1,KEY_TWO,GAMEPAD_BUTTON_LEFT_THUMB); button(2,KEY_FOUR,GAMEPAD_BUTTON_RIGHT_THUMB);
        button(3,KEY_ENTER,GAMEPAD_BUTTON_MIDDLE_RIGHT);
        button(4,KEY_UP,GAMEPAD_BUTTON_LEFT_FACE_UP); button(5,KEY_RIGHT,GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        button(6,KEY_DOWN,GAMEPAD_BUTTON_LEFT_FACE_DOWN); button(7,KEY_LEFT,GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        button(8,KEY_ONE,GAMEPAD_BUTTON_LEFT_TRIGGER_2); button(9,KEY_THREE,GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
        button(10,KEY_Q,GAMEPAD_BUTTON_LEFT_TRIGGER_1); button(11,KEY_E,GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        button(12,KEY_V,GAMEPAD_BUTTON_RIGHT_FACE_UP); button(13,KEY_C,GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        button(14,KEY_X,GAMEPAD_BUTTON_RIGHT_FACE_DOWN); button(15,KEY_Z,GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        const auto axis=[&](unsigned index,int padAxis,int negative,int positive) {
            float value=gamepad ? GetGamepadAxisMovement(0,padAxis) : 0.f;
            if (std::abs(value)<0.15f) value=0;
            if (IsKeyDown(negative)) value=-1; if (IsKeyDown(positive)) value=1;
            input.axes[index]=uint8_t(std::clamp(std::lround(127+(value>0?128:127)*value),0l,255l));
        };
        axis(0,GAMEPAD_AXIS_RIGHT_X,KEY_J,KEY_L); axis(1,GAMEPAD_AXIS_RIGHT_Y,KEY_I,KEY_K);
        axis(2,GAMEPAD_AXIS_LEFT_X,KEY_A,KEY_D); axis(3,GAMEPAD_AXIS_LEFT_Y,KEY_W,KEY_S);
        constexpr unsigned pressureBits[]{5,7,4,6,12,13,14,15,10,11,8,9};
        for (unsigned i=0;i<12;++i) input.pressure[i]=(input.buttons&(1u<<pressureBits[i]))?0:255;
    }
    static uint16_t previous=0xffff;
    if (input.buttons!=previous) {
        std::cerr << "[host-pad-input] buttons=0x" << std::hex << input.buttons << std::dec
                  << " focused=" << focused << '\n';
        previous=input.buttons;
    }
    drakengard::publishVirtualPadInput(input);
}
