# Source and relinking information

This file records how to obtain and rebuild the source corresponding to the
published Type-C Cable Tester v0.3.6 firmware images.

## Project source

- Firmware source for the released images:
  [TypeC_Cable_Tester/firmware at tag v0.3.6](https://github.com/Liuxianseen/Hardware/tree/v0.3.6/TypeC_Cable_Tester/firmware)
- Build and flashing instructions:
  [STM32C071 target README](TypeC_Cable_Tester/firmware/target/stm32c071/README.md)
- Pinned build definition:
  [platformio.ini](TypeC_Cable_Tester/firmware/target/stm32c071/platformio.ini)

The normal and office-silent images were built from the same source. The
office-silent variant adds `TYPEC_TESTER_OFFICE_SILENT=1` through the
`office_silent` PlatformIO environment.

## Third-party source

The build pins these primary dependencies:

- [Arduino Core for STM32 2.12.0](https://github.com/stm32duino/Arduino_Core_STM32/tree/2.12.0), distributed by PlatformIO as `framework-arduinoststm32@4.21200.0`
- Arm CMSIS Core `framework-cmsis@2.60300.0`
- PlatformIO ST STM32 platform `19.7.1`
- `toolchain-gccarmnoneeabi@1.120301.0` (GCC `12.3.1`, build `20230626`), including newlib/newlib-nano `4.3.0`

PlatformIO downloads those pinned packages when the documented build command
is run. The build environment also resolves `framework-cmsis-dsp@1.16.2`, but
the v0.3.6 map contains no linked CMSIS-DSP archive or symbol. This repository
contains no patch against the framework packages; board adaptation and the USB
pin override are implemented in the project source.

## Rebuilding or relinking

From `TypeC_Cable_Tester/firmware/target/stm32c071`, run:

```powershell
.\build.ps1 -Clean
.\build.ps1 -Clean -OfficeSilent
```

To test a modified LGPL-covered Arduino Core, use the same framework version as
a starting point, apply the desired changes to a local framework package, point
`platform_packages` in `platformio.ini` to that package, and rebuild. Because
the project source and build definition are provided, recipients can rebuild
and relink the firmware with a modified compatible framework.

Exact image sizes, compiler versions, and hashes are stored with the release
artifacts in `BUILD_INFO*.txt` and `SHA256SUMS*.txt`.

The release asset
`TypeC_Tester_STM32C071_v0.3.6-corresponding-source.zip` contains a snapshot of
this repository, the installed Arduino Core for STM32, CMSIS Core, and
CMSIS-DSP package trees, plus the relevant license texts copied verbatim from
the installed build packages. The compiler toolchain binaries are not
duplicated; their exact package identifier, runtime license texts, and rebuild
version are included, and PlatformIO can download the pinned toolchain package.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the license boundary
and component-specific terms.
