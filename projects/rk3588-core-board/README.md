# RK3588 六层核心板

基于 Rockchip RK3588 的自制核心板，完成电路设计、六层 PCB 布局布线、手工焊接及系统启动调试。在同一块样机上实现 Android 12 图形系统运行、内存测试与 CPU / NPU 本地大模型推理，将硬件设计落实到可运行的应用平台。

![RK3588 核心板实物与 PCB 设计](images/web-board.jpg)

## 项目概览

| 项目 | 内容 |
| --- | --- |
| 主处理器 | Rockchip RK3588 |
| 内存 | 4 GB LPDDR4X，四通道，初始化频点 2112 MHz |
| PCB | 六层板，0.3/0.4 mm 免费过孔规格 |
| 层叠结构 | T — GND — PWR — PWR — GND — B |
| 系统应用 | Android 12 图形界面、3D 游戏、本地大模型对话 |
| 调试与连接 | 串口、USB OTG、ADB、scrcpy 投屏与音频转发、USB 网络共享 |

## 应用成果

### NPU 本地大模型推理

使用 RKLLM 在 RK3588 上部署 **Qwen3.5-2B W8A8** 模型，完成中文交互与连续对话。运行截图中启用了 3 个 NPU 核心，并记录了模型加载、文本预填充和生成阶段的数据。

一次中文回答生成 162 个 token，生成速度为 **7.88 token/s**，预填充速度为 **42.04 token/s**，峰值内存占用约 **2257 MB**。这项应用把核心板的内存、电源与 NPU 运行环境串联到实际推理任务中。

![Qwen3.5-2B NPU 中文对话与性能数据](images/npu-qwen35-2b.png)

### CPU 本地大模型推理

通过 **llama.cpp** 加载 **Qwen3.5-4B Q4_K_M GGUF** 模型，在板端完成多轮文本交互。截图中的生成速度约为 **5.3–7.2 token/s**，展示了同一硬件平台上的 CPU 推理方案与命令行应用部署能力。

![llama.cpp 运行 Qwen3.5-4B](images/cpu-qwen35-4b.png)

### Android 图形系统与 3D 应用

核心板成功启动 Android 12，通过 USB OTG 接入电脑，使用 ADB 安装应用、scrcpy 显示并操作安卓界面。《我的世界》试玩版已进入实际游戏场景，展示了样机运行图形系统与 3D 应用的成果。

同一 OTG 接口还实现了音频转发和电脑网络共享，形成便于开发与演示的桌面调试环境。

![Android 12 系统运行界面](images/android12-system.png)

![我的世界试玩版实机运行](images/minecraft-demo.jpg)

## 内存与样机验证

LPDDR4X 四通道完成初始化，启动日志报告总容量 **4 GB**，初始化频点 **2112 MHz**。在系统启动后进一步使用 memtester 检查内存读写，包括随机数据、算术比较、位模式和顺序写入等测试项。

| 测试 | 截图结果 |
| --- | --- |
| 2048 MB 内存测试，1 轮 | PASS，0 failures |
| 10 轮循环内存测试 | PASS，0 failures |

![2048 MB memtester 测试通过](images/memtester-2048mb.png)

<details>
<summary>展开查看 10 轮循环测试结果</summary>

![10 轮 memtester 测试结果](images/memtester-10-loops.png)

</details>

## PCB 设计

参考 Rockchip 官方硬件方案，完成处理器、LPDDR4X、电源和接口电路的设计与板级实现。采用六层 PCB，在 **T — GND — PWR — PWR — GND — B** 叠构下完成 BGA 扇出、DDR 等长、供电分配及接口布线，并使用 **0.3/0.4 mm 免费过孔规格**实现样机制作。

### 层叠与阻抗

使用华秋 **06161HA3-1080** 层叠结构进行阻抗计算，覆盖 **50 Ω、40 Ω 单端**及 **80 Ω 差分**参数。下图给出对应的线宽、线距、参考层和介质厚度。

![六层板层叠与阻抗计算参数](images/stackup-impedance.png)

### 四层布线

以下四张图对应六层板中的四个布线层，展示 DDR 等长、处理器扇出、电源铜皮与接口走线的布置。

![布线层视图 1](images/pcb-133316.jpg)

![布线层视图 2](images/pcb-133323.jpg)

![布线层视图 3](images/pcb-133331.jpg)

![布线层视图 4](images/pcb-133339.jpg)

<details>
<summary>展开查看 PCB 布线总览</summary>

![核心板 PCB 布线总览](images/web-pcb-routing.jpg)

</details>
