#include <efi.h>
#include <efilib.h>

// This is the actual entrypoint.
// Application entrypoint (must be set to 'efi_main' for gnu-efi crt0 compatibility)
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{

	UINTN NumHandles;
	EFI_STATUS status = EFI_SUCCESS;
	EFI_HANDLE *SFS_Handles;
	EFI_BLOCK_IO_PROTOCOL *BlkIo;
	EFI_DEVICE_PATH_PROTOCOL *FilePath;
	EFI_LOADED_IMAGE_PROTOCOL *ImageInfo;
	EFI_HANDLE AppImageHandle = NULL;
	CHAR16 *FileName = L"Menu.efi";
	UINTN ExitDataSize;

#if defined(_GNU_EFI)
	InitializeLib(ImageHandle, SystemTable);
#endif

	Print(L"*** Attempt to load Menu.efi. ***\n\n");
	Print(L"*** Locating handle buffers. ***\n\n");

	status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&NumHandles,
		&SFS_Handles);

	if (status != EFI_SUCCESS) 
	{
		Print(L"Could not find handles - %r\n", status);
		goto exit;
	}

	Print(L"*** Attempting to open protocols.. ***\n\n");
	for (UINTN index = 0; index < NumHandles; index++) 
	{
		status = gBS->OpenProtocol(
			SFS_Handles[index],
			&gEfiSimpleFileSystemProtocolGuid,
			(VOID**)&BlkIo,
			ImageHandle,
			NULL,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);

		if (status != EFI_SUCCESS) {
			Print(L"Protocol is not supported - %r\n", status);
			goto exit;
		}

		FilePath = FileDevicePath(SFS_Handles[index], FileName);
		status = gBS->LoadImage(
			FALSE,
			ImageHandle,
			FilePath,
			(VOID*)NULL,
			0,
			&AppImageHandle);

		if (status != EFI_SUCCESS) {
			Print(L"Could not load the image - %r\n", status);
			continue;
		}

		Print(L"Loaded the image with success\n");
		status = gBS->OpenProtocol(
			AppImageHandle,
			&gEfiLoadedImageProtocolGuid,
			(VOID**) &ImageInfo,
			ImageHandle,
			(VOID*)NULL,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL
		);

		Print(L"ImageInfo opened\n");

		if (!EFI_ERROR(status)) {
			Print(L"ImageSize = %d\n", ImageInfo->ImageSize);
		}

		Print(L"Image start:\n");
		status = gBS->StartImage(AppImageHandle, &ExitDataSize, (CHAR16**) NULL);
		if (status != EFI_SUCCESS) {
			Print(L"Could not start the image - %r %x\n", status, status);
			Print(L"Exit data size: %d\n", ExitDataSize);
			continue;
		}

	}

exit:
	Print(L"%E Will exit in 10 seconds. %N\n");
	SystemTable->BootServices->Stall(10000000);
	return status;

}
