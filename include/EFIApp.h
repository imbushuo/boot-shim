#pragma once
#ifndef _EFIAPP_H_

#include <efi.h>
#include <efilib.h>

#include "lk.h"
#include "elf.h"
#include "ProcessorSupport.h"

// Util for Stall
#ifndef _EFI_STALL_UTIL_
#define _EFI_STALL_UTIL_
#define SECONDS_TO_MICROSECONDS(x) x * 1000000
#endif

BOOLEAN CheckElf32Header(Elf32_Ehdr* header);
VOID JumpToAddress(EFI_HANDLE ImageHandle, uint32_t addr);

#endif