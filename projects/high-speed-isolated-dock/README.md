# VL822 高速加隔离拓展坞

基于 VL822 的自制四层拓展坞，完成 USB 扩展、千兆以太网与隔离调试接口的电路设计及 PCB 布局布线。通过 P4 端口的 USB 3.0 / USB 2.0 分别连接网卡与隔离支路，在同一块板上集成日常扩展接口和隔离调试接口。样机已完成 USB 设备枚举与文件复制测试，复制截图显示约 **331 MB/s**，网口协商速率为 **1.0 Gbps**。

![VL822 拓展坞实物与 Altium 3D 设计对照](images/board-and-3d-design.jpg)

## 项目概览

| 项目 | 内容 |
| --- | --- |
| 主控与上游 | VL822，Type-C 经 VL162 接入 |
| USB 扩展 | 3 路 USB-A 3.0 |
| 以太网 | RTL8153B-VB-CG，搭配 HR911130A 集成网络变压器的千兆 RJ45 |
| USB 隔离 | 两颗 CH318T，采用网络变压器耦合 |
| 隔离侧接口 | CH347F 调试接口、1 路 USB-A 2.0 |
| 隔离供电 | B0505S-2WR3，2 W 隔离电源 |
| PCB | 四层板，华秋 04161H05-7628 叠构 |
| 布线与分区 | 90 Ω / 100 Ω 差分阻抗计算，2 mm 隔离开槽 |

[功能设计](#功能设计) · [板级验证](#板级验证) · [PCB 设计](#pcb-设计)

## 功能设计

### USB 3.0 扩展与千兆以太网

Type-C 上游经 VL162 接入 VL822，三个下行端口连接 USB-A 3.0。P4 的 USB 3.0 信号连接 **RTL8153B-VB-CG**，再通过 **HR911130A** 引出千兆网口，将 USB 扩展与有线网络集中到同一上游接口。

### USB 2.0 隔离扩展

P4 的 USB 2.0 信号接入上位机模式的 **CH318T**，经网络变压器耦合至隔离侧的另一颗 CH318T。下位机 CH318T 的两个 USB 2.0 下行端口分别连接 **CH347F** 和普通 USB-A，形成隔离调试与隔离外设两路接口。

### 隔离调试与供电

CH347F 的调试信号通过板右侧排针引出，与隔离 USB-A 共用隔离侧电源和地。供电采用 **B0505S-2WR3、2 W 隔离电源**，配合 USB 数据链路的变压器耦合构成隔离支路。

<details>
<summary>展开查看硬件功能框图</summary>

```mermaid
flowchart LR
    HOST["上游 Type-C"] --> VL162["VL162"] --> HUB["VL822"]
    HUB -->|"其余 3 个端口"| USB3["3 × USB-A 3.0"]
    HUB -->|"P4 USB 3.0"| RTL["RTL8153B-VB-CG"]
    RTL --> RJ45["HR911130A<br/>集成网络变压器的千兆 RJ45"]
    HUB -->|"P4 USB 2.0"| U1["CH318T<br/>上位机模式"]
    U1 --> TX["网络变压器<br/>USB 信号隔离"]
    TX --> U2["CH318T<br/>下位机模式"]
    U2 -->|"USB 2.0"| CH347["CH347F"]
    U2 -->|"USB 2.0"| USBA["隔离 USB-A"]
    POWER["非隔离侧电源"] --> DC["B0505S-2WR3<br/>2 W 隔离电源"] --> ISO["隔离侧电源"]
    ISO -.-> U2
    ISO -.-> CH347
    ISO -.-> USBA
```

</details>

## 板级验证

### USB 设备枚举

电脑端 USB 设备树显示，VIA Hub 的 USB 2.0 与 USB 3.x 分支均已枚举。USB 2.0 分支的 P4 下可见两级沁恒 Hub 及 CH347F 复合设备，USB 3.x 分支的 P4 下可见 **Realtek USB GbE Family Controller**，与 P4 分别连接隔离 USB 和网卡的设计对应。

| 验证内容 | 截图结果 |
| --- | --- |
| USB Hub | VIA Labs USB 2.1 Hub、USB 3.1 Gen 1 Hub 均被识别 |
| 隔离 USB 链路 | 枚举出两级 Nanjing QinHeng USB 2.0 Hub 及下游 CH347F |
| CH347F 双串口 | SERIAL-A：COM19；SERIAL-B：COM16 |
| 以太网设备 | Realtek USB GbE Family Controller 被识别 |
| CH347F 多功能接口 | UART+SPI+I2C+JTAG 节点显示“28”提示，驱动状态待确认 |

![拓展坞 USB 设备树：VIA Hub、沁恒 Hub、CH347F 双串口与 Realtek 网卡枚举结果](images/usb-device-enumeration.png)

### USB 3.0 文件传输

样机在 **USB 5 Gbps 链路**下运行稳定。通过拓展坞进行文件复制时，截图中的速度曲线较为平稳，接近完成时显示约 **331 MB/s**；5 Gbps 为 USB 链路速率，331 MB/s 为此次文件复制截图中的传输速度。

![通过 VL822 拓展坞复制文件，截图速度约 331 MB/s](images/usb3-file-transfer.png)

### 千兆以太网链路

Windows 已识别 **Realtek USB GbE Family Controller**，以太网状态显示连接速率为 **1.0 Gbps**，确认网口完成千兆链路协商。

![RTL8153B 网卡的以太网连接状态，协商速率 1.0 Gbps](images/ethernet-1gbps-link.png)

## PCB 设计

采用四层 PCB 完成 VL822、网卡、隔离 USB 与供电电路的布局布线。三个 USB-A 3.0 和千兆网口沿板边布置，隔离调试区集中在板右侧。

### 层叠与阻抗

使用华秋 **04161H05-7628** 层叠结构进行阻抗计算，覆盖 **90 Ω、100 Ω 差分**参数。下图给出对应的线宽、线距、参考层及介质厚度；计算结果分别约为 **89.70–90.24 Ω** 和 **100.04–100.23 Ω**。

![四层板层叠与 90 Ω、100 Ω 差分阻抗计算参数](images/stackup-impedance.png)

### 布局与布线

以下两张图展示不同布线视图，可见 VL822 向 USB-A 接口的差分走线、网卡至 RJ45 的连接，以及隔离侧调试排针的信号引出。

![VL822 拓展坞 PCB 布局与接口分布](images/pcb-routing-overview.png)

![VL822 拓展坞布线与供电铜皮](images/pcb-routing-secondary.png)

### 隔离边界与铜皮分区

主电路区与隔离调试区之间采用 **2 mm 开槽**，隔离侧独立使用 **ISO+5V、GND_ISO** 网络。下图展示电源铜皮、隔离侧连接与开槽边界的布局关系。

![拓展坞铜皮分区与 2 mm 隔离开槽](images/pcb-isolation-copper.png)

USB 隔离支路使用两颗 CH318T 之间的网络变压器；HR911130A 内置的网络变压器用于以太网接口。

[CH318 技术手册](https://www.wch.cn/downloads/CH318DS1_PDF.html) · [HR911130A 产品资料](https://www.hanrun.com/rj45_1000/203.html) · [返回项目作品集](../../README.md)
