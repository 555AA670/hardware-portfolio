# 算法仿真与复现

离线模型以48→36 V、10 Ω、15 µH、901 µF为标称工况。模型结果与实物带载记录分开保存；最终报告中的`hardwareValidated=false`、`firmwareFlashed=false`表示**当时离线仿真阶段的范围**，不否认后来v7已烧录，也不因后来短时运行就改成全面硬件验证通过。

![仿真控制链](../CleanBuck36/Simulation/Results/input-cascade/cascade-control-path.png)

## 方法与结果

实际单精度C控制器接入逐PWM周期的LC/二极管求解器；ADC量化、端口电流观测、指令延迟、限幅、轻载补能和故障注入进入模型。MATLAB独立计算线性模型与裕量，并核对C内环冲激响应。

| 检查 | 最终结果 |
| --- | --- |
| 扩展线性组合 | 2313/2313满足设定判据 |
| 内环最小相位 / 增益裕量 | 54.85° / 11.50 dB |
| 外环最小相位 / 增益裕量 | 75.22° / 13.74 dB |
| 非线性场景 | 31/31满足各自判据 |
| 原生C脉冲及不变量检查 | 4,955,164项 |
| C与MATLAB冲激差 | 最大约1.09e−10 duty，相对约7.28e−6 |

31项包括24组正常电压场景、2组独立电流跟踪、1组电流参考限幅、1组断开反馈的反向检查、1组不可达目标关断及2组ADC故障注入。后几类期望结果是限流或关断，不能写成“31组都稳定输出36 V”。

模型名义稳态约36.012 V；执行增益0.9接载时保守电压下界32.409 V，恢复至±1%约145 ms；空载尾部周期内纹波保守上界157 mVpp。均为模型数值。

参数范围含L10.5～22 µH、C630～1171 µF、负载7.5～20 Ω、输入45～52 V、执行增益0.9～1、电流观测20～100 µs、ESR3～40 mΩ与传感增益±10%。端点检查不构成整个连续参数域的数学证明。

## 环境与运行

已使用MATLAB R2024b + Control System Toolbox、MATLAB附带LCC64，另用Armclang检查Cortex-M4对象。没有打包商业工具本身。所有指令只离线构建与运行，不连接串口、不下载、不START。

在本项目目录打开MATLAB：

```matlab
addpath(fullfile(pwd,'CleanBuck36','Simulation'));
% 默认复核已选择的参数；不改动C系数。
verify_input_cascade_linear;
```

PowerShell在本项目目录执行：

```powershell
& .\CleanBuck36\Simulation\CurrentCascade\BuildAndRun.ps1
```

工具装在其他位置时提供`-LccRoot`和`-ArmClang`。该脚本编译实际C、运行单元与31个场景，重建本机可执行程序及源哈希，不烧录。随后MATLAB执行：

```matlab
validate_input_cascade;
```

验证脚本会检查刚生成的本机程序SHA256；发布包不附仿真EXE，因此从干净包复现时，先运行上述PowerShell构建。已归档报告可直接阅读，无需商业工具。

若要重新进行较慢的参数搜索：

```matlab
design_input_cascade;
verify_input_cascade_linear;
export_input_cascade; % 会重写C系数头，不能在未重新验证时直接烧录
```

`Candidate`与`Firmware/Core`中保留的少量历史C仅是求解器依赖/对照，不是本版烧录入口。主入口为`FirmwareCascade`。结果目录保留最终31组CSV与拒绝候选报告，避免只展示通过结果。

## 模型不覆盖的因素

不含真实自举充放电、驱动失效、Coss、体二极管反向恢复、PCB寄生和磁芯损耗。内部5 µs步长与1 ms导出曲线不能认证纳秒级门极时序；模型IL峰值不是实物测量，也不是已实现的硬件峰值保护。

模型将Iin视为可观测的平均输入端口电流，实际固定相位采样是否满足该假设仍需核对。仿真通过不能排除虚焊、驱动异常或ADC相位偏差。

新双环并非所有动态都比旧电压单环快：同类执行增益0.9场景旧v4约14 ms，新双环约145 ms，端口观测模型也有差异；不能当作受控硬件A/B对比。

[完整机器可读报告](../CleanBuck36/Simulation/Results/input-cascade/matlab-report.json) · [最终场景汇总](../CleanBuck36/Simulation/Results/input-cascade/nonlinear/summary.csv)
