#include "EFIApp.h"

VOID Arm32JumpToAddress(EFI_HANDLE ImageHandle, uint32_t addr)
{

	EFI_STATUS Status;
	UINTN MemMapSize = 0;
	EFI_MEMORY_DESCRIPTOR* MemMap = 0;
	UINTN MapKey = 0;
	UINTN DesSize = 0;
	UINT32 DesVersion = 0;

	/* Entry */
	VOID(*entry)() = (VOID*) addr;

	gBS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DesSize, &DesVersion);

	/* Shutdown */
	Status = gBS->ExitBootServices(ImageHandle, MapKey);
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

ElfVerifierStatus CheckElf32Header(Elf32_Ehdr* bl_elf_hdr)
{

	EFI_PHYSICAL_ADDRESS ElfEntryPoint;
	EFI_STATUS Status = EFI_SUCCESS;

	if (bl_elf_hdr == NULL) return InvalidFile;

	// Sanity check: Signature
	if (bl_elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		bl_elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		bl_elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		bl_elf_hdr->e_ident[EI_MAG3] != ELFMAG3)
	{
		Print(L"Fail: Invalid ELF magic\n");
		return InvalidMagic;
	}

	// Sanity check: exec
	if (bl_elf_hdr->e_type != ET_EXEC)
	{
		Print(L"Fail: Not EXEC ELF\n");
		return InvalidType;
	}

	// Sanity check: entry point and size
	ElfEntryPoint = bl_elf_hdr->e_entry;

	Status = gBS->AllocatePages(
		AllocateAddress, 
		EfiBootServicesCode, 
		1, 
		&ElfEntryPoint);
	if (EFI_ERROR(Status))
	{
		Print(L"Fail: Invalid entry point\n");
		return InvalidEntry;
	}

	// Free page allocated
	gBS->FreePages(
		ElfEntryPoint, 
		1);

	// Sanity check: program header entries. At least one should present.
	if (bl_elf_hdr->e_phnum < 1)
	{
		Print(L"Fail: Less than one program header entry found\n");
		return NoLoadSection;
	}

	// Sanity check: Architecture
	if (bl_elf_hdr->e_machine == EM_AARCH64)
	{
		// Need to check again in AArch64 mode.
		return ValidateAArch64;
	}
	else if (bl_elf_hdr->e_machine != EM_ARM)
	{
		Print(L"Fail: Not ARM architecture ELF32 file\n");
		return InvalidArchitecture;
	}

	return ClearToLoad;
}

ElfVerifierStatus CheckElf64Header(Elf64_Ehdr* bl_elf_hdr)
{

	EFI_PHYSICAL_ADDRESS ElfEntryPoint;
	EFI_STATUS Status = EFI_SUCCESS;

	if (bl_elf_hdr == NULL) return InvalidFile;

	// Sanity check: Signature
	if (bl_elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		bl_elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		bl_elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		bl_elf_hdr->e_ident[EI_MAG3] != ELFMAG3)
	{
		Print(L"Fail: Invalid ELF magic\n");
		return InvalidMagic;
	}

	// Sanity check: exec
	if (bl_elf_hdr->e_type != ET_EXEC)
	{
		Print(L"Fail: Not EXEC ELF\n");
		return InvalidType;
	}

	// Sanity check: entry point and size
	ElfEntryPoint = bl_elf_hdr->e_entry;

	Status = gBS->AllocatePages(
		AllocateAddress,
		EfiBootServicesCode,
		1,
		&ElfEntryPoint);

	if (EFI_ERROR(Status))
	{
		Print(L"Fail: Invalid entry point\n");
		return InvalidEntry;
	}

	// Free page allocated
	gBS->FreePages(
		ElfEntryPoint,
		1);

	// Sanity check: program header entries. At least one should present.
	if (bl_elf_hdr->e_phnum < 1)
	{
		Print(L"Fail: Less than one program header entry found\n");
		return NoLoadSection;
	}

	// Sanity check: Architecture
	if (bl_elf_hdr->e_machine != EM_AARCH64)
	{
		Print(L"Fail: Not AArch64 architecture ELF64 file\n");
		return InvalidArchitecture;
	}

	return ClearToLoad;
}

