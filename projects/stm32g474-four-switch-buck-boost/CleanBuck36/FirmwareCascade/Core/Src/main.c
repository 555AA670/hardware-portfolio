#include "board.h"
#include "cascade_control.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum { F_TEMP=1,F_OC=2,F_OV=3,F_VIN=4,F_ADC=5,F_STOP=9,F_PREFLIGHT=10,F_MAIN=13,F_RIGHT=14,F_PWM=16,F_CONTROL=17 };
volatile uint32_t cb_running,cb_fault,cb_start_used,cb_done,cb_ready;
volatile uint32_t cb_request,cb_next_cmp,cb_active_cmp,cb_reference_mv;
volatile uint32_t cb_final_request;
volatile uint32_t cb_samples,cb_pwm_count,cb_pwm_max,cb_pwm_gap_max,cb_pwm_reason;
volatile uint32_t cb_trace_count,cb_trace_frozen;
volatile uint16_t cb_raw[4],cb_ntc[2];
typedef struct {uint32_t cycles;uint16_t vout,iin,request,active,next,flags;} Trace;
volatile Trace cb_trace[1024];
static CascadeControl control,shadow_control;
volatile uint32_t cb_control_max,cb_selftest_left=1000U,cb_selftest_sink;
volatile uint32_t cb_callback_max,cb_selftest_request;
static uint32_t selftest_credit;
static float voltage_scale,current_scale,observed_iin;
static volatile uint32_t adc_last,main_last,pwm_last;
static volatile uint32_t start_pending,arm_pending,stop_ack,estop_ack,reject_ack,done_ack;
static volatile uint32_t pulse_credit;
static uint32_t ntc_time[2],trace_read,trace_dump;
static uint16_t vin_min_raw,vin_max_raw,ov_raw,oc_raw,start_vout_raw,start_iin_raw;

static uint32_t Lock(void){uint32_t p=__get_PRIMASK();__disable_irq();return p;}
static void Unlock(uint32_t p){__set_PRIMASK(p);}
static uint32_t Millivolts(uint16_t raw)
{return(uint32_t)(((uint64_t)raw*ol_vdda_mv*20113U+2047500U)/4095000U);}
static uint16_t RawVoltage(uint32_t mv)
{return(uint16_t)(((uint64_t)mv*4095000U)/((uint64_t)ol_vdda_mv*20113U));}
static void StopLocked(uint32_t code)
{
 cb_running=0U;cb_request=cb_final_request=0U;pulse_credit=0U;start_pending=arm_pending=0U;
 Board_GatesOff();
 HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP1xR=0U;
 cb_next_cmp=0U;cb_ready=0U;cb_start_used=1U;cb_done=1U;
 if(code&&!cb_fault)cb_fault=code;
 cb_trace_frozen=1U;done_ack=1U;
}
static void Stop(uint32_t code){uint32_t p=Lock();StopLocked(code);Unlock(p);}

/* Only the200kHz timer IRQ owns runtime CMP1 writes. Same tested cascade
 * output brake / complete-pulse allocator. No DMA or asynchronous OEN skip. */
