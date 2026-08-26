# STM32C071G8U6 可烧录固件 v0.3.6

本目录是当前 Type-C 线缆开短路测试工具实板对应的可烧录目标工程。目标 MCU 为 `STM32C071G8U6`，固件已按当前 PCB 网表接入三颗 PCAL6524、START 键、三色总状态灯、蜂鸣器和 USB CDC。

## 直接烧录

优先烧录 `release/TypeC_Tester_STM32C071_v0.3.6.elf` 或 `.hex`；两者自带 Flash 地址。只有烧录 `.bin` 时才需要手动指定基址 `0x08000000`。

办公室调试可使用 `release/TypeC_Tester_STM32C071_v0.3.6-office-silent.elf`。该构建将 PA5 蜂鸣器控制强制保持低电平，但保留 LED、测试状态机、USB CDC 命令和报告；`VERSION` 返回 `FW 0.3.6-OFFICE-SILENT`。正常 `v0.3.6` 工厂版的蜂鸣器行为不受影响。

静音版构建命令：

```powershell
.\build.ps1 -OfficeSilent
```

J4 接线如下：

| J4 | 信号 | ST-LINK |
|---:|---|---|
| 1 | 3V3 | VTref / 3.3V reference |
| 2 | SWDIO | SWDIO |
| 3 | SWCLK | SWCLK |
| 4 | NRST | NRST |
| 5 | GND | GND |

在 PowerShell 中执行：

```powershell
cd .\TypeC_Cable_Tester\firmware\target\stm32c071
# 工厂有声版
.\flash-stlink.ps1
# 办公室静音版
.\flash-stlink.ps1 -OfficeSilent
# 多个 ST-LINK 同时连接时指定序列号
.\flash-stlink.ps1 -StLinkSerial '<your-stlink-serial>'
```

脚本会使用 STM32CubeProgrammer 以 SWD under-reset 模式烧录 ELF，随后校验并复位 MCU。首次烧录时不要在 J1/J2 接线缆或任何带电设备。

## 首次上板顺序

1. J1/J2 空载，只连接 J3 供电/USB 和 J4 ST-LINK。
2. 烧录后确认无上电误响；初始化完成且三颗 PCAL 正常时，三个总状态灯均灭并进入 `IDLE`。
3. 打开 USB CDC 串口，发送 `VERSION`，工厂版应返回 `FW 0.3.6`，静音版应返回 `FW 0.3.6-OFFICE-SILENT`；发送 `STATUS`，应看到 `STATE=IDLE PROFILE=AUTO`。
4. 先接 24 路黄金直连夹具，发送 `START STRAIGHT24`，确认 24 路映射、状态灯和报告正确。
5. 再测试已知单点开路、两点短路和错线样件，最后才接真实 Type-C 线缆。

若 U5/U2/U3 任一地址不应答或扫描时发生 I²C 错误，三颗总状态灯不伪装成线缆 SHORT/OPEN；工厂版蜂鸣器快速鸣叫，办公室静音版始终无声。固件会把共享 `PCAL_RESET#` 拉低，强制 U2/U3 回到高阻。排查后必须复位或重新上电才能继续。

v0.3.6 将“PCAL 未释放”和“被测线缆节点恢复慢”分开处理：前者通过方向寄存器回读判为硬件错误；后者最长等待 100 ms，超时返回 `DUT_SETTLE_TIMEOUT/UNSTABLE`，保持高阻但不锁死 PCAL。VBUS/GND/CC/VCONN 使用 250 ms 激励稳定时间，其他数据脚使用 50 ms。报告会附带扫描进度、失败源、基线/释放原始值、方向寄存器与恢复耗时。

v0.3.6 保留完整有向扫描矩阵，并把方向不对称 (`ASYMMETRIC_PAIRS`) 与重复采样不一致 (`TEMPORAL_UNSTABLE_PAIRS`) 分开报告。标记型 profile 的稳定 CC/VCONN 电子路径另计 `EMARKER_ELECTRONIC_PATH_PAIRS`，不会因合法单向路径或允许表内双向 Ra 证据误报不稳定；未允许的双向连接仍是确认短路，时间抖动仍是 `UNSTABLE`。每个物理 GND/VBUS 组合独立统计：双向稳定或 GND 源→VBUS 稳定低电平判为 `POWER_CROSS_FAULT`；仅 VBUS 源→GND 稳定低电平或存在电源对时间不稳定证据判为 `POWER_CROSS_SUSPECT`。

扫描期间三颗总状态灯保持熄灭。24 路 U5 在开始时清空；J1 的 24 个源完成释放/提交后按 A1..B12 逐个点亮，J2 的 24 个源完成后按同顺序逐个熄灭。刷新只发生在 `completed_source_count` 增加时；扫描完成或异常后立即由真实结果位图覆盖。

