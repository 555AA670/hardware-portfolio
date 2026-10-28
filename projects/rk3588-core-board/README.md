# RK3588 核心板

一块基于 Rockchip RK3588 的自制核心板，采用手工焊接。项目仍在调试中。

![核心板实物](images/web-board.jpg)

## 当前进展

- 核心板已启动 Android 12，并通过 USB OTG 连接 ADB 和 scrcpy。
- 启动日志报告 4 GB LPDDR4X，四个通道各 1 GB；DDR 初始化到 2112 MHz。
- 已在 USB OTG 直连电脑时使用投屏；经过部分扩展坞时曾出现连接不稳定。
- 更多外设和长期稳定性仍需测试。

![PCB 布线](images/web-pcb-routing.jpg)

## PCB 设计与制造参数

- 六层 PCB；采用 0.3/0.4 mm 免费过孔规格。
- 层叠采用截图中标注的华秋 06161HA3-1080 结构。阻抗计算包含 50 Ω、40 Ω 单端和 80 Ω 差分；具体线宽、间距和参考层见下图。计算值为设计参考，实际成品以板厂工艺和测试结果为准。

### 层叠与阻抗计算

![六层板层叠及阻抗计算参数](images/stackup-impedance.png)

### PCB 布线视图

![PCB 布线视图一](images/pcb-133316.jpg)

![PCB 布线视图二](images/pcb-133323.jpg)

![PCB 布线视图三](images/pcb-133331.jpg)

![PCB 布线视图四](images/pcb-133339.jpg)

## 文件说明

本仓库目前提供项目图片与进展记录。原理图、PCB 工程及固件源码尚未包含在此版本中。

本项目参考 Rockchip 官方资料。第三方软件、固件及资料的版权和许可归其各自权利人所有。
