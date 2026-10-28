# 构建、版本与操作

本包默认不自动烧录或启动功率级。实际带载验证与未验证范围见[测试记录](test-results.md)。完整硬件资料尚不齐全，复刻者需要先核对引脚、驱动极性和实装BOM。

## 文件入口

- MCU：[FirmwareCascade](../CleanBuck36/FirmwareCascade/README.md)。唯一可烧录工程为其`MDK-ARM/CleanBuck36.uvprojx`。
- 上位机：[Monitor/FixedSerialMonitor.csproj](../CleanBuck36/FirmwareCascade/Monitor/FixedSerialMonitor.csproj)。
- 控制源码：[Simulation/CurrentCascade](../CleanBuck36/Simulation/CurrentCascade/README.md)。
- 原样测试映像：[v7 HEX](../releases/v7/CLEANBUCK36_BENCH.hex)及[源码哈希](../releases/v7/build-manifest.json)。

原样映像SHA256：

`21D226A76234C3449E1EEBFFD09E33C12AAA3889BD808AB6CC19C751316964CC`

## MCU构建

Windows + Keil ARMCC5（记录版本5.06 update 5 build528）。从项目目录运行：

```powershell
& .\CleanBuck36\FirmwareCascade\Build.ps1
# 非默认位置：追加 -CompilerRoot 'D:\Keil\ARM\ARMCC\bin'
python .\CleanBuck36\FirmwareCascade\Verify.py
```

输出在`CleanBuck36/FirmwareCascade/MDK-ARM/Build/CLEANBUCK36_BENCH`。控制模块单独用`-O3 -Otime`，其余`-O2`，未启用fast-math。直接在uVision用不同优化编译，不保证得到已测试映像或相同时间预算。

`Build.ps1`只编译，不下载。发布整理仅把工具位置设为参数、构建清单改成相对路径、验证脚本改为在本地Build目录找HEX；main.c、board.c、配置和C算法没有改变。新编译映像与原样HEX分开比较，先核对身份和哈希，不将任意重编结果说成已经实测。

## 上位机构建与检查

Windows + .NET 7 SDK / Desktop Runtime（复现原项目环境，未在此打包升级框架）。

```powershell
dotnet build .\CleanBuck36\FirmwareCascade\Monitor\FixedSerialMonitor.csproj -c Release -o .\build\monitor
dotnet run --project .\CleanBuck36\FirmwareCascade\HostTests\ManualStartTests.csproj -c Release -- '.\build\monitor\G474串口监视器.dll'
```

这些检查不打开串口、不显示功率启动窗口，不发送START。完成后手动运行`build/monitor/G474串口监视器.exe`，选择实际串口，115200；历史COM31只是原电脑端口，不能当作所有电脑的默认。

## 固件与窗口匹配

只接受`CLEANBUCK36-CASCADE-20261027-v7`。MCU上电关闭四路输出，预检通过后由用户在本地按钮确认START；本机HTTP API仅供查看及停机，`POST /api/start`始终拒绝。停止/故障后需检查并复位才能再次测试，不自动清故障或循环启动。

无正常运行时长限制；39 V采样过压、8 A原始输入采样过流、温度、ADC陈旧与执行时间保护仍在。code1温度保护可能在约54°C附近触发，不要误认为仍有10秒计时限制。

首次接板应确认供电、地、引脚、极性、负载额定功率和独立断电方式；下载结束先确认`run=0`、`OEN=000`及四路门极命令关闭。10 Ω在36 V约130 W，负载与散热能力需匹配。当前仅有一次约66秒带载记录，以较短观察起步，不把它作为无人值守运行固件。
