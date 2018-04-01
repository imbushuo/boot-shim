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

ElfVerifierStatus CheckElf32Header(Elf32_Ehdr* header);
ElfVerifierStatus CheckElf64Header(Elf64_Ehdr* header);

VOID Arm32JumpToAddress(EFI_HANDLE ImageHandle, uint32_t addr);

EFI_STATUS Elf32Load(EFI_HANDLE ImageHandle, VOID* PayloadFileBuffer);
EFI_STATUS Elf64Load(EFI_HANDLE ImageHandle, VOID* PayloadFileBuffer);

#endif