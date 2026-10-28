"""Build linkage and fail-closed integration checks, no hardware access."""
import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parent
build=root/'MDK-ARM/Build/CLEANBUCK36_BENCH'
manifest=json.loads((build/'manifest.json').read_text(encoding='utf-8-sig'))
count=0
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def check(ok,message):
 global count
 assert ok,message
 count+=1
def function(text,name):
 a=text.index(name);b=text.index('{',a);end=b+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[a:end]
check(sha(build/Path(manifest['Hex']).name)==manifest['SHA256'],'Fresh HEX hash')
for row in manifest['Sources']:
 check(sha(root/'MDK-ARM'/row['File'])==row['SHA256'],row['File'])
sim=root.parent/'Simulation/Results/input-cascade'
result=json.loads((sim/'matlab-report.json').read_text())
check(result['allAlgorithmModelChecksPass'],'Final model report passed')
native=json.loads((sim/'nonlinear/source-manifest.json').read_text(encoding='utf-8-sig'))
for row in native['Sources']:
 if row['File'] in ['cascade_control.c','cascade_control.h','cascade_coefficients.h']:
  check(sha(root.parent/'Simulation/CurrentCascade'/row['File'])==row['SHA256'],'Exact simulated '+row['File'])
check(sha(root/'Core/Src/board.c')==sha(root.parent/'Firmware/Core/Src/board.c'),'Original hardware setup unchanged')
main=(root/'Core/Src/main.c').read_text(encoding='utf-8');cfg=(root/'Core/Inc/bench_config.h').read_text(encoding='utf-8')
adc=function(main,'void HAL_ADCEx_InjectedConvCpltCallback(')
irq=function(main,'void HRTIM1_TIMB_IRQHandler(')
start=function(main,'static void Start(')
stop=function(main,'static void StopLocked(')
check(adc.index('Stop(F_OC)')<adc.index('request=Cascade_Sample'),'Raw current trip before loop')
check(adc.index('Stop(F_OV)')<adc.index('request=Cascade_Sample'),'Raw voltage trip before loop')
check('CMP1xR' not in adc,'ADC never writes runtime PWM compare')
check('if(cb_running&&!cb_fault)' in adc,'Fault check before request publication')
check(irq.index('if(cb_running&&!cb_fault)')<irq.index('Cascade_Pulse'),'PWM generation gated')
check('Cascade_Output(&control,request)' in adc,'Same final execution brake at50kHz')
check('Cascade_Output(' not in irq,'No redundant floating brake in200kHz IRQ')
check('control.hold?0U:cb_final_request' in irq,'Immediate light-load hold retained')
check('Cascade_Pulse(request,1U,&credit)' in irq,'Same tested pulse allocator')
check('Board_GatesOff();' in stop and 'cb_request=cb_final_request=0U' in stop and 'cb_start_used=1U' in stop,'Latched OFF and both commands zero')
check('Cascade_Reset(&control' in start and start.index('Cascade_Reset')<start.index('p=Lock()'),'Reset not in IRQ-masked region')
check('cb_selftest_left||cb_control_max>BENCH_CONTROL_BUDGET_CYCLES' in start,'Timing selftest required')
check('BENCH_ADC_PERIOD_CYCLES (OL_CLOCK_HZ/50000U)' in cfg,'Actual20us ADC deadline')
check('callback_elapsed>BENCH_ADC_CALLBACK_BUDGET_CYCLES' in adc,'Whole callback cutoff before publishing')
check('if(cb_running||(cb_selftest_left&&adc_last))' in irq,'OFF benchmark includes runtime PWM checks')
check('BENCH_TEST_MS' not in cfg+main and 'start_ms' not in main,'Run-duration cutoff removed')
check('HAL_GetTick' not in irq and 'StopLocked(0U)' not in irq,'No elapsed-time normal stop in PWM ISR')
check('timedStop=0' in main,'Explicit untimed telemetry')
check('BENCH_OCP_MA 8000U' in cfg and 'BENCH_OVP_MV 39000U' in cfg,'Model trip configuration')
check('BENCH_MAX_EFFECTIVE_COUNTS 24480U' in cfg and 'BUCK_DEADTIME_COUNTS 164U' in cfg,'Duty and deadtime preserved')
check('BENCH_IIN_ZERO_RAW 3U' in cfg and 'BENCH_IOUT_ZERO_RAW 3U' in cfg,'Explicit zero assumptions')
check('current_scale=(float)ol_vdda_mv*(.01f/4095.0f)' in main,'Raw current to ampere units')
check('voltage_scale=(float)ol_vdda_mv*(20.113f/(4095.0f*1000.0f))' in main,'Raw voltage to volts')
ui=(root/'Monitor/Program.cs').read_text(encoding='utf-8');policy=(root/'Monitor/ManualStartPolicy.cs').read_text(encoding='utf-8')
fw=re.search(r'#define BENCH_FW_ID "([^"]+)"',cfg)[1]
check(fw in policy,'UI exact firmware match')
check('manual_start_only_use_local_button' in ui,'API cannot start MOS')
check(ui.count('_serial!.WriteLine("START")')==1,'Only one manual START writer')
check('CleanControl_Sample' not in main,'Old voltage controller not used')
report={'checks':count,'FirmwareId':fw,'HexSHA256':manifest['SHA256'],'BuildChecksOnly':True,'PerformsFlashing':False,'SendsPowerStart':False}
(root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
