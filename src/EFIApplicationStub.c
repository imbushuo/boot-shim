#include <efi.h>
#include <efilib.h>
#include "lk.h"
#include "elf.h"

BOOLEAN CheckElf32Header(Elf32_Ehdr* header);

BOOLEAN CheckElf32Header(Elf32_Ehdr* bl_elf_hdr)
{

	EFI_PHYSICAL_ADDRESS ElfEntryPoint;
	EFI_STATUS Status = EFI_SUCCESS;

	if (bl_elf_hdr == NULL) return FALSE;

	// Sanity check: Signature
	if (bl_elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		bl_elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		bl_elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		bl_elf_hdr->e_ident[EI_MAG3] != ELFMAG3)
	{
		Print(L"Fail: Invalid ELF magic\n");
		return FALSE;
	}

	// Sanity check: Architecture
	if (bl_elf_hdr->e_machine != EM_ARM)
	{
		Print(L"Fail: Not ARM architecture ELF32 file\n");
		return FALSE;
	}

	// Sanity check: exec
	if (bl_elf_hdr->e_type != ET_EXEC)
	{
		Print(L"Fail: Not EXEC ELF\n");
		return FALSE;
	}

	// Sanity check: entry point and size
	ElfEntryPoint = bl_elf_hdr->e_entry;
	Status = gBS->AllocatePages(AllocateAddress, EfiBootServicesCode, 1, &ElfEntryPoint);
	if (EFI_ERROR(Status))
	{
		Print(L"Fail: Invalid entry point\n");
		return FALSE;
	}

	// Free page allocated
	gBS->FreePages(ElfEntryPoint, 1);

	// Sanity check: program header entries. At least one should present.
	if (bl_elf_hdr->e_phnum < 1)
	{
		Print(L"Fail: Less than one program header entry found\n");
		return FALSE;
	}

	return TRUE;
}

// This is the actual entrypoint.
// Application entrypoint (must be set to 'efi_main' for gnu-efi crt0 compatibility)
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{

	EFI_STATUS Status = EFI_SUCCESS;
	EFI_PHYSICAL_ADDRESS LkEntryPoint = LK_ENTRY_POINT_ADDR;

	UINTN NumHandles = 0;
	EFI_HANDLE *SfsHandles;

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SfsProtocol;

	EFI_FILE_PROTOCOL *FileProtocol;
	EFI_FILE_PROTOCOL *LkFileProtocol;
	CHAR16 *LkFileName = LK_BINARY_NAME;

	UINTN LkFileBufferSize;
	VOID* LkFileBuffer;

	EFI_FILE_INFO *LkFileInformation = NULL;
	UINTN LkFileInformationSize = 0;

	Elf32_Ehdr* lk_elf32_ehdr = NULL;

#if defined(_GNU_EFI)
	InitializeLib(ImageHandle, SystemTable);
#endif

	// Load emmc_appsboot.mbn
	Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&NumHandles,
		&SfsHandles);

	if (EFI_ERROR(Status))
	{
		Print(L"Fail to locate Simple File System Handles\n");
		goto exit;
	}

	for (UINTN index = 0; index < NumHandles; index++)
	{
		Status = gBS->HandleProtocol(
			SfsHandles[index],
			&gEfiSimpleFileSystemProtocolGuid,
			(VOID**) &SfsProtocol
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to invoke HandleProtocol.\n");
			continue;
		}

		Status = SfsProtocol->OpenVolume(
			SfsProtocol,
			&FileProtocol
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Fail to get file protocol handle\n");
			continue;
		}

		Status = FileProtocol->Open(
			FileProtocol,
			&LkFileProtocol,
			LkFileName,
			EFI_FILE_MODE_READ,
			EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to open LK image: %r\n", Status);
			continue;
		}

		// Read image and parse ELF32 file
		Print(L"Opened LK image\n");

		Status = LkFileProtocol->GetInfo(
			LkFileProtocol,
			&gEfiFileInfoGuid,
			&LkFileInformationSize,
			(VOID *) LkFileInformation
		);

		if (Status == EFI_BUFFER_TOO_SMALL)
		{
			Status = gBS->AllocatePool(EfiBootServicesData, LkFileInformationSize, &LkFileInformation);
			if (EFI_ERROR(Status))
			{
				Print(L"Failed to allocate pool for file info: %r\n", Status);
				goto local_cleanup;
			}

			Status = LkFileProtocol->GetInfo(
				LkFileProtocol,
				&gEfiFileInfoGuid,
				&LkFileInformationSize,
				(VOID *)LkFileInformation
			);
		}

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to stat LK image: %r\n", Status);
			goto local_cleanup;
		}

		Print(L"LK image size: 0x%llx\n", LkFileInformation->FileSize);
		if (LkFileInformation->FileSize > UINT32_MAX)
		{
			Print(L"LK image is too large\n");
			goto local_cleanup_free_info;
		}

		LkFileBufferSize = (UINTN) LkFileInformation->FileSize;

		/* Allocate pool for reading file */
		Status = gBS->AllocatePool(EfiBootServicesData, LkFileBufferSize, &LkFileBuffer);
		if (EFI_ERROR(Status))
		{
			Print(L"Failed to allocate pool for file: %r\n", Status);
			goto local_cleanup_free_info;
		}

		/* Read file */
		Status = LkFileProtocol->Read(
			LkFileProtocol,
			&LkFileBufferSize,
			LkFileBuffer
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to read file: %r\n", Status);
			goto local_cleanup_file_pool;
		}

		Print(L"LK read into memory at 0x%x.\n", LkFileBuffer);

		/* Check LK file */
		lk_elf32_ehdr = LkFileBuffer;
		if (!CheckElf32Header(lk_elf32_ehdr))
		{
			Print(L"Cannot load this LK image\n");
			goto local_cleanup_file_pool;
		}

		Print(L"Proceeded to LK image load\n");

		local_cleanup_file_pool:
		gBS->FreePool(LkFileBuffer);

		local_cleanup_free_info:
		gBS->FreePool((VOID *) LkFileInformation);

		local_cleanup:
		Status = LkFileProtocol->Close(LkFileProtocol);
		if (EFI_ERROR(Status))
		{
			Print(L"Failed to close LK image: %r\n", Status);
		}

		// Finished
		break;
	}

exit:
	Print(L"%EWill exit in 5 seconds. %N\n");
	gBS->Stall(5000000);
	return Status;

}
