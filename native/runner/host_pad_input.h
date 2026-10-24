#pragma once
#include <cstdint>
void pollVirtualPadInput();
bool configureHostPadWindow();
bool hostPadWindowRunsAlways();
uint16_t pollHostArrowInput(bool focused);
