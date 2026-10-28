namespace FixedSerialMonitor;

// Pure policy shared with headless tests. No UI/API caller can bypass MCU checks.
internal sealed record ManualStartState(
    bool Connected, bool Durable, bool Fresh, bool CorrectFirmware,
    bool DeviceReady, bool DeviceFault, bool DeviceRunning, bool Pending,
    bool EmergencyRequested, float? Vin, float? Vout, float? Iin,
    float? Ntc0, float? Ntc1);

internal static class ManualStartPolicy
{
    public const string FirmwareId = "CLEANBUCK36-CASCADE-20261027-v7";

    public static bool CanStart(ManualStartState state, out string reason)
    {
        reason = state switch
        {
            { Connected: false } => "请先连接串口。",
            { Durable: false } => "日志不可写，不能启动。",
            { Fresh: false } => "等待新的设备状态和测量值。",
            { CorrectFirmware: false } => "请先烧录匹配的48V→36V双环v7固件。",
            { EmergencyRequested: true } => "急停已锁存；检查硬件并复位MCU后再试。",
            { DeviceFault: true } => "MCU故障锁存；检查硬件并复位MCU后再试。",
            { DeviceRunning: true } => "设备正在运行。",
            { Pending: true } => "启动结果尚未确认，不会重复发送。",
            { DeviceReady: false } => "设备未准备好或本轮已结束；检查台架后复位MCU再测。",
            _ when state.Vin is not (>= 45F and <= 52F) => "双环测试输入须在45～52V。",
            _ when state.Vout is not (>= 0F and < 1.5F) => "请先确认输出低于1.5V。",
            _ when state.Iin is not (>= 0F and < 200F) => "待机输入电流异常，不能启动。",
            _ when state.Ntc0 is not (>= -20F and < 45F) ||
                   state.Ntc1 is not (>= -20F and < 45F) => "温度未确认或不适合开始首测。",
            _ => "就绪：48V→36V双环v7，无运行时限；Iin参考4A/采样关断8A/OVP39V，本人启动。"
        };
        return reason.StartsWith("就绪：", System.StringComparison.Ordinal);
    }
}
