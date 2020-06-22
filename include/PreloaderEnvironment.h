#pragma once

#ifndef SIGNATURE_16
#define SIGNATURE_16(A, B)        ((A) | (B << 8))
#endif

#ifndef SIGNATURE_32
#define SIGNATURE_32(A, B, C, D)  (SIGNATURE_16 (A, B) | (SIGNATURE_16 (C, D) << 16))
#endif

#ifndef __PRELOADER_ENV_H__
#define __PRELOADER_ENV_H__

#define PRELOADER_ENV_ADDR 0xb0000000
#define PRELOADER_HEADER SIGNATURE_32('B', 'S', 'E', '2')

typedef enum _PRELOADER_ENVIRONMENT_BOOT_MODE {
	BOOT_MODE_PSCI = 0,
	BOOT_MODE_MPPARK,
	BOOT_MODE_MPPARK_EL2,
	BOOT_MODE_MAX
} PRELOADER_ENVIRONMENT_BOOT_MODE;

typedef struct _PRELOADER_ENVIRONMENT {
	UINT32 Header;
	UINT32 PreloaderVersion;
	CHAR8 PreloaderRelease[64];
	EFI_TIME BootTimeEpoch;
	UINT32 UefiDisplayInfo[30];
	UINT32 BootMode;
	UINT32 EnablePlatformSdCardBoot;
	UINT32 UseQuadCoreConfiguration;
	UINT32 Crc32;
} PRELOADER_ENVIRONMENT, *PPRELOADER_ENVIRONMENT;

#endif

#define PRELOADER_VERSION 0x1001
#define PRELOADER_RELEASE "PreLoader Unknown Release"
