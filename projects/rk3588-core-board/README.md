# RK3588 六层核心板

基于 Rockchip RK3588 设计制作的核心板，完成从电路设计、六层 PCB 布局布线到手工焊接和系统启动调试。

![RK3588 核心板实物](images/web-board.jpg)

## 项目概览

| 项目 | 内容 |
| --- | --- |
| 主处理器 | Rockchip RK3588 |
| 内存 | 4 GB LPDDR4X，四通道 |
| PCB | 六层板，采用 0.3/0.4 mm 免费过孔规格 |
| 层叠结构 | T — GND — PWR — PWR — GND — B |
| 系统与调试 | Android 12、USB OTG、ADB、scrcpy |

## PCB 设计

采用六层板布局，从顶层到底层依次为 **T — GND — PWR — PWR — GND — B**。PCB 设计涵盖 RK3588、LPDDR4X、高速信号、电源及接口的布局与布线，DDR 信号按照官方要求进行等长设计。

阻抗计算采用华秋 **06161HA3-1080** 层叠结构，计算参数包括 50 Ω、40 Ω 单端和 80 Ω 差分阻抗。

![六层板层叠与阻抗计算](images/stackup-impedance.png)

### 四层布线展示

![布线层视图 1](images/pcb-133316.jpg)

![布线层视图 2](images/pcb-133323.jpg)

![布线层视图 3](images/pcb-133331.jpg)

![布线层视图 4](images/pcb-133339.jpg)

## 样机与系统验证

- 手工焊接核心板并完成上电调试。
- LPDDR4X 四通道均完成初始化，启动日志显示总容量为 4 GB，初始化频点为 2112 MHz。
- 成功启动 Android 12，通过 USB OTG 实现 ADB 连接、scrcpy 投屏和音频转发。
- 实现通过 OTG 共享电脑网络。

![核心板 PCB 布线总览](images/web-pcb-routing.jpg)
