using System.ComponentModel;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Microsoft.Win32.SafeHandles;

namespace FixedSerialMonitor;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new MonitorForm());
    }
}

internal sealed class MonitorForm : Form
{
    private readonly SerialJournal _journal = new();
    private readonly TextBox _port = new() { Text = "COM31", Width = 75 };
    private readonly ComboBox _baud = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 100 };
    private readonly Button _connect = new() { Text = "打开串口", AutoSize = true };
    private readonly Button _pause = new() { Text = "暂停波形", AutoSize = true, Enabled = false };
    private readonly Button _copy = new() { Text = "复制当前数据", AutoSize = true };
    private readonly Button _start = new() { Text = "启动48V→36V双环", AutoSize = true,
        BackColor = Color.LightGreen, Enabled = false };
    private readonly Button _stop = new() { Text = "停止", AutoSize = true, Enabled = false };
    private readonly Label _manualHint = new() { AutoSize = true, ForeColor = Color.DimGray };
    private readonly System.Windows.Forms.Timer _manualTimer = new() { Interval = 250 };
    private readonly Button _emergencyStop = new() { Text = "紧急停机", AutoSize = true,
        BackColor = Color.Firebrick, ForeColor = Color.White };
    private readonly Label _status = new() { Text = "未连接", AutoSize = true, ForeColor = Color.Firebrick, Padding = new Padding(12, 7, 0, 0) };
    private readonly Label _apiLabel = new() { Text = "API: 启动中", AutoSize = true, ForeColor = Color.DimGray, Padding = new Padding(12, 7, 0, 0) };

    private readonly TextBox _measurement = Field(76);
    private readonly TextBox _temperature = Field(76);
    private readonly TextBox _current = Field(54);
    private readonly TextBox _buck = Field(76);
    private readonly TextBox _hrtim = Field(76);
    private readonly TextBox _event = Field(76);

    private readonly ScopePanel _voltageScope = new(
        "电压波形", new[] { ("Vin", Color.Lime), ("Vout", Color.Gold) }, 0F, 60F);
    private readonly ScopePanel _currentScope = new(
        "电流波形", new[] { ("Iin", Color.DeepSkyBlue), ("Iout", Color.Orange) }, 0F, 2500F, "mA");
    private readonly ScopePanel _temperatureScope = new(
        "温度波形", new[] { ("NTC0", Color.HotPink), ("NTC1", Color.MediumPurple) }, null, null, "°C", 10F);

    private static readonly Regex TelemetryPattern = new(
        @"VREF=(?<vref>-?[0-9.]+)mV\s+Vin=(?<vin>-?[0-9.]+)V\s+Vout=(?<vout>-?[0-9.]+)V\s+Iin=(?<iin>-?[0-9.]+)mA\s+Iout=(?<iout>-?[0-9.]+)mA\s+NTC0=(?<ntc0>-?[0-9.]+)C\s+NTC1=(?<ntc1>-?[0-9.]+)C",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private readonly object _stateGate = new();
    private SerialConnection? _serial;
    private LocalApiServer? _api;
    private bool _displayPaused;
    private string _lastSnapshot = "";
    private float? _vin;
    private float? _vout;
    private float? _iin;
    private float? _iout;
    private float? _ntc0;
    private float? _ntc1;
    private double? _inputPowerW;
    private double? _outputPowerW;
    private double? _efficiencyPercent;
    private DateTime? _lastTelemetryAt;
    private volatile bool _startAttempted;
    private int _emergencyStopRequested;
    private long _emergencyStopRequestSequence;
    private bool _deviceStatusSeen;
    private bool _deviceProtocolReady;
    private bool _deviceFault;
    private bool _deviceStartUsed;
    private bool _deviceRunning;
    private DateTime _deviceStatusAt;
    private bool _manualFirmware, _manualReady, _manualStartPending;

    private const double PowerFilterAlpha = 0.20;
    private const double MinimumEfficiencyInputPowerW = 2.0;

    public MonitorForm(int apiPort = 18765)
    {
        Text = "G474 48V→36V 电压/平均输入电流双环 v7 无运行时限 120ns — 本人启动";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(980, 680);
        Size = new Size(1180, 800);
        Font = new Font("Microsoft YaHei UI", 10F);

        _baud.Items.AddRange(new object[] { "9600", "57600", "115200", "230400", "460800", "921600" });
        _baud.SelectedItem = "115200";

        var top = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = true,
            Padding = new Padding(8, 6, 8, 4)
        };
        top.Controls.AddRange(new Control[]
        {
            new Label { Text = "端口", AutoSize = true, Padding = new Padding(0, 7, 0, 0) }, _port,
            new Label { Text = "波特率", AutoSize = true, Padding = new Padding(10, 7, 0, 0) }, _baud,
            _connect, _pause, _copy, _start, _stop, _emergencyStop, _status, _apiLabel, _manualHint
        });

        var dashboard = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 3,
            Padding = new Padding(8)
        };
        dashboard.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        dashboard.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
        dashboard.RowStyles.Add(new RowStyle(SizeType.Percent, 33.33F));
        dashboard.RowStyles.Add(new RowStyle(SizeType.Percent, 33.33F));
        dashboard.RowStyles.Add(new RowStyle(SizeType.Percent, 33.34F));
        dashboard.Controls.Add(Group("电压/电流测量（最新）", _measurement), 0, 0);
        dashboard.Controls.Add(Group("温度传感器（NTC0 / NTC1）", _temperature), 1, 0);
        dashboard.Controls.Add(Group("输入电流限制（最新）", _current), 0, 1);
        dashboard.Controls.Add(Group("Buck 控制状态（最新）", _buck), 1, 1);
        dashboard.Controls.Add(Group("HRTIM 寄存器（最新）", _hrtim), 0, 2);
        dashboard.Controls.Add(Group("最新事件", _event), 1, 2);

        var scopeLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 3,
            Padding = new Padding(8)
        };
        scopeLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 34));
        scopeLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 33));
        scopeLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 33));
        scopeLayout.Controls.Add(_voltageScope, 0, 0);
        scopeLayout.Controls.Add(_currentScope, 0, 1);
        scopeLayout.Controls.Add(_temperatureScope, 0, 2);

        var tabs = new TabControl { Dock = DockStyle.Fill };
        var scopeTab = new TabPage("实时波形") { BackColor = Color.FromArgb(20, 22, 24) };
        var dashboardTab = new TabPage("固定数据") { BackColor = SystemColors.Control };
        scopeTab.Controls.Add(scopeLayout);
        dashboardTab.Controls.Add(dashboard);
        tabs.TabPages.Add(scopeTab);
        tabs.TabPages.Add(dashboardTab);

        var root = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2, ColumnCount = 1 };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 86));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.Controls.Add(top, 0, 0);
        root.Controls.Add(tabs, 0, 1);
        Controls.Add(root);

        _connect.Click += (_, _) => ToggleConnection();
        _pause.Click += (_, _) => TogglePause();
        _copy.Click += (_, _) => CopySnapshot();
        _start.Click += (_, _) => HumanStart();
        _stop.Click += (_, _) =>
        {
            string result = RequestStop();
            SetEvent("本人请求停止：" + result);
        };
        _manualTimer.Tick += (_, _) => RefreshManualControls();
        _manualTimer.Start();
        _emergencyStop.Click += (_, _) =>
        {
            string response = RequestEmergencyStop();
            using JsonDocument json = JsonDocument.Parse(response);
            bool sent = json.RootElement.TryGetProperty("commandSent", out JsonElement value) &&
                        value.GetBoolean();
            SetEvent(sent ? "急停命令已发出，等待 MCU 确认" :
                "急停请求已锁存，但串口未确认发送；请手动断开输入电源", true);
        };
        FormClosing += (_, _) =>
        {
            if (_serial is not null) RequestEmergencyStop();
            _manualTimer.Stop();
            _manualTimer.Dispose();
            _api?.Dispose();
            CloseSerial();
            _journal.Dispose();
        };

        try
        {
            _api = new LocalApiServer(apiPort, HandleApiRequest);
            _api.Start();
            _apiLabel.Text = $"API: 127.0.0.1:{apiPort}";
            _apiLabel.ForeColor = Color.SeaGreen;
        }
        catch (Exception ex)
        {
            _apiLabel.Text = "API启动失败";
            _apiLabel.ForeColor = Color.Firebrick;
            SetEvent("本机API启动失败：" + ex.Message, true);
        }
    }

    private static TextBox Field(int height) => new()
    {
        Dock = DockStyle.Fill,
        Multiline = true,
        ReadOnly = true,
        BorderStyle = BorderStyle.None,
        BackColor = Color.FromArgb(248, 249, 250),
        Font = new Font("Consolas", 11F),
        ScrollBars = ScrollBars.None,
        ShortcutsEnabled = true,
        Height = height,
        WordWrap = true
    };

    private static GroupBox Group(string title, Control child)
    {
        var box = new GroupBox { Text = title, Dock = DockStyle.Fill, Padding = new Padding(10) };
        box.Controls.Add(child);
        return box;
    }

    private void ToggleConnection()
    {
        if (_serial is not null)
        {
            CloseSerial();
            return;
        }

        try
        {
            string port = _port.Text.Trim().ToUpperInvariant();
            int baud = int.Parse(_baud.Text);
            _serial = new SerialConnection(port, baud, OnSerialLine, OnSerialError);
            _serial.Open();
            _connect.Text = "关闭串口";
            _port.Enabled = false;
            _baud.Enabled = false;
            _pause.Enabled = true;
            SetStatus($"已连接 {port} / {baud}", Color.SeaGreen);
            SetEvent("串口已打开，等待数据……");
        }
        catch (Exception ex)
        {
            _serial?.Dispose();
            _serial = null;
            SetStatus("打开失败", Color.Firebrick);
            SetEvent("无法打开串口：" + ex.Message + $"\r\n请先关闭其他占用 {_port.Text.Trim()} 的串口助手。", true);
        }
    }

    private void CloseSerial()
    {
        _serial?.Dispose();
        _serial = null;
        _deviceStatusSeen = false;
        _deviceProtocolReady = false;
        _manualFirmware = false;
        _manualReady = false;
        _manualStartPending = false;
        _connect.Text = "打开串口";
        _port.Enabled = true;
        _baud.Enabled = true;
        _pause.Enabled = false;
        _displayPaused = false;
        _pause.Text = "暂停波形";
        SetStatus("未连接", Color.Firebrick);
    }

    private void TogglePause()
    {
        _displayPaused = !_displayPaused;
        _pause.Text = _displayPaused ? "继续波形" : "暂停波形";
        SetStatus(_displayPaused ? "波形已暂停（仍在接收）" : $"已连接 {_port.Text} / {_baud.Text}",
                  _displayPaused ? Color.DarkOrange : Color.SeaGreen);
    }

    private bool CanManualStart(out string reason) => ManualStartPolicy.CanStart(new(
        _serial is not null, _journal.Durable,
        _deviceStatusSeen && _deviceProtocolReady &&
        (DateTime.UtcNow - _deviceStatusAt).TotalSeconds <= 2 &&
        _lastTelemetryAt.HasValue && (DateTime.Now - _lastTelemetryAt.Value).TotalSeconds <= 2,
        _manualFirmware, _manualReady, _deviceFault, _deviceRunning,
        _manualStartPending, Volatile.Read(ref _emergencyStopRequested) != 0,
        _vin, _vout, _iin, _ntc0, _ntc1), out reason);

    private void RefreshManualControls()
    {
        _start.Enabled = CanManualStart(out string reason);
        _stop.Enabled = _serial is not null;
        _manualHint.Text = reason;
    }

    // Only the local button calls this method. There is no API route to it.
    private void HumanStart()
    {
        lock (_stateGate)
        {
            if (!CanManualStart(out string reason)) { SetEvent(reason, true); return; }
            if (MessageBox.Show(this,
                "确认输入48V（允许45～52V），负载接法和电源限流已检查，你在台架旁可立即断电。\n双环v7：电压外环10kHz、平均输入电流内环50kHz；200kHz/120.59ns，右桥Q3/Q4关闭，参考20V/s，空载迟滞补能。\n输入平均电流参考上限4A，原始ADC采样关断8A，OVP39V；不是电感峰值或逐周期保护。无运行时限，持续运行至本人停止或保护触发。停止/故障后需检查并复位才能再测。\n仅本人点击是后发送一次START，不会自动重试。",
                "48V→36V双环v7持续运行", MessageBoxButtons.YesNo, MessageBoxIcon.Warning,
                MessageBoxDefaultButton.Button2) != DialogResult.Yes) return;
            // Recheck freshness and fault status after the human confirmation.
            if (!CanManualStart(out reason)) { SetEvent(reason, true); return; }
            _manualStartPending = true; // Set before write; never retry an uncertain START.
            _startAttempted = true;
            _journal.Append("HOST HUMAN START REQUEST");
            if (!_journal.Durable) { RefreshManualControls(); return; }
            try
            {
                if (Volatile.Read(ref _emergencyStopRequested) != 0)
                {
                    _manualStartPending = false;
                    SetEvent("急停请求已到达，取消启动。", true);
                    RefreshManualControls();
                    return;
                }
                _serial!.WriteLine("START");
                _journal.Append("HOST HUMAN CMD START SENT");
                SetEvent("本人启动命令已发送，等待MCU确认；不会自动重试。");
            }
            catch (Exception ex)
            {
                _journal.Append("HOST HUMAN START WRITE UNCERTAIN: " + ex.Message);
                SetEvent("发送结果不确定，请停止并检查连接，不会重试。", true);
            }
            RefreshManualControls();
        }
    }

    private void OnSerialLine(string line)
    {
        // Persist before scheduling UI work, including while charts are paused.
        _journal.Append(line);
        if (IsDisposed || !IsHandleCreated) return;
        try { BeginInvoke(() => UpdateLine(line)); }
        catch (InvalidOperationException) { }
    }

    private void OnSerialError(string error)
    {
        if (IsDisposed) return;
        BeginInvoke(() =>
        {
            _journal.Append("HOST SERIAL ERROR: " + error);
            SetEvent("串口读取错误：" + error, true);
            CloseSerial();
        });
    }

    private void UpdateLine(string line)
    {
        if (string.IsNullOrWhiteSpace(line)) return;
        string s = line.Trim();
        if (s.StartsWith("TEST fw=" + ManualStartPolicy.FirmwareId, StringComparison.Ordinal) &&
            s.Contains("BOOT OFF", StringComparison.Ordinal))
        {
            _manualStartPending = false;
            Interlocked.Exchange(ref _emergencyStopRequested, 0);
        }
        if (s.StartsWith("TEST DONE fw=" + ManualStartPolicy.FirmwareId, StringComparison.Ordinal) ||
            s.StartsWith("CMD START REJECT", StringComparison.Ordinal) ||
            s.StartsWith("CMD ESTOP LATCHED", StringComparison.Ordinal))
            _manualStartPending = false;
        if (s.StartsWith("BUCK run=", StringComparison.Ordinal))
        {
            _deviceStatusSeen = true;
            _deviceProtocolReady = s.Contains(" startUsed=", StringComparison.Ordinal);
            _deviceStatusAt = DateTime.UtcNow;
            _deviceRunning = ReadFlag(s, "run");
            _deviceFault = ReadFlag(s, "fault");
            _deviceStartUsed = ReadFlag(s, "startUsed");
            _manualFirmware = s.Contains("fw=" + ManualStartPolicy.FirmwareId + " ", StringComparison.Ordinal) &&
                              ReadFlag(s, "manualStart");
            _manualReady = ReadFlag(s, "ready");
        }

        if (s.StartsWith("cnt="))
        {
            // Raw ADC is deliberately folded into the fixed measurement box.
            _measurement.Tag = s;
        }
        else if (s.StartsWith("VREF="))
        {
            UpdateTelemetry(s);
        }
        else if (s.StartsWith("IIN filtered:") || s.StartsWith("CASCADE "))
            _current.Text = s;
        else if (s.StartsWith("BUCK run="))
            _buck.Text = s;
        else if (s.StartsWith("HRTIM A["))
            _hrtim.Text = s;
        else if (s.StartsWith("BUCK ", StringComparison.OrdinalIgnoreCase) ||
                 s.Contains("FAULT", StringComparison.OrdinalIgnoreCase) ||
                 s.Contains("ERROR", StringComparison.OrdinalIgnoreCase) ||
                 s.Contains("boot:", StringComparison.OrdinalIgnoreCase))
            SetEvent(DateTime.Now.ToString("HH:mm:ss") + "  " + s,
                     s.Contains("FAULT", StringComparison.OrdinalIgnoreCase) ||
                     s.Contains("ERROR", StringComparison.OrdinalIgnoreCase));

        _lastSnapshot = BuildSnapshot();
        RefreshManualControls();
    }

    private void UpdateTelemetry(string line)
    {
        Match match = TelemetryPattern.Match(line);
        if (!match.Success)
        {
            _measurement.Text = line;
            return;
        }

        float vin = Parse(match, "vin");
        float vout = Parse(match, "vout");
        float iin = Parse(match, "iin");
        float iout = Parse(match, "iout");
        float ntc0 = Parse(match, "ntc0");
        float ntc1 = Parse(match, "ntc1");

        _vin = vin;
        _vout = vout;
        _iin = iin;
        _iout = iout;
        _ntc0 = ntc0;
        _ntc1 = ntc1;
        _lastTelemetryAt = DateTime.Now;

        // Average the products, rather than multiplying long-term averaged
        // voltage/current values.  This remains meaningful when the load moves.
        double inputPowerW = Math.Max(0.0, vin) * Math.Max(0.0, iin) / 1000.0;
        double outputPowerW = Math.Max(0.0, vout) * Math.Max(0.0, iout) / 1000.0;
        _inputPowerW = LowPass(_inputPowerW, inputPowerW);
        _outputPowerW = LowPass(_outputPowerW, outputPowerW);
        _efficiencyPercent = _inputPowerW >= MinimumEfficiencyInputPowerW
            ? 100.0 * _outputPowerW / _inputPowerW
            : null;

        string efficiencyText = _efficiencyPercent.HasValue
            ? $"{_efficiencyPercent.Value,6:0.0} %"
            : "   --  (Pin<2W)";

        string raw = _measurement.Tag as string ?? "";
        _measurement.Text =
            $"Vin   {vin,7:0.00} V       Vout  {vout,7:0.00} V\r\n" +
            $"Iin   {iin,7:0.00} mA      Iout  {iout,7:0.00} mA\r\n" +
            $"Pin   {_inputPowerW.Value,7:0.000} W       Pout  {_outputPowerW.Value,7:0.000} W   效率 {efficiencyText}\r\n" + raw;
        _temperature.Text =
            $"NTC0  {ntc0,7:0.0} °C\r\n" +
            $"NTC1  {ntc1,7:0.0} °C\r\n" +
            $"温差  {Math.Abs(ntc0 - ntc1),7:0.0} °C";

        if (!_displayPaused)
        {
            _voltageScope.AddSample(vin, vout);
            _currentScope.AddSample(iin, iout);
            _temperatureScope.AddSample(ntc0, ntc1);
        }
    }

    private static bool ReadFlag(string line, string key) =>
        Regex.IsMatch(line, $@"(?:^|\s){Regex.Escape(key)}=1(?:\s|$)",
            RegexOptions.CultureInvariant);

    private static float Parse(Match match, string group) =>
        float.Parse(match.Groups[group].Value, System.Globalization.CultureInfo.InvariantCulture);

    private static double LowPass(double? previous, double sample) =>
        previous.HasValue
            ? previous.Value + PowerFilterAlpha * (sample - previous.Value)
            : sample;

    private string HandleApiRequest(string method, string path)
    {
        // Journal reads do not wait for WinForms message processing.
        string readRoute = path.Split('?', 2)[0].TrimEnd('/').ToLowerInvariant();
        if (method == "GET" && readRoute == "/api/logs")
        {
            long after = QueryLong(path, "after", 0);
            int limit = (int)Math.Clamp(QueryLong(path, "limit", 200), 1, 1000);
            return _journal.ReadAfter(after, limit);
        }
        if (method == "GET" && readRoute == "/api/fault")
            return _journal.FaultWindow();
        if (method == "POST" && readRoute == "/api/emergency-stop")
            return RequestEmergencyStop();
        if (method == "POST" && readRoute == "/api/start")
            return ApiError("manual_start_only_use_local_button");
        if (InvokeRequired)
            return (string)Invoke(() => HandleApiRequest(method, path));

        string route = path.Split('?', 2)[0].TrimEnd('/').ToLowerInvariant();
        if (route.Length == 0) route = "/";

        if (method == "POST" && route == "/api/stop")
            return RequestStop();
        if (method == "POST" && route == "/api/connect")
        {
            string? requestedPort = path.Split('?', 2).Skip(1).FirstOrDefault()?
                .Split('&', StringSplitOptions.RemoveEmptyEntries)
                .Select(item => item.Split('=', 2))
                .Where(parts => parts.Length == 2 && parts[0].Equals("port", StringComparison.OrdinalIgnoreCase))
                .Select(parts => Uri.UnescapeDataString(parts[1]).Trim().ToUpperInvariant())
                .FirstOrDefault();
            if (_serial is null && !string.IsNullOrWhiteSpace(requestedPort))
                _port.Text = requestedPort;
            if (_serial is null) ToggleConnection();
            return BuildApiStatusJson();
        }
        if (method == "POST" && route == "/api/disconnect")
        {
            if (_serial is not null) CloseSerial();
            return BuildApiStatusJson();
        }
        if (method == "POST" && route == "/api/pause")
        {
            if (!_displayPaused) TogglePause();
            return BuildApiStatusJson();
        }
        if (method == "POST" && route == "/api/resume")
        {
            if (_displayPaused) TogglePause();
            return BuildApiStatusJson();
        }
        if (method == "GET" && (route == "/api/status" || route == "/api/latest"))
            return BuildApiStatusJson();
        if (method == "GET" && (route == "/" || route == "/api"))
        {
            return JsonSerializer.Serialize(new
            {
                ok = true,
                name = "G474 fixed serial monitor",
                apiVersion = 1,
                endpoints = new[]
                {
                    "GET /api/status", "GET /api/latest", "POST /api/connect",
                    "POST /api/disconnect", "POST /api/pause", "POST /api/resume",
                    "POST /api/stop", "POST /api/emergency-stop",
                    "GET /api/logs", "GET /api/fault"
                }
            });
        }
        return JsonSerializer.Serialize(new { ok = false, error = "Unknown endpoint", method, path });
    }

    private string RequestEmergencyStop()
    {
        // Mark the request before taking the state lock so START sees it immediately.
        Interlocked.Exchange(ref _emergencyStopRequested, 1);
        lock (_stateGate)
        {
            // The request flag is set before locking so a concurrent button cannot start.
            Interlocked.Exchange(ref _emergencyStopRequested, 1);
            _startAttempted = true;
            JournalEntry request = _journal.Append("HOST ESTOP REQUEST");
            Interlocked.Exchange(ref _emergencyStopRequestSequence, request.Sequence);
            SerialConnection? serial = Volatile.Read(ref _serial);
            if (serial is null)
                return JsonSerializer.Serialize(new { ok = false, commandSent = false,
                    confirmed = false, error = "serial_disconnected",
                    hardwareCutoffAvailable = false });
            try
            {
                serial.WriteLine("ESTOP");
                _journal.Append("HOST CMD ESTOP SENT");
                return JsonSerializer.Serialize(new { ok = true, commandSent = true,
                    confirmed = false, acknowledgement = "poll /api/status or /api/logs",
                    hardwareCutoffAvailable = false });
            }
            catch (Exception ex)
            {
                _journal.Append("HOST CMD ESTOP WRITE ERROR: " + ex.Message);
                return JsonSerializer.Serialize(new { ok = false, commandSent = false,
                    confirmed = false, error = "serial_write_uncertain",
                    hardwareCutoffAvailable = false });
            }
        }
    }

    private static string ApiError(string reason) =>
        JsonSerializer.Serialize(new { ok = false, error = reason });

    private string RequestStop()
    {
        lock (_stateGate)
        {
            if (_serial is null) return ApiError("serial_disconnected");
            try
            {
                _serial.WriteLine("STOP");
                _journal.Append("HOST CMD STOP SENT");
                return JsonSerializer.Serialize(new { ok = true, stopRequested = true });
            }
            catch (Exception ex)
            {
                _journal.Append("HOST CMD STOP WRITE ERROR: " + ex.Message);
                return ApiError("serial_write_uncertain");
            }
        }
    }

    private static long QueryLong(string path, string key, long fallback)
    {
        string? query = path.Split('?', 2).Skip(1).FirstOrDefault();
        if (query is null) return fallback;
        foreach (string part in query.Split('&', StringSplitOptions.RemoveEmptyEntries))
        {
            string[] kv = part.Split('=', 2);
            if (kv.Length == 2 && kv[0].Equals(key, StringComparison.OrdinalIgnoreCase) &&
                long.TryParse(kv[1], out long result))
                return result;
        }
        return fallback;
    }

    private string BuildApiStatusJson()
    {
        return JsonSerializer.Serialize(new
        {
            ok = true,
            apiVersion = 1,
            connected = _serial is not null,
            port = _port.Text.Trim().ToUpperInvariant(),
            baud = int.TryParse(_baud.Text, out int parsedBaud) ? parsedBaud : 0,
            displayPaused = _displayPaused,
            startAttempted = _startAttempted,
            emergencyStopRequested = Volatile.Read(ref _emergencyStopRequested) != 0,
            emergencyStopConfirmed = Volatile.Read(ref _emergencyStopRequested) != 0 &&
                _journal.EmergencyAckSequence is long ack &&
                ack > Volatile.Read(ref _emergencyStopRequestSequence),
            emergencyStopAckSequence = _journal.EmergencyAckSequence,
            hardwareCutoffAvailable = false,
            startLocked = _manualStartPending || _deviceRunning || _deviceFault ||
                          Volatile.Read(ref _emergencyStopRequested) != 0,
            manualStartOnly = true,
            canManualStart = CanManualStart(out _),
            deviceProtocolReady = _deviceProtocolReady,
            deviceStatusFresh = _deviceStatusSeen &&
                (DateTime.UtcNow - _deviceStatusAt).TotalSeconds <= 2,
            deviceFault = _deviceFault,
            deviceStartUsed = _deviceStartUsed,
            deviceRunning = _deviceRunning,
            latestLogSequence = _journal.LatestSequence,
            faultSequence = _journal.FaultSequence,
            logDurable = _journal.Durable,
            logPath = _journal.FilePath,
            logError = _journal.WriteError,
            lastUpdate = _lastTelemetryAt?.ToString("O"),
            telemetry = new
            {
                vinV = _vin,
                voutV = _vout,
                iinMa = _iin,
                ioutMa = _iout,
                inputPowerW = _inputPowerW,
                outputPowerW = _outputPowerW,
                efficiencyPercent = _efficiencyPercent,
                efficiencyValid = _efficiencyPercent.HasValue,
                ntc0C = _ntc0,
                ntc1C = _ntc1,
                ntcDeltaC = (_ntc0.HasValue && _ntc1.HasValue)
                    ? Math.Abs(_ntc0.Value - _ntc1.Value) : (float?)null
            },
            rawAdc = _measurement.Tag as string,
            currentLimit = _current.Text,
            buck = _buck.Text,
            hrtim = _hrtim.Text,
            latestEvent = _event.Text
        });
    }

    private void SetEvent(string text, bool alarm = false)
    {
        _event.ForeColor = alarm ? Color.Firebrick : Color.Black;
        _event.Text = text;
    }

    private void SetStatus(string text, Color color)
    {
        _status.Text = text;
        _status.ForeColor = color;
    }

    private string BuildSnapshot() =>
        $"测量值\r\n{_measurement.Text}\r\n\r\n温度\r\n{_temperature.Text}\r\n\r\n" +
        $"电流限制\r\n{_current.Text}\r\n\r\nBuck\r\n{_buck.Text}\r\n\r\n" +
        $"HRTIM\r\n{_hrtim.Text}\r\n\r\n事件\r\n{_event.Text}";

    private void CopySnapshot()
    {
        string text = string.IsNullOrWhiteSpace(_lastSnapshot) ? BuildSnapshot() : _lastSnapshot;
        try
        {
            Clipboard.SetText(text);
            SetStatus("当前数据已复制", Color.RoyalBlue);
        }
        catch (Exception ex)
        {
            SetEvent("复制失败：" + ex.Message, true);
        }
    }
}

