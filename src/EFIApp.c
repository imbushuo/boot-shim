#include "EFIApp.h"
#include "scm.h"
#include "PCIe.h"
#include "PreloaderEnvironment.h"

VOID JumpToAddressAArch64(
	EFI_HANDLE ImageHandle, 
	EFI_PHYSICAL_ADDRESS Address,
	VOID* PayloadBuffer,
	UINT64 PayloadLength
)
{

	EFI_STATUS Status;
	UINTN MemMapSize = 0;
	EFI_MEMORY_DESCRIPTOR* MemMap = 0;
	UINTN MapKey = 0;
	UINTN DesSize = 0;
	UINT32 DesVersion = 0;
	UINT32 PayloadAddress32 = (UINT32) Address;
	UINT32 PayloadLength32 = (UINT32) PayloadLength;

	EFI_PHYSICAL_ADDRESS DynamicEl1ParamAddress = 0xA0000000;
	el1_system_param* DynamicEl1Param;

	Status = gBS->AllocatePages(
		AllocateAddress,
		EfiRuntimeServicesData,
		1,
		&DynamicEl1ParamAddress
	);

	SetMem(
		(VOID*) DynamicEl1ParamAddress,
		EFI_PAGE_SIZE,
		0xFF
	);

	if (EFI_ERROR(Status))
	{
		Print(L"EL1 Param allocation failed! \n");
		while (TRUE) { }
	}

	DynamicEl1Param = (VOID*) DynamicEl1ParamAddress;
	DynamicEl1Param->el1_x0 = 0;
	DynamicEl1Param->el1_elr = Address;

	Print(L"EL1 Param set at 0x%llx \n", DynamicEl1ParamAddress);

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

	/* Move LOAD section to actual location */
	SetMem(
		(VOID*)PayloadAddress32,
		PayloadLength32,
		0xFF);

	CopyMem(
		(VOID*)PayloadAddress32,
		PayloadBuffer,
		PayloadLength32
	);

	/* De-initialize */
	ArmDeInitialize();

	/* Disable GIC */
	writel(0, GIC_DIST_CTRL);

	/* SMC */
	ArmCallSmcHardCoded();

	// You should not reach here
	while (TRUE) { }
}

VOID JumpToAddressAArch32(
	EFI_HANDLE ImageHandle,
	EFI_PHYSICAL_ADDRESS AArch32Address,
	EFI_PHYSICAL_ADDRESS AArch64Address,
	VOID* AArch64PayloadBuffer,
	UINT64 AArch64PayloadLength
)
{

	EFI_STATUS Status;
	UINTN MemMapSize = 0;
	EFI_MEMORY_DESCRIPTOR* MemMap = 0;
	UINTN MapKey = 0;
	UINTN DesSize = 0;
	UINT32 DesVersion = 0;
	UINT32 PayloadAddress32 = (UINT32) AArch64Address;
	UINT32 PayloadLength32 = (UINT32) AArch64PayloadLength;

	/* Entry */
	VOID(*entry)() = (VOID*) AArch32Address;

	Print(L"Exiting boot services... \n");

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

	/* Move LOAD section to actual location */
	SetMem(
		(VOID*)PayloadAddress32,
		PayloadLength32,
		0xFF);

	CopyMem(
		(VOID*)PayloadAddress32,
		AArch64PayloadBuffer,
		PayloadLength32
	);

	/* De-initialize */
	ArmDeInitialize();

	/* Disable GIC */
	writel(0, GIC_DIST_CTRL);

	/* Lets go */
	entry();

}

