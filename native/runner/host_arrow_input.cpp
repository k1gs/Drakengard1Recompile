#include "host_pad_input.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// Observe physical/OS virtual-key state, never inject events. This also handles
// arrow events whose scan code is not recognized by GLFW. Discard unfocused
// edges so switching to the game does not replay old desktop key presses.
uint16_t pollHostArrowInput(bool focused) {
    uint16_t mask=0;
#ifdef _WIN32
    static bool previouslyFocused=false;
    constexpr int keys[]{VK_UP,VK_RIGHT,VK_DOWN,VK_LEFT};
    for (unsigned i=0;i<4;++i) {
        const auto state=GetAsyncKeyState(keys[i]);
        if (focused && ((state&0x8000) || (previouslyFocused && (state&1))))
            mask|=uint16_t(1u<<(4+i));
    }
    previouslyFocused=focused;
#else
    (void)focused;
#endif
    return mask;
}