internal sealed class LocalApiServer : IDisposable
{
    private readonly TcpListener _listener;
    private readonly Func<string, string, string> _handler;
    private readonly CancellationTokenSource _cancel = new();
    private Task? _serverTask;

    public LocalApiServer(int port, Func<string, string, string> handler)
    {
        _listener = new TcpListener(IPAddress.Loopback, port);
        _handler = handler;
    }

    public void Start()
    {
        _listener.Start(8);
        _serverTask = Task.Run(() => AcceptLoop(_cancel.Token));
    }

    private async Task AcceptLoop(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            try
            {
                TcpClient client = await _listener.AcceptTcpClientAsync(token);
                _ = Task.Run(() => HandleClient(client, token), token);
            }
            catch (OperationCanceledException) { return; }
            catch (ObjectDisposedException) { return; }
        }
    }

    private async Task HandleClient(TcpClient client, CancellationToken token)
    {
        using (client)
        using (NetworkStream stream = client.GetStream())
        using (var reader = new StreamReader(stream, Encoding.ASCII, false, 2048, leaveOpen: true))
        {
            try
            {
                string? requestLine = await reader.ReadLineAsync();
                if (string.IsNullOrWhiteSpace(requestLine)) return;
                string[] parts = requestLine.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                string method = parts.Length > 0 ? parts[0].ToUpperInvariant() : "GET";
                string path = parts.Length > 1 ? parts[1] : "/";

                string? header;
                do { header = await reader.ReadLineAsync(); }
                while (!string.IsNullOrEmpty(header) && !token.IsCancellationRequested);

                string json = _handler(method, path);
                byte[] body = Encoding.UTF8.GetBytes(json);
                string headers =
                    "HTTP/1.1 200 OK\r\n" +
                    "Content-Type: application/json; charset=utf-8\r\n" +
                    "Cache-Control: no-store\r\n" +
                    "Access-Control-Allow-Origin: http://127.0.0.1\r\n" +
                    $"Content-Length: {body.Length}\r\n" +
                    "Connection: close\r\n\r\n";
                byte[] headerBytes = Encoding.ASCII.GetBytes(headers);
                await stream.WriteAsync(headerBytes, token);
                await stream.WriteAsync(body, token);
            }
            catch (IOException) { }
            catch (OperationCanceledException) { }
        }
    }

    public void Dispose()
    {
        _cancel.Cancel();
        _listener.Stop();
        try { _serverTask?.Wait(300); } catch { }
        _cancel.Dispose();
    }
}