`POWER_CROSS_PAIRS` 保持兼容，但现在表示出现任意方向或时间不稳定证据的唯一 GND/VBUS 组合总数；`POWER_CROSS_BIDIR_PAIRS`、`POWER_CROSS_GND_SOURCE_TO_VBUS_PAIRS`、`POWER_CROSS_VBUS_SOURCE_TO_GND_PAIRS`、`POWER_CROSS_TEMPORAL_PAIRS` 给出细分原因。时间不稳定证据可与稳定方向证据重叠。这些结果只基于数字输入阈值，不等价于测得 0 Ω；区分金属短路、数十 kΩ 泄漏、RC 充放电或未供电电子路径仍需万用表/阻抗测量。

## USB CDC 命令

串口工具可设为 115200 8N1；CDC 实际不依赖波特率。命令不区分大小写，每条以换行结束。

```text
VERSION
STATUS
PROFILE LIST
PROFILE SET USB2_UNMARKED
START [profile]
REPORT
ABORT
HELP
```

默认 profile 是 `AUTO`。一次原始扫描后，固件在 `USB2_UNMARKED`、`USB2_EMARKED`、`FULL_EMARKED` 三个候选中按故障分数和最小能力原则选择最佳匹配；实体 START 键每次强制 AUTO。串口仍可发送 `START FULL_EMARKED` 等显式 profile 做已知 SKU 诊断。AUTO 是观测最佳拟合：全功能线 10 路高速/SBU 全断时可能与合法 USB 2.0 线不可区分，因此已知全功能 SKU 的验收仍应显式使用 `FULL_EMARKED`。

报告的 `REQUESTED_PROFILE` 记录本次请求，`DETECTED_PROFILE` 与兼容字段 `PROFILE` 记录实际分析 profile；空载/单端时检测值保持 `AUTO`。

常用选择：

| 场景 | profile | 导体正常时结果 |
|---|---|---|
| 未知 C-C 线自动识别 | `AUTO` | 按检测线型返回 |
| 24 路黄金直连夹具 | `STRAIGHT24` | `PASS` |
| 普通无 eMarker USB 2.0 C-C 线 | `USB2_UNMARKED` | `PASS` |
| 带 eMarker USB 2.0 C-C 线 | `USB2_EMARKED` | `CONDUCTORS_PASS_EMARKER_UNVERIFIED` |
| 全功能 C-C 线 | `FULL_EMARKED` | `CONDUCTORS_PASS_EMARKER_UNVERIFIED` |
| 未知线缆摸底 | `DISCOVERY` | `DISCOVERY` |

## Type-C 规则与指示

- C-C 线缆的四个 VBUS 触点属于同一预期电源组，四个 GND 触点属于同一预期地组；组内及两端之间导通是正常结构，不判短路。
- eMarker 线不再强制要求两端 B5/VCONN 低阻直通。未供电 eMarker 可能出现 VCONN 到 GND 的 Ra 或单向电子路径，这些路径允许存在但不是通过条件。
- 当前硬件只做数字通断矩阵，不能执行 USB-PD SOP' Discover Identity，也不能可靠区分合法 Ra 与 B5-GND 硬短。因此带 eMarker/全功能 profile 的导体即使全部正常，也只给出“导体通过、eMarker 未验证”，不宣称完整合规 `PASS`。

| 总状态灯 | 含义 |
|---|---|
| 绿色 | `PASS`，或导体通过但 eMarker/PD 能力未验证 |
| 红色 | 存在确认的意外双向连接，或 `POWER_CROSS_FAULT` |
| 黄色 | `MISSING_PAIRS>0` 的确认开路 |
| 红色 + 黄色 | 同时存在确认短路和确认开路 |
| 全灭并蜂鸣/报告告警 | `UNSTABLE`、`POWER_CROSS_SUSPECT` 或 `HARDWARE_ERROR`；不伪装成 SHORT/OPEN |

24 路信号灯具有两个明确模式：`SCANNING` 时只表示扫描进度，不代表导通或故障；进入 `RESULT` 后才表示稳定、双向、跨 J1/J2 确认的实际导体。即使最终结果为 `UNSTABLE`、`POWER_CROSS_FAULT` 或 `POWER_CROSS_SUSPECT`，已经确认的导体灯仍保留；只有 `HARDWARE_ERROR` 会清空全部信号灯。`CONDUCTORS_PASS_EMARKER_UNVERIFIED` 只亮绿色，`UNSTABLE/POWER_CROSS_SUSPECT/HARDWARE_ERROR` 不亮三颗总状态灯，工厂版仍通过蜂鸣和串口报告告警。

## 构建与产物校验

```powershell
.\build.ps1 -Clean
```

脚本固定使用 PlatformIO `ststm32 19.7.1`、Arduino_Core_STM32 `2.12.0` 和 Arm GCC `12.3.1`，生成 ELF/BIN/HEX、链接 map、`BUILD_INFO.txt` 与 `SHA256SUMS.txt`。构建缓存放在工作区根目录的纯 ASCII 路径下，避免 Windows GNU ld 处理中文路径时失败。

当前 v0.3.6 全量构建后，工厂版 Flash 映像为 47,932 B（73.1%），静音版为 47,964 B（73.2%）；两者 RAM 均保留 7,280 B（29.6%，包含链接器预留的 heap/stack）。哈希与详细信息见本次 `BUILD_INFO*.txt`、`SHA256SUMS*.txt` 和 map。
