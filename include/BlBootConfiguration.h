#pragma once
#ifndef _BOOTCONFIG_H_

#include <efi.h>
#include <efilib.h>

// Sup types
#include "suptypes.h"

// Bootloader
#include "bl.h"
#include "context.h"

typedef struct _BcdElementType
{
	union {
		ULONG   PackedValue;
		struct {
			ULONG   SubType : 24;
			ULONG   Format : 4;
			ULONG   Class : 4;
		};
	};
} BcdElementType, * PBcdElementType;

EFI_STATUS BlGetBootOptionBoolean(
	PBL_BCD_OPTION List, 
	ULONG Type, 
	BOOLEAN* Value
);

EFI_STATUS BlGetBootOptionInteger(
	PBL_BCD_OPTION List, 
	ULONG Type, 
	UINT64* Value
);

EFI_STATUS BlGetLoadedApplicationEntry(
	PBOOT_APPLICATION_PARAMETER_BLOCK BootAppParameters, 
	PBL_LOADED_APPLICATION_ENTRY BlpApplicationEntry
);
#endif