internal sealed class ScopePanel : Control
{
    private const int Capacity = 300;
    private readonly (string Name, Color Color)[] _channels;
    private readonly Queue<float>[] _samples;
    private readonly float? _fixedMin;
    private readonly float? _fixedMax;
    private readonly float _minimumSpan;
    private readonly string _unit;
    private readonly string _title;

    public ScopePanel(string title, (string Name, Color Color)[] channels,
                      float? fixedMin, float? fixedMax, string unit = "V", float minimumSpan = 1F)
    {
        _title = title;
        _channels = channels;
        _fixedMin = fixedMin;
        _fixedMax = fixedMax;
        _unit = unit;
        _minimumSpan = minimumSpan;
        _samples = channels.Select(_ => new Queue<float>(Capacity)).ToArray();
        Dock = DockStyle.Fill;
        BackColor = Color.FromArgb(12, 16, 14);
        ForeColor = Color.Gainsboro;
        DoubleBuffered = true;
        SetStyle(ControlStyles.ResizeRedraw, true);
    }

    public void AddSample(params float[] values)
    {
        for (int i = 0; i < _samples.Length && i < values.Length; i++)
        {
            if (_samples[i].Count >= Capacity) _samples[i].Dequeue();
            _samples[i].Enqueue(values[i]);
        }
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        Graphics g = e.Graphics;
        g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        g.Clear(BackColor);

        Rectangle plot = new(62, 28, Math.Max(10, Width - 76), Math.Max(10, Height - 48));
        using var gridPen = new Pen(Color.FromArgb(45, 60, 52), 1F);
        using var axisPen = new Pen(Color.FromArgb(90, 120, 103), 1F);
        using var textBrush = new SolidBrush(ForeColor);
        using var smallFont = new Font("Consolas", 8.5F);
        using var titleFont = new Font("Microsoft YaHei UI", 10F, FontStyle.Bold);

        g.DrawRectangle(axisPen, plot);
        for (int i = 1; i < 10; i++)
        {
            int x = plot.Left + plot.Width * i / 10;
            g.DrawLine(gridPen, x, plot.Top, x, plot.Bottom);
        }
        for (int i = 1; i < 8; i++)
        {
            int y = plot.Top + plot.Height * i / 8;
            g.DrawLine(gridPen, plot.Left, y, plot.Right, y);
        }

        (float min, float max) = GetRange();
        g.DrawString(_title, titleFont, textBrush, 8, 4);
        g.DrawString($"{max:0.##}{_unit}", smallFont, textBrush, 3, plot.Top - 6);
        g.DrawString($"{min:0.##}{_unit}", smallFont, textBrush, 3, plot.Bottom - 12);

        int legendX = 130;
        for (int channel = 0; channel < _channels.Length; channel++)
        {
            float[] data = _samples[channel].ToArray();
            using var tracePen = new Pen(_channels[channel].Color, 2F);
            using var traceBrush = new SolidBrush(_channels[channel].Color);
            string latest = data.Length == 0 ? "--" : $"{data[^1]:0.##}{_unit}";
            g.DrawString($"{_channels[channel].Name}: {latest}", smallFont, traceBrush, legendX, 6);
            legendX += 145;
            if (data.Length < 2) continue;

            var points = new PointF[data.Length];
            for (int i = 0; i < data.Length; i++)
            {
                float x = plot.Left + (plot.Width * i / (float)(Capacity - 1));
                float normalized = Math.Clamp((data[i] - min) / (max - min), 0F, 1F);
                float y = plot.Bottom - normalized * plot.Height;
                points[i] = new PointF(x, y);
            }
            g.DrawLines(tracePen, points);
        }

        g.DrawString($"最近 {Capacity / 2} 秒（固件每 500 ms 一点）", smallFont,
                     Brushes.DimGray, plot.Right - 225, plot.Bottom + 3);
    }

