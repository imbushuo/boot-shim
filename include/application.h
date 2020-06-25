#pragma once

// Sup types
#include "suptypes.h"

// Bootloader
#include "bl.h"
#include "context.h"

// Status enum
#include "ntstatus.h"

// BCD
#include "BlBootConfiguration.h"

// Exports
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS EFIApp_Main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable, PBOOT_APPLICATION_PARAMETER_BLOCK BootAppParameters);