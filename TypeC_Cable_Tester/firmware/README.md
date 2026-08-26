# Type-C 线缆测试治具固件

当前目录包含可移植 C11 固件核心，以及已经完成正式 ARM 编译和链接的 STM32C071G8U6 实板目标。可直接烧录的 v0.3.6 产物和上板步骤见 [`target/stm32c071/README.md`](target/stm32c071/README.md)。

对应的硬件原理图和 PCB 工程见[立创开源硬件平台](https://oshwhub.com/liu_xianseen/project_mzckqhhy)。

## 许可证

本目录中由项目作者原创的固件源码、测试、构建/烧录脚本和文档采用 [Apache License 2.0](../../LICENSE)。原理图和 PCB 工程另行采用 OpenAtom OHL 1.0。预编译固件包含的第三方组件继续遵循各自许可证，详见 [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) 与 [SOURCE_AND_RELINKING.md](../../SOURCE_AND_RELINKING.md)。

## 已实现

- U2/U3 共 48 端点逐点扫描；任何时刻只允许一个低电平激励。
- 每次激励前先把两颗测试扩展器全部切回输入，严格执行 break-before-make。
- PCAL6524 约 100 kΩ 内部上拉、寄存器回读、静态基线、激励低电平和释放恢复自检。
- 5 次采样和不稳定采样记录；只有 5/5 一致才提交稳定导通。普通数据脚激励稳定 50 ms，VBUS/GND/CC/VCONN 激励稳定 250 ms，用于滤除线缆/eMarker 充放电瞬态。
- 每一路释放后回读两颗 PCAL 的方向寄存器，确认全高阻；输入电平按 5 ms 复采，最长等待 100 ms，并要求连续两次恢复。
- 扫描结束及任何 I²C 错误后的双端全高阻恢复。
- USB 2.0、全功能、带/不带 e-marker、直连夹具和发现模式。
- 上电默认 `AUTO`；一次原始扫描后在 `USB2_UNMARKED`、`USB2_EMARKED`、`FULL_EMARKED` 中选择最佳匹配。实体 START 键每次都强制 AUTO，不受串口上次选择影响。
- J1/J2 各自正插/反插共 4 个方向假设自动匹配。
- VBUS/GND 多触点并联、USB 2.0 单组 D+/D-、全功能线高速对与 SBU 交叉关系。
- 开路、短路、错线、方向不对称和时间不稳定分别计数并保留端点明细；有向矩阵不再用反向结果镜像覆盖。
- 扫描开始时清空 24 路灯；J1 的 24 个源只有在释放并提交后才按 A1..B12 逐灯填满，随后 J2 的 24 个源按同顺序逐灯熄灭。仅在完成源计数增加时写 U5，不在每次 tick 重复写。
- 扫描完成后，24 路灯立即切换为稳定、双向、跨 J1/J2 确认的实际导体；线缆电气故障不会抹掉已确认导体，只有扫描硬件错误才全部清空。
- 空载 `NO_CONNECTION`、单端 `ONE_END_ONLY`、三颗总状态灯、非阻塞蜂鸣节奏和最近一次报告留存。
- 每个物理 GND/VBUS 组合独立分类为双向、GND 源→VBUS、VBUS 源→GND 或时间不稳定；双向或 GND 源→VBUS 返回 `POWER_CROSS_FAULT`，仅 VBUS 源→GND 或电源对时间不稳定返回 `POWER_CROSS_SUSPECT`。两类安全结果均优先于普通线缆判定，`DISCOVERY` 也不能绕过。
- CDC 文本命令：`START`、`PROFILE`、`STATUS`、`REPORT`、`ABORT`、`VERSION`、`HELP`。

## 目录

- `include/`、`src/`：可移植 C11 核心。
- `port/stm32cube_port_template.c`：STM32Cube HAL/CDC 接入模板，不参加便携核心编译。
- `tests/`：线缆拓扑模型、故障注入和 C 语法检查。
- `target/stm32c071/`：当前 PCB 的 PlatformIO/Arduino STM32 目标工程、烧录脚本和 v0.3.6 release 产物。
- `测试与指示状态机.md`：原始产品/硬件行为基线。

## 默认参数

| 项目 | 默认值 |
|---|---:|
| U5 LED 地址 | `0x20` |
| U2 / J1 测试端地址 | `0x22` |
| U3 / J2 测试端地址 | `0x23` |
| 每个源的采样次数 | 5 |
| 有效票数 | 3 |
| 激励稳定时间 | 50 ms |
| VBUS/GND/CC/VCONN 激励稳定时间 | 250 ms |
| 样本间隔 | 1 ms |
| 释放首次检查 | 10 ms |
| 释放复采间隔 | 5 ms |
| 释放恢复超时 | 100 ms |
| 默认线缆配置 | `AUTO` |

`tester_scan_config_t.contact_to_pcal_pin` 是唯一的 U2/U3 板级通道映射入口，默认按 `P0_0..P2_7 → A1..A12、B1..B12` 顺序。硬件最终网表冻结后必须用引脚—网络黄金表复核；若实际次序不同，从 `tester_scan_default_config()` 取得默认值、修改该表，再调用 `tester_app_init_with_scan_config()`，不改扫描和判定逻辑。

## 当前实板接入

1. MCU 为 STM32C071G8U6；HSI48 在 CDC 初始化前由 USB SOF CRS 校准。
2. I²C1 使用 PB6=SCL、PB7=SDA、400 kHz，HAL 单次操作超时 20 ms。
3. PB1 控制 U2/U3/U5 共用 `PCAL_RESET#`；PA1 是共用 `PCAL_INT#` 输入，当前扫描采用轮询。
4. PA5 在 HAL 全局初始化阶段即输出低；PA2/PA3/PA4 同期输出高，保证蜂鸣器和低有效状态灯安全。
5. PA0 是外部上拉、低电平按下；PA11/PA12 用于 USB D-/D+；PA13/PA14 保留 SWD。
6. U5/U2/U3 的 7 位地址固定为 `0x20/0x22/0x23`；U2/U3 的 24 路映射按 `P0_0..P2_7 → A1..A12、B1..B12` 固化。

## 线缆配置

```text
AUTO
DISCOVERY
USB2_UNMARKED
USB2_EMARKED
FULL_UNMARKED
FULL_EMARKED
STRAIGHT24
```

USB-C 拓扑按 Type-C-to-Type-C 线缆结构实现：全功能线使用标准高速对和 SBU 交叉关系；USB 2.0 线只要求 GND、VBUS、CC、D+、D-。四个 VBUS 触点和四个 GND 触点各自作为允许并联的电源组。

eMarker/VCONN 规则不要求 B5↔B5 低阻直通，允许但不要求 VCONN-GND Ra/隔离路径。v0.3.6 将标记型 profile 中 A5/B5 与 GND 或其他 CC 触点之间的稳定电子路径计入 `EMARKER_ELECTRONIC_PATH_PAIRS`：稳定单向路径和允许表内的稳定双向 Ra 证据不计 `UNSTABLE`，未允许的双向连接仍计确认短路，带时间抖动的路径仍计 `TEMPORAL_UNSTABLE_PAIRS/UNSTABLE`。由于本硬件没有 USB-PD Discover Identity，`USB2_EMARKED`、`FULL_UNMARKED`、`FULL_EMARKED` 导体正常时返回 `CONDUCTORS_PASS_EMARKER_UNVERIFIED`，而不是完整合规 `PASS`。

AUTO 只做观测拓扑的最佳拟合，不等价于已知 SKU 验收。全功能线的 10 路 SuperSpeed/SBU 导体若全部开路，其观测可能与合法 USB 2.0 线无法区分；测试已知全功能 SKU 时仍应显式执行 `START FULL_EMARKED`。AUTO 报告同时给出 `REQUESTED_PROFILE=AUTO`、`DETECTED_PROFILE=<匹配线型>` 和兼容字段 `PROFILE=<实际分析线型>`；空载或单端插入时 `DETECTED_PROFILE=AUTO`，不虚构线型。

报告中的 `ASYMMETRIC_PAIRS` 是两个扫描方向结果不同的无序端点对，`TEMPORAL_UNSTABLE_PAIRS` 是同一方向重复采样不一致的无序端点对，两者不再混为一个原因。`POWER_CROSS_PAIRS` 为发现任意稳定方向或时间不稳定证据的唯一 GND/VBUS 物理组合总数；细分字段为 `POWER_CROSS_BIDIR_PAIRS`、`POWER_CROSS_GND_SOURCE_TO_VBUS_PAIRS`、`POWER_CROSS_VBUS_SOURCE_TO_GND_PAIRS` 和 `POWER_CROSS_TEMPORAL_PAIRS`。时间不稳定证据可与稳定方向分类重叠，因此四个细分计数不保证相加等于总数。

## 本机验证

便携规则测试：

```powershell
python tests\check_c_syntax.py
python -m unittest discover -s tests -p test_firmware_model.py -v
```

STM32C071 全量构建：

```powershell
target\stm32c071\build.ps1 -Clean
```

当前已通过 36 项 Python 规则测试（含实测全功能有向矩阵固定样本、两阶段扫描进度和 LED 失效安全合同）、8 个 C 翻译单元语法门，以及 Arm GCC 12.3.1 的无警告全量编译/链接。仍必须完成真实板联调：扫描进度灯、三地址应答、全高阻失效安全、24 路黄金夹具、开路/短路/错线样件、四种插入方向以及 USB 断开时的按键独立测试。
