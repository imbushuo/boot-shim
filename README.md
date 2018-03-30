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

Only Visual Studio is supported in this branch.

## Visual Studio 2017 and ARM support

Please be mindful that, to enable ARM compilation support in Visual Studio
2017, you __MUST__ go to the _Individual components_ screen in the setup application
and select the ARM compilers and libraries there, as they do __NOT__ appear in
the default _Workloads_ screen:

![VS2017 Individual Components](http://files.akeo.ie/pics/VS2017_Individual_Components2.png)

## ELF requirements

Only single `LOAD` section in ELF64 files will be recognized, it must has physical address 
and virtual address matched (EFI identity mapping), memory size and file size must match. 
Additionaly, entry point and physical address must match.

Little Kernel (aboot) signed variants meet these requirements.