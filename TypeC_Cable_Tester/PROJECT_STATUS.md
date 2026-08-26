# Type-C 线缆测试工具当前进展

> 更新日期：2026-08-26  
> 当前状态：固件 v0.3.6 已发布并完成实板烧录，进入样线与故障样件回归阶段。

## 当前基线

- 固件与许可证基线：[`e684cec`](https://github.com/Liuxianseen/Hardware/commit/e684cec)；本进展记录不修改固件源码或发布产物。
- 固件发布页：[v0.3.6](https://github.com/Liuxianseen/Hardware/releases/tag/v0.3.6)。
- 硬件开源页：[立创开源硬件平台](https://oshwhub.com/liu_xianseen/project_mzckqhhy)。
- 目标器件：STM32C071G8U6、三颗 PCAL6524、24 路信号灯、三色状态灯、蜂鸣器和 USB CDC。

## 已完成

### 固件与实板

- 已实现 48 端点双向有向扫描：前 24 轮由 J1 的 A1～B12 依次单点拉低，后 24 轮换为 J2 依次输出；其余 47 点始终为高阻输入。
- 扫描阶段信号灯实时显示进度：J1 阶段逐灯填满，J2 阶段逐灯熄灭；完成后由真实导体位图覆盖，进度灯不与最终结果混用。
- 已实现 `AUTO`、`USB2_UNMARKED`、`USB2_EMARKED`、`FULL_EMARKED`、`STRAIGHT24` 和 `DISCOVERY` 等测试模式。
- 已纳入 Type-C 合法并联规则：VBUS 触点按电源组处理、GND 触点按地组处理，USB 2.0 与全功能线按各自预期拓扑分析。
- 已区分开路、确认短路、错线、方向不对称、时间不稳定、VBUS/GND 电源交叉及 PCAL 硬件错误。
- ST-LINK 的 `VTref / SWDIO / SWCLK / NRST / GND` 接线已验证；连接 NRST 后可使用 SWD under-reset 稳定重复烧录。
- 已提供工厂有声版与办公室静音版；当前新板可使用工厂有声版继续验证。
- 已完成空载、单端插入、USB 2.0 线和全功能线的多轮实物观察；最新版显示逻辑已由使用者确认总体符合预期。

### 自动验证与发布

- 36 项固件规则测试通过。
- 8 个 C 翻译单元语法检查通过。
- Arm GCC 12.3.1 全量编译与链接通过。
- v0.3.6 的 BIN、HEX、ELF、构建信息和 SHA-256 校验文件已发布。
- Release 已补充 88,167,646 字节的对应源码与许可证包；其 SHA-256 为 `8ae249bbbed5e57feee0e791ec7464301bfbb3012ee7cbaa0ebca8ecec67e959`。

### 开源许可证

- 项目原创固件、测试、脚本和文档： [Apache License 2.0](../LICENSE)。
- 立创平台上的原理图和 PCB：OpenAtom OHL 1.0。
- 预编译固件中的 Arduino Core、HAL/CMSIS、ST USB Device、GCC/newlib 运行库继续采用各自许可证，详见 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)。

## 已知检测边界

- 当前硬件验证直流通断与线序，不测差分阻抗、插损、串扰、眼图或 USB 3.x 实际速率。
- 当前硬件不能执行 USB-PD SOP' Discover Identity，因此只能报告 eMarker 相关导体或电子路径特征，不能宣称 eMarker 合规通过。
- `AUTO` 是观测拓扑的最佳拟合；当全功能线的全部高速/SBU 导体都开路时，结果可能与合法 USB 2.0 线不可区分。验收已知 SKU 时应显式选择对应 profile。
- VBUS/GND 电源交叉判断基于数字输入阈值，不等同于测得 0 Ω；疑似泄漏或 RC 路径仍需万用表或阻抗设备复核。

## 下一步验证清单

- 使用黄金直连夹具执行 `START STRAIGHT24`，保存完整 CDC 报告。
- 对 USB 2.0 无 eMarker、USB 2.0 带 eMarker及全功能线分别完成四种插头方向回归。
- 使用已知单点开路、两点短路、错线和接触抖动样件验证灯光、蜂鸣器及报告字段。
- 对 VBUS/GND 交叉与疑似单向电子路径样件进行数字结果和万用表阻值交叉验证。
- 连续插拔、重复测试并记录误判率和单次扫描时间，为工厂测试阈值和操作规范提供数据。

## 恢复工作入口

- 固件设计与命令：[firmware/README.md](firmware/README.md)
- 烧录、接线与首次上板：[firmware/target/stm32c071/README.md](firmware/target/stm32c071/README.md)
- 测试与 LED 状态机：[firmware/测试与指示状态机.md](firmware/测试与指示状态机.md)
- 第三方源码与重链接：[SOURCE_AND_RELINKING.md](../SOURCE_AND_RELINKING.md)

常用验证命令：

```powershell
cd .\TypeC_Cable_Tester\firmware
python tests\check_c_syntax.py
python -m unittest discover -s tests -p test_firmware_model.py -v

cd .\target\stm32c071
.\build.ps1 -Clean
```