BOOLEAN CheckElf64Header(Elf64_Ehdr * bl_elf_hdr)
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
	if (bl_elf_hdr->e_machine != EM_AARCH64)
	{
		Print(L"Fail: Not AArch64 architecture ELF64 file\n");
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
		Print(L"%EFail: Invalid entry point. Boot may fail!%N\n");
	}
	else
	{
		// Free page allocated
		gBS->FreePages(
			ElfEntryPoint,
			1
		);
	}

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

	EFI_PHYSICAL_ADDRESS UefiEntryPoint = PAYLOAD_ENTRY_POINT_ADDR_INVALID;
	EFI_PHYSICAL_ADDRESS LkEntryPoint = PAYLOAD_ENTRY_POINT_ADDR_INVALID;
	UINTN PayloadFileBufferSize;
	VOID* PayloadFileBuffer;
	VOID* PayloadLoadSec;
	EFI_FILE_INFO *PayloadFileInformation = NULL;
	UINTN PayloadFileInformationSize = 0;

	Elf64_Ehdr* PayloadElf64Ehdr = NULL;
	Elf64_Phdr* PayloadElf64Phdr = NULL;
	UINT64 PayloadSectionOffset = 0;
	UINT64 PayloadLength = 0;

	QCOM_PCIE_PROTOCOL *PCIExpressProtocol;

	PRELOADER_ENVIRONMENT PreloaderEnv;
	UINT32 PreloaderEnvCrc32;
	UINTN VarSize;
	VOID* PreloaderEnvFinalDest;
	EFI_PHYSICAL_ADDRESS PreloaderEnvAddr = PRELOADER_ENV_ADDR;

#if defined(_GNU_EFI)
	InitializeLib(
		ImageHandle, 
		SystemTable
	);
