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

### [VL822 高速加隔离拓展坞](projects/high-speed-isolated-dock/README.md)

完成基于 VL822 的四层拓展坞电路设计与 PCB 布局布线，集成 3 路 USB-A 3.0、千兆以太网、隔离 USB-A 与 CH347F 调试接口。

| 方向 | 设计与验证 |
| --- | --- |
| 接口扩展 | VL822 P4 的 USB 3.0 / USB 2.0 分别连接网卡与隔离支路 |
| 隔离设计 | 双 CH318T、网络变压器、2 W 隔离电源与 2 mm 开槽 |
| PCB 设计 | 四层叠构、90 Ω / 100 Ω 差分阻抗计算与分区布线 |
| 板级验证 | USB 10 Gbps 链路协商、文件复制截图约 331 MB/s、网口协商 1.0 Gbps、CH347F 双串口枚举 |

**关键词：** VL822 · USB 3.0 · 千兆以太网 · CH318T · CH347F · 四层 PCB · 隔离设计

**当前版本：** 高速 USB-A 连接器需更换为明确支持 10 Gbps 的型号；CH347F 功能尚未验证。

[查看项目设计与成果 →](projects/high-speed-isolated-dock/README.md)
