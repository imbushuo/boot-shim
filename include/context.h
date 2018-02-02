#pragma once

#include "bl.h"
#include "suptypes.h"

#ifdef _ARM
// Switch to specific context on AArch32 (ARMv7A) platform.
// Reverse engineered from developermenu.efi ARMv7A
void SwitchToRealModeContext(int ptr3, int ptr7);
#endif