#endif

	// Load UEFI
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
		break;
	}

	// Check UEFI file
	if (PayloadFileBuffer == NULL) goto exit;
	PayloadElf64Ehdr = PayloadFileBuffer;
	if (!CheckElf64Header(PayloadElf64Ehdr))
	{
		Print(L"Cannot load this LK image\n");
		goto local_cleanup_file_pool;
	}

	// Check overlapping
	if (PayloadElf64Ehdr->e_phoff < sizeof(Elf32_Ehdr))
	{
		Print(L"ELF header has overlapping\n");
		goto local_cleanup_file_pool;
	}

	Print(L"Proceeded to Payload load\n");
	PayloadElf64Phdr = (VOID*)(((UINTN)PayloadFileBuffer) + PayloadElf64Ehdr->e_phoff);
	UefiEntryPoint = PayloadElf64Ehdr->e_entry;

	Print(L"%d sections will be inspected.\n", PayloadElf64Ehdr->e_phnum);

	// Determine LOAD section
	for (UINTN ph_idx = 0; ph_idx < PayloadElf64Ehdr->e_phnum; ph_idx++)
	{
		PayloadElf64Phdr = (VOID*)(((UINTN)PayloadElf64Phdr) + (ph_idx * sizeof(Elf64_Phdr)));

		// Check if it is LOAD section
		if (PayloadElf64Phdr->p_type != PT_LOAD)
		{
			Print(L"Section %d skipped because it is not LOAD, it is 0x%x\n", ph_idx, PayloadElf64Phdr->p_type);
			continue;
		}

		// Sanity check: PA = VA, PA = entry_point, memory size = file size
		if (PayloadElf64Phdr->p_paddr != PayloadElf64Phdr->p_vaddr)
		{
			Print(L"LOAD section %d skipped due to identity mapping vioaltion\n", ph_idx);
			continue;
		}

		if (PayloadElf64Phdr->p_filesz != PayloadElf64Phdr->p_memsz)
		{
			Print(L"%ELOAD section %d size inconsistent; use with caution%N\n", ph_idx);
		}

		if (PayloadElf64Phdr->p_paddr != UefiEntryPoint)
		{
			Print(L"LOAD section %d skipped due to entry point violation\n", ph_idx);
			continue;
		}

		PayloadSectionOffset = PayloadElf64Phdr->p_offset;
		PayloadLength = PayloadElf64Phdr->p_memsz;

		// Exit on the first result
		break;
	}

	if (PayloadSectionOffset == 0 || PayloadLength == 0)
	{
		Print(L"Unable to find suitable LOAD section\n");
		goto local_cleanup_file_pool;
	}

	Print(L"ELF entry point = 0x%llx\n", PayloadElf64Phdr->p_paddr);
	Print(L"ELF offset = 0x%llx\n", PayloadSectionOffset);
	Print(L"ELF length = 0x%llx\n", PayloadLength);

	PayloadLoadSec = (VOID*)(((UINTN)PayloadFileBuffer) + PayloadSectionOffset);

	// Ensure loader is not located too high
	if (UefiEntryPoint > UINT32_MAX)
	{
		Print(L"Loader located too high\n");
		Status = EFI_INVALID_PARAMETER;
		goto local_cleanup_file_pool;
	}

	// Make sure PCIe is initialized
	Status = gBS->LocateProtocol(
		&gQcomPcieInitProtocolGuid,
		NULL,
		(VOID**)&PCIExpressProtocol
	);

	ASSERT(Status == EFI_SUCCESS);
	Status = PCIExpressProtocol->PCIeInitHardware(PCIExpressProtocol);
	ASSERT(Status == EFI_SUCCESS);

	Print(L"PCI Express initialized \n");

	// Configure environment HOB
	PreloaderEnv.Header = PRELOADER_HEADER;
	PreloaderEnv.PreloaderVersion = PRELOADER_VERSION;
	CopyMem(PreloaderEnv.PreloaderRelease, PRELOADER_RELEASE, sizeof(PRELOADER_RELEASE));
	Status = gRT->GetTime(&PreloaderEnv.BootTimeEpoch, NULL);
	if (EFI_ERROR(Status))
	{
		Print(L"Time retrieval failed \n");
		goto exit;
	}

	VarSize = sizeof(PreloaderEnv.UefiDisplayInfo);
	Status = gRT->GetVariable(
		L"UEFIDisplayInfo",
		&gEfiGraphicsOutputProtocolGuid,
		NULL,
		&VarSize,
		&PreloaderEnv.UefiDisplayInfo
	);
	if (EFI_ERROR(Status))
	{
		Print(L"Variable retrieval failed \n");
		goto exit;
	}

	PreloaderEnv.Crc32 = 0x0;
	Status = gBS->CalculateCrc32(
		&PreloaderEnv, 
		sizeof(PreloaderEnv), 
		&PreloaderEnvCrc32
	);
	if (EFI_ERROR(Status))
	{
		Print(L"CRC32 calc failed \n");
		goto exit;
	}
	PreloaderEnv.Crc32 = PreloaderEnvCrc32;

	// Relocate HOB
	Status = gBS->AllocatePages(
		AllocateAddress,
		EfiLoaderData,
		1,
		&PreloaderEnvAddr
	);

	// Copy HOB for PEI pickup
	if (EFI_ERROR(Status))
	{
		Print(L"Page allocation failed \n");
		goto exit;
	}

	PreloaderEnvFinalDest = (VOID*)PreloaderEnvAddr;
	CopyMem(PreloaderEnvFinalDest, &PreloaderEnv, sizeof(PreloaderEnv));

	// Jump to LOAD section entry point and never returns
	Print(L"\nJump to address 0x%llx. See you in AArch64...\n", LkEntryPoint);
	JumpToAddressAArch64(
		ImageHandle,
		UefiEntryPoint,
		PayloadLoadSec,
		PayloadLength
	);

local_cleanup_file_pool:
	gBS->FreePool(PayloadFileBuffer);

local_cleanup_free_info:
	gBS->FreePool((VOID *)PayloadFileInformation);

local_cleanup:
	Status = PayloadFileProtocol->Close(PayloadFileProtocol);
	if (EFI_ERROR(Status))
	{
		Print(L"Failed to close Payload image: %r\n", Status);
	}

exit:
	// If something fails, give 5 seconds to user inspect what happened
	gBS->Stall(SECONDS_TO_MICROSECONDS(5));
	return Status;

}
