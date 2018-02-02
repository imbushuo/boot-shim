#include <efi.h>
#include <efilib.h>

// This is the actual entrypoint.
// Application entrypoint (must be set to 'efi_main' for gnu-efi crt0 compatibility)
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	UINTN Event;

#if defined(_GNU_EFI)
	InitializeLib(ImageHandle, SystemTable);
#endif

	/*
	* In addition to the standard %-based flags, Print() supports the following:
	*   %N       Set output attribute to normal
	*   %H       Set output attribute to highlight
	*   %E       Set output attribute to error
	*   %B       Set output attribute to blue color
	*   %V       Set output attribute to green color
	*   %r       Human readable version of a status code
	*/
	Print(L"\n%H*** ~ Never gonna give up up ~ ***%N\n");
	Print(L"*** ~ Never gonna let you down ~ ***\n\n");

	Print(L"%E This device will restart in 30 seconds. %N\n");
	SystemTable->BootServices->Stall(30000000);

	return EFI_SUCCESS;
}
