Boot Shim (UEFI Variant)
=======================================

Boot Shim is a small ARM32 Windows Boot Manager Application that intended to 
chain-load ARM64 UEFI [hacked Lumias](http://wpinternals.net), and particularly Lumia 950/950 XL.

It is based on [UEFI-Simple](https://github.com/pbatard/uefi-simple). 
IDE-debugging is not supported.

## For final-users

You can download latest BootShim for UEFI chainload by clicking the Azure Pipelines button below.

[![Build Status](https://dev.azure.com/LumiaWoA/Boot%20Shim/_apis/build/status/UEFI%20PreLoader%20Build?branchName=msm8994-pcie)](https://dev.azure.com/LumiaWoA/Boot%20Shim/_build/latest?definitionId=3&branchName=msm8994-pcie)

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
