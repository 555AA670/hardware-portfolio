# 实际C控制器与仿真

这里的`cascade_control.c/.h`和`cascade_coefficients.h`由v7 MCU工程直接链接，模型也执行同一份C。

`BuildAndRun.ps1`只执行离线编译与仿真，不包含烧录或串口命令。支持`-LccRoot`、`-ArmClang`以及`-OutputDirectory`，默认工具位置沿用原开发环境。复现步骤及模型边界见[仿真说明](../../../docs/simulation.md)。

`../Candidate`的求解器与`../../Firmware`的历史控制模块是原生测试依赖，不能删掉后仍声称可复现。最终数值结果在`../Results/input-cascade`；保留原始`hardwareValidated=false`标签，不把离线模型结论改成硬件认证。

当前固件已由独立项目接入MCU，短时带载结果另见[实测记录](../../../docs/test-results.md)。
