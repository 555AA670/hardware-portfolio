# RK3588 核心板固件

[返回核心板项目](../README.md) · [固件发布页](https://github.com/555AA670/hardware-portfolio/releases/tag/rk3588-firmware-20261027)

| 固件 | 下载 |
| --- | --- |
| Android 12 A1 | [Android12-core-a1-factory.zip](https://github.com/555AA670/hardware-portfolio/releases/download/rk3588-firmware-20261027/Android12-core-a1-factory.zip) |
| Buildroot F2 | [Buildroot-core-f2.zip](https://github.com/555AA670/hardware-portfolio/releases/download/rk3588-firmware-20261027/Buildroot-core-f2.zip) |

## Android 12 A1

Android 镜像修改自香橙派 5 Plus Android 12 v1.0.2，沿用原版 Linux 5.10.160 内核，修改设备树、启动配置和 Android 初始化配置以适配本核心板。

原版镜像来源：https://drive.google.com/file/d/1urwy97uNiXwbR3xdltzEO1MN90C32TDZ/view

解压 Android12-core-a1-factory.zip 后，得到 Android12-core-a1-factory.img。

## Buildroot F2

Buildroot 基础固件使用 Linux 5.10.198，保留核心板启动、电源管理、eMMC、串口、板载 Micro-USB OTG 和片上加速器，底板相关外设暂未启用。模型和演示应用需另行部署。

解压 Buildroot-core-f2.zip 后，得到 Buildroot-core-f2.img，以及配套的 MiniLoaderAll.bin、parameter.txt。

## 烧录与连接

- 两个 .img 均为 Rockchip RKFW 完整固件包，使用 RKDevTool 的“升级固件”功能烧录。
- 完整烧录会覆盖 eMMC 上原有系统和数据。
- 板载 Micro-USB OTG0 用于下载和系统连接。
- 调试串口：UART2_M0，1500000 波特率，8N1。
- Buildroot 登录：root / rockchip。
- Android 保留 ADB 授权验证。

[SHA256SUMS.txt](SHA256SUMS.txt) 提供压缩包和解压后镜像的校验值。

## 硬件参考

原理图参考 Rockchip 官方 RK3588_AIOT_REF_SCH_V10_20211228。底板尚未完成，外设还未验证，PCB 暂不开源。
