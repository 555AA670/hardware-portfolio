using FixedSerialMonitor;

var good = new ManualStartState(true, true, true, true, true, false, false,
                                false, false, 48F, 0F, 25F, 25F, 25F);
int cases = 0;
void Check(ManualStartState state, bool expected)
{
    bool actual = ManualStartPolicy.CanStart(state, out string reason);
    if (actual != expected || string.IsNullOrWhiteSpace(reason))
        throw new Exception($"Unexpected start policy: {state}; {reason}");
    cases++;
}
Check(good, true);
// Exhaustive boolean combinations for all command/lifecycle gates.
for (int bits = 0; bits < 512; bits++)
{
    bool B(int n) => (bits & (1 << n)) != 0;
    var state = good with { Connected=B(0), Durable=B(1), Fresh=B(2),
        CorrectFirmware=B(3), DeviceReady=B(4), DeviceFault=B(5),
        DeviceRunning=B(6), Pending=B(7), EmergencyRequested=B(8) };
    Check(state, B(0) && B(1) && B(2) && B(3) && B(4) &&
                 !B(5) && !B(6) && !B(7) && !B(8));
}
foreach (float? value in new float?[] { null, float.NaN, float.NegativeInfinity,
                                      float.PositiveInfinity, -1F, 0F, 44.99F, 52.01F, 20F })
    Check(good with { Vin=value }, false);
foreach (float value in new[] {45F, 48F, 52F}) Check(good with {Vin=value}, true);
foreach (float? value in new float?[] {null, float.NaN, -0.01F, 1.5F, 10F})
    Check(good with {Vout=value}, false);
foreach (float? value in new float?[] {null, float.NaN, -1F, 200F, 2000F})
    Check(good with {Iin=value}, false);
foreach (float? value in new float?[] {null, float.NaN, -21F, 45F, 100F})
{
    Check(good with {Ntc0=value}, false);
    Check(good with {Ntc1=value}, false);
}
Check(good with {Vout=1.499F, Iin=199F, Ntc0=44.9F}, true);
Check(good with {Pending=true}, false); // no double-click / uncertain-write retry
Check(good with {CorrectFirmware=false}, false); // cannot start a legacy image
Console.WriteLine($"{cases} manual-start policy cases passed; no serial commands or GUI started.");

// Exercise the compiled form/route without showing it, connecting serial, or
// sharing the operational API port. Port 0 requests a separate ephemeral port.
if (args.Length == 1)
{
    Exception? failure = null;
    int integrationCases = 0;
    var thread = new Thread(() =>
    {
        System.Windows.Forms.Form? form = null;
        Type? formType = null;
        try
        {
            var assembly = System.Reflection.Assembly.LoadFrom(Path.GetFullPath(args[0]));
            formType = assembly.GetType("FixedSerialMonitor.MonitorForm", throwOnError:true)!;
            form = (System.Windows.Forms.Form)Activator.CreateInstance(formType,
                System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic |
                System.Reflection.BindingFlags.Instance, null, new object[]{0}, null)!;
            object? Field(string name) => formType.GetField(name,
                System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)?.GetValue(form);
            void Assert(bool value, string message)
            {
                if (!value) throw new Exception(message);
                integrationCases++;
            }
            Assert(Field("_serial") is null, "form must not connect serial automatically");
            Assert(Field("_unlock") is null, "unlock button must be removed");
            Assert(Field("_start") is System.Windows.Forms.Button {Enabled:false}, "START disabled without device");
            Assert(Field("_start") is System.Windows.Forms.Button {Text:"启动48V→36V双环"}, "button identifies cascade experiment");
            Assert(form.Text.Contains("双环 v7") && form.Text.Contains("无运行时限"), "window identifies untimed cascade firmware");
            Assert(Field("_stop") is System.Windows.Forms.Button {Enabled:false}, "STOP disabled disconnected");
            var handler = formType.GetMethod("HandleApiRequest",
                System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!;
            string response = (string)handler.Invoke(form, new object[]{"POST", "/api/start"})!;
            using var json = System.Text.Json.JsonDocument.Parse(response);
            Assert(!json.RootElement.GetProperty("ok").GetBoolean(), "API must not start");
            Assert(json.RootElement.GetProperty("error").GetString() ==
                "manual_start_only_use_local_button", "API must demand the local button");
            string status = (string)handler.Invoke(form, new object[]{"GET", "/api/status"})!;
            using var statusJson = System.Text.Json.JsonDocument.Parse(status);
            Assert(statusJson.RootElement.GetProperty("manualStartOnly").GetBoolean(), "manual-only capability");
            Assert(!statusJson.RootElement.GetProperty("canManualStart").GetBoolean(), "disconnected cannot start");
            Assert(Field("_serial") is null, "API denial must not connect serial");
            formType.GetMethod("HumanStart", System.Reflection.BindingFlags.NonPublic |
                System.Reflection.BindingFlags.Instance)!.Invoke(form, null);
            Assert(Field("_serial") is null, "local button must obey disconnected guard");
        }
        catch (Exception error) { failure = error; }
        finally
        {
            if (formType is not null && form is not null)
            {
                foreach (string name in new[]{"_api", "_manualTimer", "_journal"})
                    (formType.GetField(name, System.Reflection.BindingFlags.Instance |
                     System.Reflection.BindingFlags.NonPublic)?.GetValue(form) as IDisposable)?.Dispose();
                form.Dispose();
            }
        }
    });
    thread.SetApartmentState(ApartmentState.STA);
    thread.Start();
    if (!thread.Join(10000)) throw new Exception("Headless form test timed out");
    if (failure is not null) throw new Exception("Headless compiled-form test failed", failure);
    Console.WriteLine($"{integrationCases} compiled-form/API checks passed; no window shown or serial connected.");
}
