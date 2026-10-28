# 硬件项目作品集

展示个人硬件项目的设计与实现，涵盖电路设计、PCB 布局布线、手工焊接、系统调试和板端应用部署。

## 项目

### [RK3588 六层核心板](projects/rk3588-core-board/README.md)

完成基于 Rockchip RK3588 的六层核心板设计与手工焊接，搭载 4 GB LPDDR4X。样机已运行 Android 12 与 3D 游戏，并完成内存测试及 CPU / NPU 本地大模型部署。

| 方向 | 实现成果 |
| --- | --- |
| 硬件设计 | 六层叠构、BGA 扇出、DDR 等长与阻抗设计 |
| 样机调试 | LPDDR4X 2112 MHz 与 eMMC 初始化、1024 MB × 10 轮内存测试通过 |
| 图形系统 | Android 12、scrcpy 投屏、3D 游戏运行 |
| 边缘 AI | RKLLM / Qwen3.5-2B NPU 推理，llama.cpp / Qwen3.5-4B CPU 推理 |

**关键词：** RK3588 · LPDDR4X · 六层 PCB · 阻抗控制 · Android · RKLLM · llama.cpp

[查看项目设计与成果 →](projects/rk3588-core-board/README.md) · [固件下载](projects/rk3588-core-board/README.md#固件下载)

### [VL822 高速加隔离拓展坞](projects/high-speed-isolated-dock/README.md)

基于 VL822 的四层拓展坞，集成 3 路 USB-A 3.0、千兆网口、隔离 USB-A 和 CH347F。USB 文件复制约 331 MB/s，隔离侧约 37.2 MB/s，已记录 10 Gbps 链路协商结果。

**主要芯片：** VL822 · RTL8153B-VB-CG · CH318T · CH347F

USB-A 连接器在 10 Gbps 下信号完整性裕量不足，需更换型号；CH347F 功能尚未验证。项目页提供 PCB 和原理图下载。

[查看项目 →](projects/high-speed-isolated-dock/README.md)

### [STM32G474 四开关 Buck-Boost 数字电源](projects/stm32g474-four-switch-buck-boost/README.md)

基于 STM32G474 HRTIM 的四开关数字电源，PCB 和元器件按 100 V 耐压、400 W 功率设计，采用 200 kHz PWM、电压外环与平均输入电流内环，配套 C# 串口上位机和 C / MATLAB 仿真。

完成 Buck 模式 48 V → 36 V、10 Ω 负载、约 130 W 测试，效率 93.2%，运行 66.35 秒后温度保护停机。项目包含源码、v7 固件、Altium 原理图与 PCB、实物照片和测试日志。

**关键词：** STM32G474 · HRTIM · 四开关 Buck-Boost · 双环控制 · MATLAB · C#

[查看项目设计与测试 →](projects/stm32g474-four-switch-buck-boost/README.md)

### [SW3518S 双口快充供电板](projects/sw3518s-fast-charging-board/README.md)

基于 SW3518S 的 USB-A + USB-C 双口快充供电板，尺寸约 31.2 × 57.1 mm，已进行手机和电脑充电测试。

[查看项目与工程 →](projects/sw3518s-fast-charging-board/README.md)

## PCB Layout 项目

### [RK3568 + LPDDR4X + MCU 六层 ROS 机器人控制板](projects/rk3568-mcu-ros-control-board/README.md)

基于 RK3568、LPDDR4X 与 MCU 的六层机器人控制板，面向 ROS 应用。已完成 PCB 布局布线和打板，尚未焊接及上电验证。

[查看 PCB Layout →](projects/rk3568-mcu-ros-control-board/README.md)

### [K230 + LPDDR4 六层紧凑核心板](projects/k230-lpddr4-flight-core-board/README.md)

45 × 45 mm 六层核心板，采用嘉立创免费工艺，面向飞控应用。

[查看 PCB Layout →](projects/k230-lpddr4-flight-core-board/README.md)