    private (float Min, float Max) GetRange()
    {
        if (_fixedMin.HasValue && _fixedMax.HasValue)
            return (_fixedMin.Value, _fixedMax.Value);

        IEnumerable<float> all = _samples.SelectMany(q => q);
        if (!all.Any()) return (0F, _minimumSpan);
        float min = all.Min();
        float max = all.Max();
        float span = Math.Max(_minimumSpan, max - min);
        float center = (min + max) * 0.5F;
        return (center - span * 0.6F, center + span * 0.6F);
    }
}

internal sealed class SerialConnection : IDisposable
{
    private const uint GenericRead = 0x80000000;
    private const uint GenericWrite = 0x40000000;
    private const uint OpenExisting = 3;
    private const uint PurgeRxClear = 0x0008;
    private const uint PurgeTxClear = 0x0004;

    private readonly object _writeGate = new();
    private readonly string _port;
    private readonly int _baud;
    private readonly Action<string> _onLine;
    private readonly Action<string> _onError;
    private SafeFileHandle? _handle;
    private CancellationTokenSource? _cancel;
    private Task? _reader;

    public SerialConnection(string port, int baud, Action<string> onLine, Action<string> onError)
    {
        _port = port;
        _baud = baud;
        _onLine = onLine;
        _onError = onError;
    }

