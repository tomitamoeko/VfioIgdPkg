# VfioIgdPkg - EFI drivers supporting IGD passthrough

The VfioIgdPkg repository provides EFI DXE drivers supporting Intel Integrated
Graphics (IGD) passthrough with VFIO and QEMU.

According to OVMF [Bug #935](https://web.archive.org/web/20240827012422/https://bugzilla.tianocore.org/show_bug.cgi?id=935), 
upstream OVMF is not willing to accept the IGD-specific code. As a alternative
solution, a virtual PCI Option ROM is used to provide required DXE drivers for
initializing IGD device in guest and GOP display output.
For more details, refer QEMU [docs/igd-assign.txt](https://github.com/qemu/qemu/blob/master/docs/igd-assign.txt).

This reposititory consists of two DXE drivers:

* **IgdAssignmentDxe** *(Required)*: Sets up OpRegion and BDSM (Base of Data Stolen Memory) register.
* **PlatformGopPolicy** *(Optional)*: Implements the protocol required by proprietary Intel GOP driver.


## Build

```shell
# Build IgdAssignmentDxe only (NO GOP display output)
$ ./build.sh igd.rom

# Build with IntelGopDriver (with GOP display output)
$ ./build.sh --gop /path/to/IntelGopDriver.efi igd.rom
```

IntelGopDriver can be extracted from host firmware using tools like [UEFITool](https://github.com/LongSoft/UEFITool) or [UEFI BIOS Updater](https://winraid.level1techs.com/t/tool-guide-news-uefi-bios-updater-ubu/30357).