void HRTIM1_TIMB_IRQHandler(void)
{
 uint32_t entry=DWT->CYCCNT,cmp=0U,gap;
 if(!(HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].TIMxISR&HRTIM_TIMISR_REP))return;
 HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].TIMxICR=HRTIM_TIMICR_REPC;
 ++cb_pwm_count;
 gap=entry-pwm_last;if(pwm_last&&gap>cb_pwm_gap_max)cb_pwm_gap_max=gap;pwm_last=entry;
 cb_active_cmp=cb_next_cmp; /* predicted shadow transfer, not a gate measurement */
 if(arm_pending){
  arm_pending=0U;
  if(gap>BENCH_PWM_PERIOD_CYCLES*3U/2U){cb_pwm_reason=1U;StopLocked(F_PWM);}
  else if(HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CNTxR>OL_PERIOD-4096U){cb_pwm_reason=2U;StopLocked(F_PWM);}
  else{
   uint32_t p=Lock();
   main_last=entry;Board_ArmZero();
   cb_running=1U;cb_ready=0U;Unlock(p);
  }
 }
 /* During the OFF benchmark, exercise normal runtime IRQ checks too. Wait
  * for the first ADC before applying its stale-data check. No pin is armed. */
 if(cb_running||(cb_selftest_left&&adc_last)){
  if(gap>BENCH_PWM_PERIOD_CYCLES*3U/2U){cb_pwm_reason=1U;StopLocked(F_PWM);}
  else if(HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CNTxR>OL_PERIOD-4096U){cb_pwm_reason=2U;StopLocked(F_PWM);}
  else if(entry-adc_last>BENCH_ADC_TIMEOUT)StopLocked(F_ADC);
  else if(entry-main_last>BENCH_MAIN_TIMEOUT)StopLocked(F_MAIN);
  else if((GPIOA->IDR&(GPIO_PIN_8|GPIO_PIN_9)) ||
          (HRTIM1->sCommonRegs.OENR&(HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2)))StopLocked(F_RIGHT);
 }
 if(cb_running&&!cb_fault){
  uint32_t credit=pulse_credit;
  /* Control state changes at50kHz, not200kHz. ADC publishes the identical
   * final brake result once; TimerB stays integer-only. Immediate hold still
   * overrides any pending request, including mid-computation ADC updates. */
  uint32_t request=control.hold?0U:cb_final_request;
  cmp=Cascade_Pulse(request,1U,&credit);pulse_credit=credit;
  if(cmp==0xFFFFFFFFU){cb_pwm_reason=3U;StopLocked(F_PWM);cmp=0U;}
 }else if(cb_selftest_left&&adc_last){
  cb_selftest_sink=Cascade_Pulse(shadow_control.hold?0U:cb_selftest_request,1U,&selftest_credit);
  /* Deliberately discard the shadow pulse. Real CMP remains zero. */
 }
 HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP1xR=cmp;cb_next_cmp=cmp;
 {uint32_t elapsed=DWT->CYCCNT-entry;
  if(elapsed>cb_pwm_max)cb_pwm_max=elapsed;
  if(cb_running&&elapsed>BENCH_PWM_BUDGET_CYCLES){cb_pwm_reason=4U;StopLocked(F_PWM);}}
}
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *adc)
{
 uint32_t now=DWT->CYCCNT;
 if(adc!=&ol_adc1)return;
 cb_raw[0]=(uint16_t)ADC1->JDR1;cb_raw[1]=(uint16_t)ADC1->JDR2;
 cb_raw[2]=(uint16_t)ADC1->JDR3;cb_raw[3]=(uint16_t)ADC1->JDR4;
 adc_last=now;++cb_samples;
 if(!cb_running){
  if(cb_selftest_left){
   uint32_t begin=DWT->CYCCNT,elapsed;
   cb_selftest_sink=Cascade_Sample(&shadow_control,36.0f,48.0f,3.6f,2.76f);
   cb_selftest_sink=Cascade_Output(&shadow_control,cb_selftest_sink);
   cb_selftest_request=cb_selftest_sink;
   elapsed=DWT->CYCCNT-begin;if(elapsed>cb_control_max)cb_control_max=elapsed;
   --cb_selftest_left;
  }
  return;
 }
 if(!cb_trace_frozen){
  uint32_t n=cb_trace_count&1023U;
  cb_trace[n].cycles=now;cb_trace[n].vout=cb_raw[1];cb_trace[n].iin=cb_raw[2];
  cb_trace[n].request=(uint16_t)cb_request;cb_trace[n].active=(uint16_t)cb_active_cmp;
  cb_trace[n].next=(uint16_t)cb_next_cmp;cb_trace[n].flags=(uint16_t)(cb_running|(cb_reference_mv>=36000U?2U:0U));
  __DMB();++cb_trace_count;
 }
 if(cb_raw[1]>=ov_raw){Stop(F_OV);return;}
 if(cb_raw[2]>=oc_raw){Stop(F_OC);return;}
 if(cb_raw[0]<vin_min_raw||cb_raw[0]>vin_max_raw){Stop(F_VIN);return;}
 {uint32_t begin=DWT->CYCCNT,elapsed,request,final_request;
  float iin=cb_raw[2]>BENCH_IIN_ZERO_RAW?(cb_raw[2]-BENCH_IIN_ZERO_RAW)*current_scale:0.0f;
  float iout=cb_raw[3]>BENCH_IOUT_ZERO_RAW?(cb_raw[3]-BENCH_IOUT_ZERO_RAW)*current_scale:0.0f;
  /*45us observation pole. This does not convert a fixed-phase spike into
   * a proven average; raw and observed values are both exported for checks. */
  observed_iin+=.3588196116f*(iin-observed_iin);
  request=Cascade_Sample(&control,cb_raw[1]*voltage_scale,cb_raw[0]*voltage_scale,iout,observed_iin);
  final_request=Cascade_Output(&control,request);
  elapsed=DWT->CYCCNT-begin;if(elapsed>cb_control_max)cb_control_max=elapsed;
  if(elapsed>BENCH_CONTROL_BUDGET_CYCLES){Stop(F_CONTROL);return;}
  {
  uint32_t callback_elapsed=DWT->CYCCNT-now,p;
  if(callback_elapsed>cb_callback_max)cb_callback_max=callback_elapsed;
  if(callback_elapsed>BENCH_ADC_CALLBACK_BUDGET_CYCLES){Stop(F_CONTROL);return;}
  p=Lock();
  if(cb_running&&!cb_fault){cb_request=request;cb_final_request=final_request;cb_reference_mv=(uint32_t)(control.reference*1000.0f+.5f);}
  Unlock(p);}}
}
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc){if(adc==&ol_adc1)Stop(F_ADC);}
void ADC1_2_IRQHandler(void){HAL_ADC_IRQHandler(&ol_adc1);}
void SysTick_Handler(void){HAL_IncTick();}
void USART1_IRQHandler(void){HAL_UART_IRQHandler(&ol_uart);}
__asm void HardFault_Handler(void){
 IMPORT Board_FatalOff
 B Board_FatalOff
}
__asm void NMI_Handler(void){
 IMPORT Board_FatalOff
 B Board_FatalOff
}
__asm void MemManage_Handler(void){
 IMPORT Board_FatalOff
 B Board_FatalOff
}
__asm void BusFault_Handler(void){
 IMPORT Board_FatalOff
 B Board_FatalOff
}
__asm void UsageFault_Handler(void){
 IMPORT Board_FatalOff
 B Board_FatalOff
}
void SVC_Handler(void){}
void DebugMon_Handler(void){}
void PendSV_Handler(void){}