    public void Open()
    {
        _handle = CreateFileW(@"\\.\" + _port, GenericRead | GenericWrite, 0, IntPtr.Zero,
                              OpenExisting, 0, IntPtr.Zero);
        if (_handle.IsInvalid)
            throw new Win32Exception(Marshal.GetLastWin32Error());

        var dcb = new Dcb { DCBlength = (uint)Marshal.SizeOf<Dcb>() };
        if (!BuildCommDCBW($"baud={_baud} parity=n data=8 stop=1", ref dcb))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        dcb.Flags |= 1U; // fBinary
        if (!SetCommState(_handle, ref dcb))
            throw new Win32Exception(Marshal.GetLastWin32Error());

        var timeouts = new CommTimeouts
        {
            ReadIntervalTimeout = 20,
            ReadTotalTimeoutMultiplier = 0,
            ReadTotalTimeoutConstant = 50,
            WriteTotalTimeoutConstant = 200
        };
        if (!SetCommTimeouts(_handle, ref timeouts))
            throw new Win32Exception(Marshal.GetLastWin32Error());

        SetupComm(_handle, 16384, 1024);
        PurgeComm(_handle, PurgeRxClear | PurgeTxClear);
        _cancel = new CancellationTokenSource();
        _reader = Task.Run(() => ReaderLoop(_cancel.Token));
    }

