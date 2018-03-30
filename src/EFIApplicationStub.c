#include <efi.h>
#include <efilib.h>

#include "lk.h"
#include "elf.h"
#include "ProcessorSupport.h"

BOOLEAN CheckElf32Header(Elf32_Ehdr* header);
VOID JumpToAddress(EFI_HANDLE ImageHandle, uint32_t addr);

VOID JumpToAddress(EFI_HANDLE ImageHandle, uint32_t addr)
{

	EFI_STATUS Status;
	UINTN MemMapSize = 0;
	EFI_MEMORY_DESCRIPTOR* MemMap = 0;
	UINTN MapKey = 0;
	UINTN DesSize = 0;
	UINT32 DesVersion = 0;

	/* Entry */
	if (addr != 0x0f900000) return;
	VOID(*entry)() = (VOID*) 0x0f900000;

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
	EFI_PHYSICAL_ADDRESS LkEntryPoint = LK_ENTRY_POINT_ADDR_INVALID;

	UINTN NumHandles = 0;
	EFI_HANDLE *SfsHandles;

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SfsProtocol;

	EFI_FILE_PROTOCOL *FileProtocol;
	EFI_FILE_PROTOCOL *LkFileProtocol;
	CHAR16 *LkFileName = LK_BINARY_NAME;

	UINTN LkFileBufferSize;
	UINTN LkLoadPages;
	VOID* LkFileBuffer;
	VOID* LkLoadSec;

	EFI_FILE_INFO *LkFileInformation = NULL;
	UINTN LkFileInformationSize = 0;

	Elf32_Ehdr* lk_elf32_ehdr = NULL;
	Elf32_Phdr* lk_elf32_phdr = NULL;

	UINTN load_section_offset = 0;
	UINTN load_section_length = 0;

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

		/* Check overlapping */
		if (lk_elf32_ehdr->e_phoff < sizeof(Elf32_Ehdr))
		{
			Print(L"ELF header has overlapping\n");
			goto local_cleanup_file_pool;
		}

		Print(L"Proceeded to LK image load\n");
		LkEntryPoint = lk_elf32_ehdr->e_entry;
		lk_elf32_phdr = (VOID*) (((UINTN) LkFileBuffer) + lk_elf32_ehdr->e_phoff);

		Print(L"%d sections will be inspected.\n", lk_elf32_ehdr->e_phnum);

		/* Determine LOAD section */
		for (UINTN ph_idx = 0; ph_idx < lk_elf32_ehdr->e_phnum; ph_idx++)
		{
			lk_elf32_phdr = (VOID*) (((UINTN)lk_elf32_phdr) + (ph_idx * sizeof(Elf32_Phdr)));

			/* Check if it is LOAD section */
			if (lk_elf32_phdr->p_type != PT_LOAD)
			{
				Print(L"Section %d skipped because it is not LOAD, it is 0x%x\n", ph_idx, lk_elf32_phdr->p_type);
				continue;
			}

			/* Sanity check: PA = VA, PA = entry_point, memory size = file size */
			if (lk_elf32_phdr->p_paddr != lk_elf32_phdr->p_vaddr)
			{
				Print(L"LOAD section %d skipped due to identity mapping vioaltion\n", ph_idx);
				continue;
			}

			if (lk_elf32_phdr->p_filesz != lk_elf32_phdr->p_memsz)
			{
				Print(L"LOAD section %d skipped due to inconsistent size\n", ph_idx);
				continue;
			}

			if (lk_elf32_phdr->p_paddr != LkEntryPoint)
			{
				Print(L"LOAD section %d skipped due to entry point violation\n", ph_idx);
				continue;
			}

			load_section_offset = lk_elf32_phdr->p_offset;
			load_section_length = lk_elf32_phdr->p_memsz;

			/* Exit on the first result */
			break;
		}

		if (load_section_offset == 0 || load_section_length == 0)
		{
			Print(L"Unable to find suitable LOAD section\n");
			goto local_cleanup_file_pool;
		}


		Print(L"ELF entry point = 0x%x\n", lk_elf32_phdr->p_paddr);
		Print(L"ELF offset = 0x%x\n", load_section_offset);
		Print(L"ELF length = 0x%x\n", load_section_length);

		LkLoadSec = (VOID*) (((UINTN) LkFileBuffer) + load_section_offset);

		/* Allocate memory for actual bootstrapping */
		LkLoadPages = (load_section_length % EFI_PAGE_SIZE != 0) ?
			(load_section_length / EFI_PAGE_SIZE) + 1 :
			(load_section_length / EFI_PAGE_SIZE);

		Print(L"Allocate memory at 0x%x\n", lk_elf32_phdr->p_paddr);
		Print(L"Allocate 0x%x pages memory\n", LkLoadPages);

		/* Move LOAD section to actual location */
		CopyMem((VOID*) (lk_elf32_phdr->p_paddr), LkLoadSec, load_section_length);
		Print(L"Memory copied!\n");

		/* Jump to LOAD section entry point and never returns */
		Print(L"\nJump to address 0x%x\n", lk_elf32_phdr->p_paddr);

		gBS->Stall(5000000);
		JumpToAddress(ImageHandle, lk_elf32_phdr->p_paddr);

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
	return Status;

}
