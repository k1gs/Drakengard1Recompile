#pragma once
#include "ps2_runtime.h"
#include "ps2x/iop/iop_types.h"

namespace drakengard {
void processSifTransfer(PS2Runtime* runtime, uint8_t* ram, const ps2x::iop::SifTransfer& transfer);
bool takeSifReplyInterrupt(PS2Runtime* runtime);
void resetSifChannel(PS2Runtime* runtime);
}
