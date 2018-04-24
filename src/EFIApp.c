#include "EFIApp.h"

VOID JumpToAddress(
	EFI_HANDLE ImageHandle, 
	uint32_t addr
)
{

	EFI_STATUS Status;
	UINTN MemMapSize = 0;
	EFI_MEMORY_DESCRIPTOR* MemMap = 0;
	UINTN MapKey = 0;
	UINTN DesSize = 0;
	UINT32 DesVersion = 0;

	/* Entry */
	VOID(*entry)() = (VOID*) addr;

	gBS->GetMemoryMap(
		&MemMapSize, 
		MemMap, 
		&MapKey, 
		&DesSize, 
		&DesVersion
	);

	/* Shutdown */
	Status = gBS->ExitBootServices(
		ImageHandle, 
		MapKey
	);

	if (EFI_ERROR(Status))
	{
		Print(L"Failed to exit BS\n");
		return;
	}

	/* De-initialize */
	ArmDeInitialize();

	/* Lets go */
	entry();

}

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
	Status = gBS->AllocatePages(
		AllocateAddress, 
		EfiLoaderCode, 
		1, 
		&ElfEntryPoint
	);

	if (EFI_ERROR(Status))
	{
		Print(L"Fail: Invalid entry point\n");
		return FALSE;
	}

	// Free page allocated
	gBS->FreePages(
		ElfEntryPoint, 
		1
	);

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
EFI_STATUS efi_main(
	EFI_HANDLE ImageHandle, 
	EFI_SYSTEM_TABLE *SystemTable
)
{

	EFI_STATUS Status = EFI_SUCCESS;
	
	UINTN NumHandles = 0;
	EFI_HANDLE *SfsHandles;

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *EfiSfsProtocol;
	EFI_FILE_PROTOCOL *FileProtocol;
	EFI_FILE_PROTOCOL *PayloadFileProtocol;
	CHAR16 *PayloadFileName = PAYLOAD_BINARY_NAME;

	EFI_PHYSICAL_ADDRESS LkEntryPoint = PAYLOAD_ENTRY_POINT_ADDR_INVALID;
	UINTN PayloadFileBufferSize;
	UINTN PayloadLoadPages;
	VOID* PayloadFileBuffer;
	VOID* PayloadLoadSec;

	EFI_FILE_INFO *PayloadFileInformation = NULL;
	UINTN PayloadFileInformationSize = 0;

	Elf32_Ehdr* PayloadElf32Ehdr = NULL;
	Elf32_Phdr* PayloadElf32Phdr = NULL;

	UINTN PayloadSectionOffset = 0;
	UINTN PayloadLength = 0;

#if defined(_GNU_EFI)
	InitializeLib(
		ImageHandle, 
		SystemTable
	);
#endif

	// Load emmc_appsboot.mbn
	Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&NumHandles,
		&SfsHandles
	);

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
			(VOID**) &EfiSfsProtocol
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to invoke HandleProtocol.\n");
			continue;
		}

		Status = EfiSfsProtocol->OpenVolume(
			EfiSfsProtocol,
			&FileProtocol
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Fail to get file protocol handle\n");
			continue;
		}

		Status = FileProtocol->Open(
			FileProtocol,
			&PayloadFileProtocol,
			PayloadFileName,
			EFI_FILE_MODE_READ,
			EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to open payload image: %r\n", Status);
			continue;
		}

		// Read image and parse ELF32 file
		Print(L"Opened payload image\n");

		Status = PayloadFileProtocol->GetInfo(
			PayloadFileProtocol,
			&gEfiFileInfoGuid,
			&PayloadFileInformationSize,
			(VOID *) PayloadFileInformation
		);

		if (Status == EFI_BUFFER_TOO_SMALL)
		{
			Status = gBS->AllocatePool(
				EfiLoaderData, 
				PayloadFileInformationSize, 
				&PayloadFileInformation
			);

			if (EFI_ERROR(Status))
			{
				Print(L"Failed to allocate pool for file info: %r\n", Status);
				goto local_cleanup;
			}

			SetMem(
				(VOID *) PayloadFileInformation, 
				PayloadFileInformationSize, 
				0xFF
			);

			Status = PayloadFileProtocol->GetInfo(
				PayloadFileProtocol,
				&gEfiFileInfoGuid,
				&PayloadFileInformationSize,
				(VOID *)PayloadFileInformation
			);
		}

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to stat payload image: %r\n", Status);
			goto local_cleanup;
		}

		Print(L"Payload image size: 0x%llx\n", PayloadFileInformation->FileSize);
		if (PayloadFileInformation->FileSize > UINT32_MAX)
		{
			Print(L"Payload image is too large\n");
			goto local_cleanup_free_info;
		}

		PayloadFileBufferSize = (UINTN) PayloadFileInformation->FileSize;

		/* Allocate pool for reading file */
		Status = gBS->AllocatePool(
			EfiLoaderData, 
			PayloadFileBufferSize, 
			&PayloadFileBuffer
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to allocate pool for file: %r\n", Status);
			goto local_cleanup_free_info;
		}

		SetMem(
			PayloadFileBuffer,
			PayloadFileBufferSize,
			0xFF);

		/* Read file */
		Status = PayloadFileProtocol->Read(
			PayloadFileProtocol,
			&PayloadFileBufferSize,
			PayloadFileBuffer
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to read file: %r\n", Status);
			goto local_cleanup_file_pool;
		}

		Print(L"Payload loaded into memory at 0x%x.\n", PayloadFileBuffer);

		/* Check LK file */
		PayloadElf32Ehdr = PayloadFileBuffer;
		if (!CheckElf32Header(PayloadElf32Ehdr))
		{
			Print(L"Cannot load this LK image\n");
			goto local_cleanup_file_pool;
		}

		/* Check overlapping */
		if (PayloadElf32Ehdr->e_phoff < sizeof(Elf32_Ehdr))
		{
			Print(L"ELF header has overlapping\n");
			goto local_cleanup_file_pool;
		}

		Print(L"Proceeded to Payload load\n");
		PayloadElf32Phdr = (VOID*) (((UINTN) PayloadFileBuffer) + PayloadElf32Ehdr->e_phoff);
		LkEntryPoint = PayloadElf32Ehdr->e_entry;

		Print(L"%d sections will be inspected.\n", PayloadElf32Ehdr->e_phnum);

		/* Determine LOAD section */
		for (UINTN ph_idx = 0; ph_idx < PayloadElf32Ehdr->e_phnum; ph_idx++)
		{
			PayloadElf32Phdr = (VOID*) (((UINTN)PayloadElf32Phdr) + (ph_idx * sizeof(Elf32_Phdr)));

			/* Check if it is LOAD section */
			if (PayloadElf32Phdr->p_type != PT_LOAD)
			{
				Print(L"Section %d skipped because it is not LOAD, it is 0x%x\n", ph_idx, PayloadElf32Phdr->p_type);
				continue;
			}

			/* Sanity check: PA = VA, PA = entry_point, memory size = file size */
			if (PayloadElf32Phdr->p_paddr != PayloadElf32Phdr->p_vaddr)
			{
				Print(L"LOAD section %d skipped due to identity mapping vioaltion\n", ph_idx);
				continue;
			}

			if (PayloadElf32Phdr->p_filesz != PayloadElf32Phdr->p_memsz)
			{
				Print(L"%ELOAD section %d size inconsistent; use with caution%N\n", ph_idx);
			}

			if (PayloadElf32Phdr->p_paddr != LkEntryPoint)
			{
				Print(L"LOAD section %d skipped due to entry point violation\n", ph_idx);
				continue;
			}

			PayloadSectionOffset = PayloadElf32Phdr->p_offset;
			PayloadLength = PayloadElf32Phdr->p_memsz;

			/* Exit on the first result */
			break;
		}

		if (PayloadSectionOffset == 0 || PayloadLength == 0)
		{
			Print(L"Unable to find suitable LOAD section\n");
			goto local_cleanup_file_pool;
		}

		Print(L"ELF entry point = 0x%x\n", PayloadElf32Phdr->p_paddr);
		Print(L"ELF offset = 0x%x\n", PayloadSectionOffset);
		Print(L"ELF length = 0x%x\n", PayloadLength);

		PayloadLoadSec = (VOID*) (((UINTN) PayloadFileBuffer) + PayloadSectionOffset);

		/* Allocate memory for actual bootstrapping */
		PayloadLoadPages = (PayloadLength % EFI_PAGE_SIZE != 0) ?
			(PayloadLength / EFI_PAGE_SIZE) + 1 :
			(PayloadLength / EFI_PAGE_SIZE);

		Print(L"Allocate memory at 0x%x\n", PayloadElf32Phdr->p_paddr);
		Print(L"Allocate 0x%x pages memory\n", PayloadLoadPages);

		Status = gBS->AllocatePages(
			AllocateAddress, 
			EfiLoaderCode, 
			PayloadLoadPages, 
			&LkEntryPoint
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to allocate memory for ELF payload\n");
			goto local_cleanup_file_pool;
		}

		/* Move LOAD section to actual location */
		SetMem(
			(VOID*) LkEntryPoint, 
			PayloadLength, 
			0xFF);
		
		CopyMem(
			(VOID*) LkEntryPoint, 
			PayloadLoadSec, 
			PayloadLength
		);

		Print(L"Memory copied!\n");

		/* Jump to LOAD section entry point and never returns */
		Print(L"\nJump to address 0x%x\n", LkEntryPoint);

		/* Ensure loader is not located too high */
		if (LkEntryPoint > UINT32_MAX)
		{
			Print(L"Loader located too high\n");
			Status = EFI_INVALID_PARAMETER;
			goto local_cleanup_file_pool;
		}

		/* Jump to address securely */
		JumpToAddress(
			ImageHandle, 
			(uint32_t) LkEntryPoint
		);

		local_cleanup_file_pool:
		gBS->FreePool(PayloadFileBuffer);

		local_cleanup_free_info:
		gBS->FreePool((VOID *) PayloadFileInformation);

		local_cleanup:
		Status = PayloadFileProtocol->Close(PayloadFileProtocol);
		if (EFI_ERROR(Status))
		{
			Print(L"Failed to close Payload image: %r\n", Status);
		}

		break;
	}

exit:
	// If something fails, give 5 seconds to user inspect what happened
	gBS->Stall(SECONDS_TO_MICROSECONDS(5));
	return Status;

}
