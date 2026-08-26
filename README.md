# Hardware

这里收录可复现、可烧录的硬件相关开源项目。

## Type-C Cable Tester

**Type-C 线缆开短路测试工具**是一款面向研发调试、线缆维修、来料检验和产线筛查的 USB Type-C 检测平台。项目基于 STM32C071 与 PCAL6524，通过对线缆两端 48 个触点进行逐点、双向、高阻隔离扫描，自动识别普通 USB 2.0、带电子路径特征的 USB 2.0 及全功能 USB 3.x 线缆拓扑，并检测开路、短路、错线、接触不稳定和 VBUS/GND 电源交叉等异常。

板载 24 路信号灯可实时显示扫描进度与最终导通结果，同时提供总状态灯、蜂鸣器和 USB CDC 详细报告。扫描过程中任意时刻最多只有一个测试端点被拉低，其余 47 点保持高阻；每一路完成后还会验证两端已经恢复输入状态。

硬件原理图和 PCB 工程已发布至[立创开源硬件平台](https://oshwhub.com/liu_xianseen/project_mzckqhhy)，本仓库维护与该测试板配套的固件源码、构建脚本和可烧录版本。

### 主要功能

- 48 端点逐点、双向有向矩阵扫描。
- 自动识别 `USB2_UNMARKED`、`USB2_EMARKED` 和 `FULL_EMARKED` 拓扑。
- 自动匹配 Type-C 两端正反插方向。
- 独立报告开路、确认短路、错线、方向不对称、时间不稳定及电源交叉。
- J1 阶段 24 灯逐个填满、J2 阶段逐个熄灭，完成后切换为真实导体灯。
- USB CDC 支持 `START`、`STATUS`、`REPORT`、`PROFILE`、`VERSION` 等命令。
- 提供正常工厂版和关闭蜂鸣器的办公室静音版。

### 项目目录

- [立创开源硬件工程（原理图与 PCB）](https://oshwhub.com/liu_xianseen/project_mzckqhhy)
- [固件源码与完整说明](TypeC_Cable_Tester/firmware/README.md)
- [STM32C071 构建和烧录说明](TypeC_Cable_Tester/firmware/target/stm32c071/README.md)
- [v0.3.6 可烧录文件](TypeC_Cable_Tester/firmware/target/stm32c071/release/)
- [GitHub Release v0.3.6](https://github.com/Liuxianseen/Hardware/releases/tag/v0.3.6)

### 快速烧录

目标 MCU：`STM32C071G8U6`。推荐使用 ST-LINK 并连接 `VTref / SWDIO / SWCLK / NRST / GND`。

```powershell
cd .\TypeC_Cable_Tester\firmware\target\stm32c071

# 工厂有声版
.\flash-stlink.ps1

# 办公室静音版
.\flash-stlink.ps1 -OfficeSilent
```

烧录脚本使用 STM32CubeProgrammer，以 SWD under-reset 模式下载、校验并复位 MCU。详细接线、安全顺序和串口用法请阅读目标工程说明。

### 构建与测试

需要 Python 3、`pycparser`、PlatformIO Core 和 STM32CubeProgrammer。

```powershell
cd .\TypeC_Cable_Tester\firmware
python -m pip install pycparser
python tests\check_c_syntax.py
python -m unittest discover -s tests -p test_firmware_model.py -v

cd .\target\stm32c071
.\build.ps1 -Clean
```

当前 v0.3.6 已通过 36 项规则测试、8 个 C 翻译单元语法检查、Arm GCC 12.3.1 全量构建，以及 STM32C071 实板烧录和 AUTO 全功能线扫描验证。

### 检测边界

本工具主要验证直流通断和线序关系。`FULL_EMARKED` 表示检测到了全功能高速通道拓扑，不代表已经验证 USB 3.0/3.1/3.2 的实际速率；本工具也不能替代阻抗、眼图、串扰、插损或 USB-PD/eMarker Discover Identity 测试设备。

### 许可证

- 本仓库中由项目作者原创的固件源码、测试、构建/烧录脚本和文档采用 [Apache License 2.0](LICENSE)。
- 原理图和 PCB 工程在[立创开源硬件平台](https://oshwhub.com/liu_xianseen/project_mzckqhhy)按 OpenAtom OHL 1.0 发布。
- 预编译的 ELF/BIN/HEX 同时包含 Arduino Core、STM32 HAL/CMSIS、ST USB Device 和工具链运行库等第三方组件；这些部分仍遵循各自许可证，不会因本项目采用 Apache-2.0 而被重新许可。详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 与 [SOURCE_AND_RELINKING.md](SOURCE_AND_RELINKING.md)。
