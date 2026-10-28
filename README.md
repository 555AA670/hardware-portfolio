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

[查看项目设计与成果 →](projects/rk3588-core-board/README.md)

### [高速加隔离拓展坞](projects/high-speed-isolated-dock/README.md)

四层 PCB 集成 3 路 USB-A 3.0、千兆以太网、隔离 USB-A 与 CH347F 调试接口。通过两颗 CH318T、网络变压器和 2 W 隔离电源构建隔离支路，采用 2 mm 开槽分区，并完成 90 Ω / 100 Ω 差分阻抗计算。

[查看项目 →](projects/high-speed-isolated-dock/README.md)