EFI_STATUS Elf32Load(EFI_HANDLE ImageHandle, VOID* PayloadFileBuffer)
{
	EFI_STATUS Status = EFI_SUCCESS;
	Elf32_Ehdr* PayloadElf32Ehdr = NULL;
	Elf32_Phdr* PayloadElf32Phdr = NULL;
	UINTN PayloadSectionOffset = 0;
	UINTN PayloadLength = 0;
	UINTN PayloadLoadPages = 0;
	EFI_PHYSICAL_ADDRESS Elf32EntryPoint = PAYLOAD_ENTRY_POINT_ADDR_INVALID;
	VOID* PayloadLoadSec;

	if (PayloadFileBuffer == NULL)
	{
		Status = EFI_INVALID_PARAMETER;
		goto exit;
	}

	PayloadElf32Ehdr = PayloadFileBuffer;

	/* Check overlapping */
	if (PayloadElf32Ehdr->e_phoff < sizeof(Elf32_Ehdr))
	{
		Print(L"ELF32 header has overlapping\n");
		Status = EFI_ABORTED;
		goto exit;
	}

	Print(L"Proceeded to ELF32 Payload load\n");
	PayloadElf32Phdr = (VOID*)(((UINTN)PayloadFileBuffer) + PayloadElf32Ehdr->e_phoff);
	Elf32EntryPoint = PayloadElf32Ehdr->e_entry;

	Print(L"%d ELF32 sections will be inspected.\n", PayloadElf32Ehdr->e_phnum);

	/* Determine LOAD section */
	for (UINTN ph_idx = 0; ph_idx < PayloadElf32Ehdr->e_phnum; ph_idx++)
	{
		PayloadElf32Phdr = (VOID*)(((UINTN)PayloadElf32Phdr) + (ph_idx * sizeof(Elf32_Phdr)));

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
			Print(L"LOAD section %d skipped due to inconsistent size\n", ph_idx);
			continue;
		}

		if (PayloadElf32Phdr->p_paddr != Elf32EntryPoint)
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
		Status = EFI_NOT_FOUND;
		goto exit;
	}

	Print(L"ELF32 entry point = 0x%x\n", PayloadElf32Phdr->p_paddr);
	Print(L"ELF32 offset = 0x%x\n", PayloadSectionOffset);
	Print(L"ELF32 length = 0x%x\n", PayloadLength);

	PayloadLoadSec = (VOID*)(((UINTN)PayloadFileBuffer) + PayloadSectionOffset);

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
		&Elf32EntryPoint);

	if (EFI_ERROR(Status))
	{
		Print(L"Failed to allocate memory for ELF32 payload\n");
		goto exit;
	}

	/* Move LOAD section to actual location */
	CopyMem((VOID*)(PayloadElf32Phdr->p_paddr), PayloadLoadSec, PayloadLength);
	Print(L"Memory copied!\n");

	/* Jump to LOAD section entry point and never returns */
	Print(L"\nJump to address 0x%x in 32bit mode\n", PayloadElf32Phdr->p_paddr);

#if _DEBUG_INSPECT_
	gBS->Stall(SECONDS_TO_MICROSECONDS(5));
#endif
	Arm32JumpToAddress(ImageHandle, PayloadElf32Phdr->p_paddr);

exit:
	/* We should not reach here */
	return Status;
}

EFI_STATUS Elf64Load(EFI_HANDLE ImageHandle, VOID* PayloadFileBuffer)
{
	return EFI_UNSUPPORTED;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{

	EFI_STATUS Status = EFI_SUCCESS;
	
	// File IO
	UINTN NumHandles = 0;
	EFI_HANDLE *SfsHandles;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SfsProtocol;
	EFI_FILE_PROTOCOL *FileProtocol;
	EFI_FILE_PROTOCOL *PayloadFileProtocol;
	EFI_FILE_INFO *PayloadFileInformation = NULL;
	UINTN PayloadFileInformationSize = 0;
	CHAR16 *PayloadFileName = PAYLOAD_BINARY_NAME;
	UINTN PayloadFileBufferSize;
	VOID* PayloadFileBuffer;

	// Verifier
	ElfVerifierStatus VerifierStatus = OtherError;

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

		// Read image and parse ELF file
		Print(L"Opened payload image\n");

		Status = PayloadFileProtocol->GetInfo(
			PayloadFileProtocol,
			&gEfiFileInfoGuid,
			&PayloadFileInformationSize,
			(VOID *) PayloadFileInformation
		);

		if (Status == EFI_BUFFER_TOO_SMALL)
		{
			Status = gBS->AllocatePool(EfiBootServicesData, PayloadFileInformationSize, &PayloadFileInformation);
			if (EFI_ERROR(Status))
			{
				Print(L"Failed to allocate pool for file info: %r\n", Status);
				goto local_cleanup;
			}

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
			EfiBootServicesData, 
			PayloadFileBufferSize, 
			&PayloadFileBuffer);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to allocate pool for file: %r\n", Status);
			goto local_cleanup_free_info;
		}

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

		/* Check ELF file */
		VerifierStatus = CheckElf32Header((Elf32_Ehdr*) PayloadFileBuffer);

		switch (VerifierStatus)
		{
		case ClearToLoad:
			Status = Elf32Load(ImageHandle, PayloadFileBuffer);
			break;
		case ValidateAArch64:
			VerifierStatus = CheckElf64Header((Elf64_Ehdr*) PayloadFileBuffer);
			Status = (VerifierStatus == ClearToLoad) ? 
				Elf64Load(ImageHandle, PayloadFileBuffer) : 
				EFI_UNSUPPORTED;
			break;
		default:
			Status = EFI_UNSUPPORTED;
			break;
		}

		if (EFI_ERROR(Status))
		{
			Print(L"Cannot load this payload: %r\n", Status);
			goto local_cleanup_file_pool;
		}

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
