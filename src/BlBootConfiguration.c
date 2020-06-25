#include <efi.h>
#include <efilib.h>

#include "application.h"

EFI_STATUS MiscGetBootOption(
	_In_ PBL_BCD_OPTION List,
	_In_ ULONG Type)
{
	uint32_t NextOption = 0, ListOption;
	PBL_BCD_OPTION Option, FoundOption;

	/* No options, bail out */
	if (!List)
	{
		return EFI_SUCCESS;
	}

	/* Loop while we find an option */
	FoundOption = NULL;
	do
	{
		/* Get the next option and see if it matches the type */
		Option = (PBL_BCD_OPTION)((uint32_t)List + NextOption);
		if ((Option->Type == Type) && !(Option->Empty))
		{
			FoundOption = Option;
			break;
		}

		/* Store the offset of the next option */
		NextOption = Option->NextEntryOffset;

		/* Failed to match. Check for list options */
		ListOption = Option->ListOffset;
		if (ListOption)
		{
			/* Try to get a match in the associated option */
			Option = MiscGetBootOption((PBL_BCD_OPTION)((uint32_t)Option +
				ListOption),
				Type);
			if (Option)
			{
				/* Return it */
				FoundOption = Option;
				break;
			}
		}
	} while (NextOption);

	/* Return the option that was found, if any */
	return FoundOption;
}

EFI_STATUS BlGetLoadedApplicationEntry(
	_In_ PBOOT_APPLICATION_PARAMETER_BLOCK BootAppParameters,
	_Out_ PBL_LOADED_APPLICATION_ENTRY BlpApplicationEntry)
{
	PBL_APPLICATION_ENTRY AppEntry;
	uint32_t ParamPointer;

	ParamPointer = (uint32_t)BootAppParameters;
	AppEntry = (PBL_APPLICATION_ENTRY)(ParamPointer + BootAppParameters->AppEntryOffset);

	/* Check if the caller sent us their internal BCD options */
	if (AppEntry->Flags & BL_APPLICATION_ENTRY_BCD_OPTIONS_INTERNAL)
	{
		/* These are external to us now, as far as we are concerned */
		AppEntry->Flags &= ~BL_APPLICATION_ENTRY_BCD_OPTIONS_INTERNAL;
		AppEntry->Flags |= BL_APPLICATION_ENTRY_BCD_OPTIONS_EXTERNAL;
	}

	/* Save the application entry flags */
	BlpApplicationEntry->Flags = AppEntry->Flags;

	/* Copy the GUID and point to the options */
	BlpApplicationEntry->EFI_GUID = AppEntry->EFI_GUID;
	BlpApplicationEntry->BcdData = &AppEntry->BcdData;

	return EFI_SUCCESS;
}

EFI_STATUS BlGetBootOptionInteger(
	_In_ PBL_BCD_OPTION List,
	_In_ ULONG Type,
	_Out_ UINT64* Value)
{
	NTSTATUS Status;
	PBL_BCD_OPTION Option;
	BcdElementType ElementType;

	/* Make sure this is a BCD_TYPE_INTEGER */
	ElementType.PackedValue = Type;
	if (ElementType.Format != 0x05)
	{
		return STATUS_INVALID_PARAMETER;
	}

	/* Return the data */
	Option = MiscGetBootOption(List, Type);
	if (Option)
	{
		*Value = *(UINT64*)((uint32_t)Option + Option->DataOffset);
	}

	/* Option found */
	Status = Option ? STATUS_SUCCESS : STATUS_NOT_FOUND;

	return Status;
}

EFI_STATUS BlGetBootOptionBoolean(
	_In_ PBL_BCD_OPTION List,
	_In_ ULONG Type,
	_Out_ BOOLEAN* Value)
{
	NTSTATUS Status;
	PBL_BCD_OPTION Option;
	BcdElementType ElementType;

	/* Make sure this is a BCD_TYPE_BOOLEAN */
	ElementType.PackedValue = Type;
	if (ElementType.Format != 0x06)
	{
		return STATUS_INVALID_PARAMETER;
	}

	/* Return the data */
	Option = MiscGetBootOption(List, Type);
	if (Option)
	{
		*Value = *(BOOLEAN*)((uint32_t)Option + Option->DataOffset);
	}

	/* Option found */
	Status = Option ? STATUS_SUCCESS : STATUS_NOT_FOUND;

	return Status;
}