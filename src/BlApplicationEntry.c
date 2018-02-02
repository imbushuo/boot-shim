#include <efi.h>
#include <efilib.h>

#include "application.h"

NTSTATUS BlApplicationEntry(
	_In_ PBOOT_APPLICATION_PARAMETER_BLOCK BootAppParameters,
	_In_ PBL_LIBRARY_PARAMETERS LibraryParameters
)
{

	// Get excited
	PBL_LIBRARY_PARAMETERS pLibraryParam;
	PBL_FIRMWARE_DESCRIPTOR FirmwareDescriptor;
	uint32_t ParamPointer;

	if (!BootAppParameters)
	{
		// Invalid parameter
		return STATUS_INVALID_PARAMETER;
	}

	pLibraryParam = LibraryParameters;
	ParamPointer = (uint32_t) BootAppParameters;
	FirmwareDescriptor = (PBL_FIRMWARE_DESCRIPTOR) (ParamPointer + BootAppParameters->FirmwareParametersOffset);

	int ptr3 = (int) pLibraryParam + pLibraryParam[1].HeapAllocationAttributes;
	int ptr7 = *(DWORD *)(ptr3 + 56);

	// Switch mode
	SwitchToRealModeContext(ptr3, ptr7);

	// Do what ever you want now
	if (FirmwareDescriptor->SystemTable)
	{
		FirmwareDescriptor->SystemTable->RuntimeServices->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
	}

	// Do nothing right now
	return STATUS_SUCCESS;

}

