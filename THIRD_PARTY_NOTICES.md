# Third-party notices

Type-C Cable Tester v0.3.6 is built with the pinned components below. The
Apache License 2.0 in this repository applies only to project-original source,
tests, scripts, and documentation; it does not replace any third-party terms.

## Components included in the firmware image

| Component | Version used | Applicable license / notice |
|---|---:|---|
| STM32duino / Arduino Core for STM32, including the Arduino core, `Wire` and USB CDC support | `2.12.0` (`framework-arduinoststm32@4.21200.0`) | Mixed upstream terms. The Arduino core is primarily LGPL-2.1-or-later; see the exact [upstream license collection](https://github.com/stm32duino/Arduino_Core_STM32/blob/2.12.0/License.md). |
| STM32C0xx HAL Driver | `1.4.0`, bundled with Arduino Core for STM32 `2.12.0` | BSD-3-Clause; see the [upstream HAL license](https://github.com/stm32duino/Arduino_Core_STM32/blob/2.12.0/system/Drivers/STM32C0xx_HAL_Driver/LICENSE.md). |
| Arm CMSIS Core headers | CMSIS-Core(M) `6.1.0` in `framework-cmsis@2.60300.0` | Apache-2.0. |
| STM32C0 CMSIS device support | `1.3.0`, bundled with Arduino Core for STM32 `2.12.0` | Apache-2.0 and component-specific notices; see the [upstream CMSIS device license](https://github.com/stm32duino/Arduino_Core_STM32/blob/2.12.0/system/Drivers/CMSIS/Device/ST/STM32C0xx/LICENSE.md) and the upstream license collection above. |
| STM32 USB Device Library | Bundled with Arduino Core for STM32 `2.12.0` | ST SLA0044 Ultimate Liberty license, Rev 5 / February 2018. The software must be used and execute solely and exclusively on or in combination with a microcontroller or microprocessor device manufactured by or for STMicroelectronics; see the [version-pinned upstream copy](https://github.com/stm32duino/Arduino_Core_STM32/blob/2.12.0/system/Middlewares/ST/STM32_USB_Device_Library/LICENSE.md). |
| GCC runtime support, including linked `libgcc` and `libstdc++_nano` portions | GNU Arm Embedded Toolchain `12.3.1` | GPLv3 with GCC Runtime Library Exception 3.1 for covered runtime code. |
| newlib / newlib-nano C and math runtime, including `crt0` from libgloss | `4.3.0`, distributed with the pinned toolchain | Multiple permissive notices collected in `COPYING.NEWLIB` and `COPYING.LIBGLOSS` in the toolchain distribution. |

## Build-time tools

The following tools are used to build the firmware but are not, as tools,
embedded into the image:

- PlatformIO Core `6.1.19` (Apache-2.0)
- PlatformIO ST STM32 platform `19.7.1` (Apache-2.0)
- SCons `4.8.1` / `tool-scons@4.40801.0` (MIT)
- `toolchain-gccarmnoneeabi@1.120301.0` (GCC `12.3.1`, build `20230626`)

The build environment also resolves `framework-cmsis-dsp@1.16.2`
(Apache-2.0), but the v0.3.6 project does not call CMSIS-DSP and the linked map
contains no CMSIS-DSP archive or symbol.

Their own executables and packages remain under their respective upstream
licenses. Runtime portions selected by the linker are covered separately in
the table above.

## Source and redistribution

The repository and release provide the project source, the exact dependency
versions, build instructions, and relinking guidance in
[SOURCE_AND_RELINKING.md](SOURCE_AND_RELINKING.md). The v0.3.6 release asset
`TypeC_Tester_STM32C071_v0.3.6-corresponding-source.zip` contains a repository
source snapshot, the Arduino Core/CMSIS package source used by the build, and
the relevant license texts preserved from the installed packages. Exact source
locations and versions are recorded in the same document.

Before redistributing a modified framework, toolchain, or firmware binary,
review the complete upstream texts. No third-party copyright or license notice
is removed or replaced by the project license.