    public void WriteLine(string command)
    {
        if (command is not ("START" or "STOP" or "ESTOP"))
            throw new ArgumentException("Unsupported serial command", nameof(command));
        lock (_writeGate)
        {
            if (_handle is null || _handle.IsInvalid || _handle.IsClosed)
                throw new InvalidOperationException("Serial port is not open");
            byte[] bytes = Encoding.ASCII.GetBytes(command + "\n");
            if (!WriteFile(_handle, bytes, bytes.Length, out uint written, IntPtr.Zero) ||
                written != bytes.Length)
                throw new Win32Exception(Marshal.GetLastWin32Error());
        }
    }

    private void ReaderLoop(CancellationToken token)
    {
        byte[] bytes = new byte[2048];
        var pending = new StringBuilder();
        try
        {
            while (!token.IsCancellationRequested && _handle is { IsInvalid: false, IsClosed: false })
            {
                if (!ReadFile(_handle, bytes, bytes.Length, out uint count, IntPtr.Zero))
                {
                    int error = Marshal.GetLastWin32Error();
                    if (!token.IsCancellationRequested && error != 995)
                        _onError(new Win32Exception(error).Message);
                    return;
                }
                if (count == 0) continue;

                pending.Append(Encoding.ASCII.GetString(bytes, 0, (int)count));
                while (true)
                {
                    string current = pending.ToString();
                    int end = current.IndexOf('\n');
                    if (end < 0) break;
                    string line = current[..end].TrimEnd('\r');
                    pending.Remove(0, end + 1);
                    _onLine(line);
                }
                if (pending.Length > 8192)
                {
                    pending.Clear();
                    _onLine("HOST RX OVERFLOW: unterminated serial line discarded");
                }
            }
        }
        catch (Exception ex)
        {
            if (!token.IsCancellationRequested) _onError(ex.Message);
        }
    }

