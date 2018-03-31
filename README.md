Boot Shim (ELF Variant)
=======================================

Boot Shim is a small ARM32 Windows Boot Manager Application that intended to 
chain-load any random ARM32 ELF applications on [hacked Lumias](http://wpinternals.net)
or other UEFI/ARM32 Windows devices.

It is based on [UEFI-Simple](https://github.com/pbatard/uefi-simple). 
IDE-debugging is not supported.

## Prerequisites

* [Visual Studio 2017](https://www.visualstudio.com/vs/community/) with ARM support
* Git

## Sub-Module initialization

For convenience, the project relies on the gnu-efi library, so you need to initialize the git
submodule either through git commandline with:
```
git submodule init
git submodule update
```
Or, if using a UI client (such as TortoiseGit) by selecting _Submodule Update_ in the context menu.

## Compilation and testing

Only Visual Studio is supported in this branch. Do not use `Release` mode, it won't work.

I used a well-known certificate from Windows Kits to sign the binary. You can replace with yours.

## Visual Studio 2017 and ARM support

Please be mindful that, to enable ARM compilation support in Visual Studio
2017, you __MUST__ go to the _Individual components_ screen in the setup application
and select the ARM compilers and libraries there, as they do __NOT__ appear in
the default _Workloads_ screen:

![VS2017 Individual Components](http://files.akeo.ie/pics/VS2017_Individual_Components2.png)

## ELF requirements

- There must be a LOAD section has p_paddr and p_vaddr matches program entry point address (e_entry).
- LOAD section must have p_paddr equals to p_vaddr (identity mapping requirements).
- LOAD section must reside in device's memory region. That means p_paddr must larger or equal (not likely) to 
device's memory base, and p_addr + p_memsz must not go out of device's memory region.
- LOAD section must have p_memsz equals to p_filesz.
- Only first LOAD section that meets these requirements will be loaded into memory.
- e_machine must be EM_ARM.
- e_type must be ET_EXEC.
- Has name of emmc_appsboot.mbn in a firmware-recognized partition (it will try all partitions and use the first one available)

Little Kernel (aboot) signed variants meet these requirements.