/* Main is sole TX producer, ISR sole consumer. Copy with IRQs ENABLED;
 * publish only after the complete line is in the ring. */
#define TX_BYTES 4096U
static uint8_t tx[TX_BYTES],rx;
static volatile uint32_t tx_head,tx_tail,tx_busy,tx_length;
static char rx_line[24];static uint32_t rx_length,rx_invalid;
static void TxKick(void)
{
 if(!tx_busy&&tx_head!=tx_tail){
  uint32_t n=tx_head>=tx_tail?tx_head-tx_tail:TX_BYTES-tx_tail;
  if(n>128U)n=128U;tx_length=n;tx_busy=1U;
  if(HAL_UART_Transmit_IT(&ol_uart,&tx[tx_tail],(uint16_t)n)!=HAL_OK){tx_busy=0U;StopLocked(F_MAIN);}
 }
}
static int Log(const char *fmt,...)
{
 char line[480];va_list args;uint32_t free_bytes,p,head;int n;
 va_start(args,fmt);n=vsnprintf(line,sizeof(line),fmt,args);va_end(args);
 if(n<=0||n>=(int)sizeof(line))return 0;
 head=tx_head;free_bytes=(tx_tail+TX_BYTES-head-1U)%TX_BYTES;
 if(free_bytes<(uint32_t)n)return 0;
 for(int k=0;k<n;++k){tx[head]=(uint8_t)line[k];head=(head+1U)%TX_BYTES;}
 __DMB();p=Lock();tx_head=head;TxKick();Unlock(p);return 1;
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
 if(uart!=&ol_uart)return;
 tx_tail=(tx_tail+tx_length)%TX_BYTES;tx_busy=0U;
 {uint32_t p=Lock();TxKick();Unlock(p);}
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart){if(uart==&ol_uart)Board_FatalOff();}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
 if(uart!=&ol_uart)return;
 if(rx=='\r'||rx=='\n'){
  uint32_t p;rx_line[rx_length]=0;if(rx_invalid)rx_line[0]=0;p=Lock();
  if(!strcmp(rx_line,"ESTOP")){StopLocked(F_STOP);estop_ack=1U;}
  else if(!strcmp(rx_line,"STOP")){StopLocked(0U);stop_ack=1U;}
  else if(!strcmp(rx_line,"START")){
   if(cb_ready&&!cb_start_used&&!cb_running&&!cb_fault&&!start_pending){cb_start_used=1U;start_pending=1U;}
   else reject_ack=1U;
  }else if(!strcmp(rx_line,"TRACE")&&!cb_running){trace_dump=1U;trace_read=0U;}
  Unlock(p);rx_length=rx_invalid=0U;
 }else if(rx_length<sizeof(rx_line)-1U)rx_line[rx_length++]=(char)rx;
 else rx_invalid=1U;
 if(HAL_UART_Receive_IT(&ol_uart,&rx,1U)!=HAL_OK)Board_FatalOff();
}
static void Start(void)
{
 uint32_t now,p,initial_mv;
 /* The ADC callback does not touch control while OFF. Prepare floating point
  * state with IRQs enabled, then publish a short arm request atomically. */
 if(!start_pending||cb_running||cb_fault)return;
 initial_mv=Millivolts(cb_raw[1]);
 Cascade_Reset(&control,(float)initial_mv/1000.0f);observed_iin=0.0f;
 p=Lock();now=DWT->CYCCNT;
 if(!start_pending||cb_running||cb_fault){start_pending=0U;Unlock(p);return;}
 start_pending=0U;
 if(!cb_ready||now-adc_last>BENCH_ADC_TIMEOUT||cb_raw[0]<vin_min_raw||cb_raw[0]>vin_max_raw||
    cb_raw[1]>=start_vout_raw||cb_raw[2]>start_iin_raw||
    HAL_GetTick()-ntc_time[0]>100U||HAL_GetTick()-ntc_time[1]>100U||
    cb_ntc[0]<=BENCH_NTC_MIN||cb_ntc[1]<=BENCH_NTC_MIN||
    cb_ntc[0]>=BENCH_NTC_MAX||cb_ntc[1]>=BENCH_NTC_MAX||
    cb_selftest_left||cb_control_max>BENCH_CONTROL_BUDGET_CYCLES||
    cb_pwm_max>BENCH_PWM_BUDGET_CYCLES||cb_pwm_gap_max>BENCH_PWM_PERIOD_CYCLES*3U/2U||
    HRTIM1->sCommonRegs.OENR&OL_ALL_OUTPUTS){StopLocked(F_PREFLIGHT);Unlock(p);return;}
 cb_reference_mv=initial_mv;cb_request=cb_final_request=cb_next_cmp=cb_active_cmp=0U;pulse_credit=0U;
 cb_trace_count=cb_trace_frozen=0U;cb_ready=0U;
 arm_pending=1U; /* Only TimerB REP may enable the zero-command stage. */
 Unlock(p);Log("CMD START ACCEPT fw=%s CASCADE timedStop=0 rightBridge=OFF manualStart=1\r\n",BENCH_FW_ID);
}
static int Temperature(uint16_t raw)
{
 if(raw==0U||raw>=4095U)return-9999;
 return(int)((1.0f/(1.0f/298.15f+logf((4095.0f-raw)/raw)/3380.0f)-273.15f)*10.0f);
}
static void Status(void)
{
 uint16_t a[4],n[2];uint32_t running,fault,ref,used,done,request,next,active,ready,p=Lock();
 for(uint32_t k=0;k<4;++k)a[k]=cb_raw[k];n[0]=cb_ntc[0];n[1]=cb_ntc[1];
 running=cb_running;fault=cb_fault;ref=cb_reference_mv;used=cb_start_used;done=cb_done;
 request=cb_request;next=cb_next_cmp;active=cb_active_cmp;ready=cb_ready;Unlock(p);
 {uint32_t vi=Millivolts(a[0]),vo=Millivolts(a[1]),ii=(uint32_t)a[2]*ol_vdda_mv*10U/4095U;
  uint32_t io=(uint32_t)a[3]*ol_vdda_mv*10U/4095U;int t0=Temperature(n[0]),t1=Temperature(n[1]);
  Log("BUCK run=%lu stage=%u ref=%lumV skip=%lu fault=%u fcode=%lu startUsed=%lu testDone=%lu fw=%s manualStart=1 ready=%lu ctrl=9 load=%lu Vfb=%lumV duty=%lu.%02lu%% cmp=%lu OEN=%03lX\r\n",
   (unsigned long)running,running?(ref>=36000U?2U:1U):0U,(unsigned long)ref,(unsigned long)(running?control.hold:0U),fault!=0U,(unsigned long)fault,
   (unsigned long)used,(unsigned long)done,BENCH_FW_ID,(unsigned long)ready,(unsigned long)(running&&!control.light_load),(unsigned long)vo,
   (unsigned long)((next>BENCH_DEAD_PWM_COUNTS?next-BENCH_DEAD_PWM_COUNTS:0U)*10000U/OL_PERIOD/100U),
   (unsigned long)((next>BENCH_DEAD_PWM_COUNTS?next-BENCH_DEAD_PWM_COUNTS:0U)*10000U/OL_PERIOD%100U),
   (unsigned long)next,(unsigned long)HRTIM1->sCommonRegs.OENR);
  Log("VREF=%lumV Vin=%lu.%02luV Vout=%lu.%02luV Iin=%lu.00mA Iout=%lu.00mA NTC0=%s%d.%dC NTC1=%s%d.%dC\r\n",
   (unsigned long)ol_vdda_mv,(unsigned long)(vi/1000U),(unsigned long)(vi%1000U/10U),
   (unsigned long)(vo/1000U),(unsigned long)(vo%1000U/10U),(unsigned long)ii,(unsigned long)io,
   t0<0?"-":"",abs(t0)/10,abs(t0)%10,t1<0?"-":"",abs(t1)/10,abs(t1)%10);
  Log("CLEAN CTRL request=%lu predictedActive=%lu next=%lu pwmMaxCycles=%lu gapMaxCycles=%lu samples=%lu rightBridge=OFF DMA=0 timedStop=0 timingReason=%lu\r\n",
   (unsigned long)request,(unsigned long)active,(unsigned long)next,(unsigned long)cb_pwm_max,
   (unsigned long)cb_pwm_gap_max,(unsigned long)cb_samples,(unsigned long)cb_pwm_reason);
  Log("CASCADE Iref=%lumA Iobs=%lumA active=%lu refLimit=%lu ff=%lu requested=%lu slew=%lu final=%lu adcMaxCycles=%lu callbackMax=%lu budget=%u selftest=%lu rawIin=%u rawIout=%u zero=3 currentRefMax=4000mA sampledTrip=8000mA\r\n",
   (unsigned long)(control.current_reference*1000.0f+.5f),(unsigned long)(observed_iin*1000.0f+.5f),
   (unsigned long)(running?control.current_active:0U),(unsigned long)control.current_reference_limited,
   (unsigned long)(control.feedforward*27200.0f+.5f),(unsigned long)control.unclamped,
   (unsigned long)control.request,(unsigned long)(next?next-BENCH_DEAD_PWM_COUNTS:0U),
   (unsigned long)cb_control_max,(unsigned long)cb_callback_max,BENCH_CONTROL_BUDGET_CYCLES,(unsigned long)cb_selftest_left,a[2],a[3]);
 }
}
int main(void)
{
 uint32_t next_ntc=0U,next_status=0U,index=0U;
 Board_GatesOff();HAL_Init();Board_Init();
 HAL_NVIC_SetPriority(HRTIM1_TIMB_IRQn,0U,0U);HAL_NVIC_SetPriority(ADC1_2_IRQn,1U,0U);
 HAL_NVIC_SetPriority(USART1_IRQn,2U,0U);HAL_NVIC_SetPriority(SysTick_IRQn,3U,0U);
 vin_min_raw=RawVoltage(BENCH_VIN_MIN_MV);vin_max_raw=RawVoltage(BENCH_VIN_MAX_MV);
 ov_raw=RawVoltage(BENCH_OVP_MV);oc_raw=(uint16_t)((BENCH_OCP_MA*4095U+ol_vdda_mv*10U-1U)/(ol_vdda_mv*10U));
 start_vout_raw=RawVoltage(1500U);start_iin_raw=(uint16_t)((200U*4095U)/(ol_vdda_mv*10U));
 for(uint32_t k=0;k<2;++k){cb_ntc[k]=Board_ReadNtc(k);ntc_time[k]=HAL_GetTick();}
 voltage_scale=(float)ol_vdda_mv*(20.113f/(4095.0f*1000.0f));
 current_scale=(float)ol_vdda_mv*(.01f/4095.0f);
 Cascade_Reset(&control,0.0f);Cascade_Reset(&shadow_control,36.0f);
 main_last=DWT->CYCCNT;Board_StartSampling();
 if(HAL_UART_Receive_IT(&ol_uart,&rx,1U)!=HAL_OK)Board_FatalOff();
 Log("=== G474 FSBB boot: USART1 PC4(TX)/PC5(RX) 115200 8N1 ===\r\nTEST fw=%s BOOT OFF; HUMAN START once; CASCADE ref=36000 Vin=45..52V OVP=39V OCPmA=8000 currentRefMax=4000 deadtimeNs=120 sampledProtection=1 analogPeakTrip=0 timedStop=0\r\n",BENCH_FW_ID);
 for(;;){
  uint32_t ms=HAL_GetTick();main_last=DWT->CYCCNT;
  if(!cb_start_used&&!cb_fault&&ms>=200U&&cb_samples>=1000U){
   uint32_t p=Lock();cb_ready=(!cb_selftest_left&&cb_control_max<=BENCH_CONTROL_BUDGET_CYCLES&&cb_pwm_max<=BENCH_PWM_BUDGET_CYCLES&&
     cb_pwm_gap_max<=BENCH_PWM_PERIOD_CYCLES*3U/2U&&DWT->CYCCNT-adc_last<BENCH_ADC_TIMEOUT);Unlock(p);
  }
  if((int32_t)(ms-next_ntc)>=0){
   uint16_t value=Board_ReadNtc(index);cb_ntc[index]=value;ntc_time[index]=ms;
   index^=1U;next_ntc=ms+10U;
   if(cb_running&&(value<=BENCH_NTC_MIN||value>=BENCH_NTC_MAX))Stop(F_TEMP);
  }
  if(start_pending)Start();
  if(stop_ack&&Log("CMD STOP ACCEPT; outputs OFF\r\n"))stop_ack=0U;
  if(estop_ack&&Log("CMD ESTOP LATCHED; outputs OFF; reset required\r\n"))estop_ack=0U;
  if(reject_ack&&Log("CMD START REJECT locked=1\r\n"))reject_ack=0U;
  if(done_ack&&Log("TEST DONE fw=%s fault=%lu OEN=%03lX startUsed=1\r\n",BENCH_FW_ID,
      (unsigned long)cb_fault,(unsigned long)HRTIM1->sCommonRegs.OENR))done_ack=0U;
  if((int32_t)(ms-next_status)>=0){next_status=ms+250U;Status();}
  if(trace_dump&&!cb_running){
   uint32_t total=cb_trace_count,count=total<1024U?total:1024U;
   if(trace_read<count){
    const volatile Trace *t=&cb_trace[(total-count+trace_read)&1023U];
    if(Log("CBTRACE,%lu,%lu,%u,%u,%u,%u,%u,%u\r\n",(unsigned long)trace_read,
     (unsigned long)t->cycles,t->vout,t->iin,t->request,t->active,t->next,t->flags))++trace_read;
   }else if(Log("CBTRACE END samples=%lu clockHz=170000000 ADC=50000 predictedCMP=1\r\n",(unsigned long)count))trace_dump=0U;
  }
 }
}
