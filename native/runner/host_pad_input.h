#pragma once
#include <cstdint>
void pollVirtualPadInput();
bool configureHostPadWindow();
bool hostPadWindowRunsAlways();
bool hostPadWindowHiddenRequested();
bool hostPadWindowIsHidden();
void finishHostPadWindowConfiguration();
uint16_t pollHostArrowInput(bool focused);
