# 硬件踩坑记录

[返回 RK3588 项目](README.md)

## 01｜TYPEC PHY 的两颗 8.2 kΩ REXT 电阻不能漏焊

### 现象与定位

样机最初只焊接了 OTG0_REXT 的 200 Ω 电阻，TYPEC0_REXT 和 TYPEC1_REXT 的两颗 8.2 kΩ 电阻均未焊接。此时 USB OTG 下载口没有正常枚举，RKDevTool 无法识别 MASKROM 设备。

排查时，BOOT、复位、24 MHz 时钟及 USB PHY 电源已逐项确认。继续检查 TYPEC PHY 电路后发现两颗 REXT 电阻漏焊，补焊后恢复 USB 枚举，RKDevTool 成功识别 MASKROM。

### 对应位置

| 位号 | 网络 | 阻值 | 连接 |
| --- | --- | --- | --- |
| R54 | TYPEC0_REXT | 8.2 kΩ | REXT 至 GND |
| R59 | TYPEC1_REXT | 8.2 kΩ | REXT 至 GND |

![TYPEC0 与 TYPEC1 的 8.2 kΩ REXT 电阻](images/typec-rext-8k2.png)

### 实测结果

| 项目 | 本板实测 |
| --- | --- |
| 输入电压 | 5 V |
| MASKROM 状态输入电流 | 约 130 mA |
| 电脑端识别结果 | RKDevTool 显示“发现一个 MASKROM 设备” |

**装配要点：本板即使用 Micro-USB OTG0 下载，TYPEC0_REXT、TYPEC1_REXT 的两颗 8.2 kΩ 电阻也必须焊接。** 调试时结合约 130 mA 的实测电流观察上电状态，以 RKDevTool 的 MASKROM 枚举结果确认下载通道。

## 02｜USB Device Tree Viewer 与 RKDevTool 分开使用

### 现象

本次调试中，同时打开 **USB Device Tree Viewer（USB Tree Viewer）** 和 **RKDevTool** 时，出现设备识别异常及下载报错，影响 MASKROM 下载操作。

### 处理方式

查看 USB 拓扑与设备描述符时使用 USB Device Tree Viewer；准备使用 RKDevTool 识别设备或下载固件前，先退出 Tree Viewer。若已经出现识别异常，关闭 Tree Viewer 后重新连接 OTG，再由 RKDevTool 识别并执行下载。

**使用要点：USB Device Tree Viewer 和 RKDevTool 不同时运行。**

### MASKROM 成功识别画面

RKDevTool 底部显示“发现一个 MASKROM 设备”，左侧串口窗口记录 DDR 与 eMMC 初始化信息。

![RKDevTool 成功识别 MASKROM，串口显示 DDR 与 eMMC 初始化日志](images/rkdevtool-maskrom.png)
