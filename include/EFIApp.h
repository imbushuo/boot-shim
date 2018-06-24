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

/* low level macros for accessing memory mapped hardware registers */
#define REG64(addr) ((volatile uint64_t *)(addr))
#define REG32(addr) ((volatile uint32_t *)(addr))
#define REG16(addr) ((volatile uint16_t *)(addr))
#define REG8(addr) ((volatile uint8_t *)(addr))

#define writel(v, a) (*REG32(a) = (v))
#define readl(a) (*REG32(a))

#define KPSS_BASE                   0xF9000000

#define MSM_GIC_DIST_BASE           KPSS_BASE
#define GIC_DIST_REG(off)           (MSM_GIC_DIST_BASE + (off))

#define GIC_DIST_CTRL               GIC_DIST_REG(0x000)

BOOLEAN CheckElf64Header(Elf64_Ehdr * bl_elf_hdr);
VOID JumpToAddressAArch64(
	EFI_HANDLE ImageHandle,
	EFI_PHYSICAL_ADDRESS Address,
	VOID* PayloadBuffer,
	UINT64 PayloadLength
);

VOID JumpToAddressAArch32(
	EFI_HANDLE ImageHandle,
	EFI_PHYSICAL_ADDRESS AArch32Address,
	EFI_PHYSICAL_ADDRESS AArch64Address,
	VOID* AArch64PayloadBuffer,
	UINT64 AArch64PayloadLength
);

#endif