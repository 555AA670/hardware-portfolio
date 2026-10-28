# 双环v7固件与上位机

身份：`CLEANBUCK36-CASCADE-20261027-v7`。

从本项目根目录按[构建与操作](../../docs/build-and-run.md)编译；完整控制说明见[控制链](../../docs/control-and-pwm.md)。

48 V→36 V，200 kHz，120.588 ns，90%最大有效指令。电压外环10 kHz、平均输入电流内环50 kHz；轻载完整脉冲补能。输入桥Q1/Q2互补，输出桥Q3/Q4关闭；8 A采样关断、39 V采样过压、NTC和执行时间保护保留。

上电关闭，人工按钮START，正常运行无时间限制；故障/STOP后不自动重启。唯一板级工程为`MDK-ARM/CleanBuck36.uvprojx`。同级`../Firmware`仅包含仿真依赖，没有本版烧录入口。

`Build.ps1`支持`-CompilerRoot`，默认原ARMCC5位置；控制模块`-O3 -Otime`，其他`-O2`。`Verify.py`检查构建来源、模型C哈希、控制/保护链、版本匹配与无正常计时停止。它只离线检查，不烧录，不启动串口。

匹配上位机由`Monitor/FixedSerialMonitor.csproj`构建，`HostTests`包含548项按钮策略、12项已编译窗口/API检查，均不连接硬件。

项目首页：[四开关电源](../../README.md)。