    public void Dispose()
    {
        _cancel?.Cancel();
        if (_handle is { IsInvalid: false, IsClosed: false }) CancelIoEx(_handle, IntPtr.Zero);
        try { _reader?.Wait(300); } catch { }
        _handle?.Dispose();
        _cancel?.Dispose();
        _handle = null;
        _cancel = null;
        _reader = null;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Dcb
    {
        public uint DCBlength;
        public uint BaudRate;
        public uint Flags;
        public ushort wReserved;
        public ushort XonLim;
        public ushort XoffLim;
        public byte ByteSize;
        public byte Parity;
        public byte StopBits;
        public byte XonChar;
        public byte XoffChar;
        public byte ErrorChar;
        public byte EofChar;
        public byte EvtChar;
        public ushort wReserved1;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct CommTimeouts
    {
        public uint ReadIntervalTimeout;
        public uint ReadTotalTimeoutMultiplier;
        public uint ReadTotalTimeoutConstant;
        public uint WriteTotalTimeoutMultiplier;
        public uint WriteTotalTimeoutConstant;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFileW(string name, uint access, uint share,
        IntPtr security, uint creation, uint flags, IntPtr template);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool BuildCommDCBW(string definition, ref Dcb dcb);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetCommState(SafeFileHandle handle, ref Dcb dcb);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetCommTimeouts(SafeFileHandle handle, ref CommTimeouts timeouts);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetupComm(SafeFileHandle handle, uint inputSize, uint outputSize);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool PurgeComm(SafeFileHandle handle, uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool ReadFile(SafeFileHandle handle, byte[] buffer, int count,
        out uint read, IntPtr overlapped);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool WriteFile(SafeFileHandle handle, byte[] buffer, int count,
        out uint written, IntPtr overlapped);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CancelIoEx(SafeFileHandle handle, IntPtr overlapped);
}
