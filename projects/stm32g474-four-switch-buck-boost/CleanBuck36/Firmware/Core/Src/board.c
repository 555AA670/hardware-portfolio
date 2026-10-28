#include "board.h"

ADC_HandleTypeDef ol_adc1, ol_adc2;
HRTIM_HandleTypeDef ol_hrtim;
UART_HandleTypeDef ol_uart;
uint32_t ol_vdda_mv;

static void Check(HAL_StatusTypeDef result)
{
  if (result != HAL_OK) Error_Handler();
}

/* Safe before HAL_Init and from fault handlers. No HAL delays or callbacks. */
void Board_GatesOff(void)
{
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  __DSB();
  if (RCC->APB2ENR & RCC_APB2ENR_HRTIM1EN)
    HRTIM1->sCommonRegs.ODISR = OL_ALL_OUTPUTS;
  GPIOA->BRR = OL_GATE_PINS;
  GPIOA->MODER = (GPIOA->MODER & ~0x00FF0000U) | 0x00550000U;
  GPIOA->OTYPER &= ~OL_GATE_PINS;
  GPIOA->PUPDR = (GPIOA->PUPDR & ~0x00FF0000U) | 0x00AA0000U;
  __DSB();
}

void Board_FatalOff(void)
{
  __disable_irq();
  Board_GatesOff();
  for (;;) { __NOP(); }
}

void Error_Handler(void) { Board_FatalOff(); }

static void Clock_Init(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};
  Check(HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST));
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLM = RCC_PLLM_DIV6;
  osc.PLL.PLLN = 85;
  osc.PLL.PLLP = RCC_PLLP_DIV2;
  osc.PLL.PLLQ = RCC_PLLQ_DIV2;
  osc.PLL.PLLR = RCC_PLLR_DIV2;
  Check(HAL_RCC_OscConfig(&osc));
  clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  Check(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4));
  if (SystemCoreClock != OL_CLOCK_HZ) Error_Handler();
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  HAL_NVIC_SetPriority(SysTick_IRQn, 2U, 0U);
}

static void Adc_Base(ADC_HandleTypeDef *adc, ADC_TypeDef *instance)
{
  adc->Instance = instance;
  adc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  adc->Init.Resolution = ADC_RESOLUTION_12B;
  adc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
  adc->Init.ScanConvMode = ADC_SCAN_ENABLE;
  adc->Init.EOCSelection = ADC_EOC_SEQ_CONV;
  adc->Init.NbrOfConversion = 1U;
  adc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
  adc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  adc->Init.Overrun = ADC_OVR_DATA_PRESERVED;
  adc->Init.OversamplingMode = DISABLE;
  Check(HAL_ADC_Init(adc));
  Check(HAL_ADCEx_Calibration_Start(adc, ADC_SINGLE_ENDED));
}

uint16_t Board_ReadNtc(uint32_t index)
{
  ADC_ChannelConfTypeDef ch = {0};
  uint16_t value;
  ch.Channel = index ? ADC_CHANNEL_13 : ADC_CHANNEL_17;
  ch.Rank = ADC_REGULAR_RANK_1;
  ch.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
  ch.SingleDiff = ADC_SINGLE_ENDED;
  ch.OffsetNumber = ADC_OFFSET_NONE;
  Check(HAL_ADC_ConfigChannel(&ol_adc2, &ch));
  Check(HAL_ADC_Start(&ol_adc2));
  Check(HAL_ADC_PollForConversion(&ol_adc2, 2U));
  value = (uint16_t)HAL_ADC_GetValue(&ol_adc2);
  Check(HAL_ADC_Stop(&ol_adc2));
  return value;
}

static void Adc_Init(void)
{
  GPIO_InitTypeDef pin = {0};
  ADC_ChannelConfTypeDef ref = {0};
  ADC_MultiModeTypeDef multi = {0};
  ADC_InjectionConfTypeDef inj = {0};
  uint32_t ref_raw;
  const uint32_t channels[4] = {ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4};
  const uint32_t ranks[4] = {ADC_INJECTED_RANK_1, ADC_INJECTED_RANK_2,
                           ADC_INJECTED_RANK_3, ADC_INJECTED_RANK_4};
  __HAL_RCC_ADC12_CLK_ENABLE();
  pin.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
  pin.Mode = GPIO_MODE_ANALOG;
  pin.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &pin);
  Adc_Base(&ol_adc1, ADC1);
  multi.Mode = ADC_MODE_INDEPENDENT;
  Check(HAL_ADCEx_MultiModeConfigChannel(&ol_adc1, &multi));
  ref.Channel = ADC_CHANNEL_VREFINT;
  ref.Rank = ADC_REGULAR_RANK_1;
  ref.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  ref.SingleDiff = ADC_SINGLE_ENDED;
  ref.OffsetNumber = ADC_OFFSET_NONE;
  Check(HAL_ADC_ConfigChannel(&ol_adc1, &ref));
  Check(HAL_ADC_Start(&ol_adc1));
  Check(HAL_ADC_PollForConversion(&ol_adc1, 10U));
  ref_raw = HAL_ADC_GetValue(&ol_adc1);
  Check(HAL_ADC_Stop(&ol_adc1));
  if (!ref_raw) Error_Handler();
  ol_vdda_mv = VREFINT_CAL_VREF * (uint32_t)(*VREFINT_CAL_ADDR) / ref_raw;
  if (ol_vdda_mv < 2800U || ol_vdda_mv > 3600U) Error_Handler();
  inj.InjectedNbrOfConversion = 4U;
  inj.InjectedSamplingTime = ADC_SAMPLETIME_12CYCLES_5;
  inj.InjectedSingleDiff = ADC_SINGLE_ENDED;
  inj.InjectedOffsetNumber = ADC_OFFSET_NONE;
  inj.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJEC_HRTIM_TRG2;
  inj.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
  inj.InjecOversamplingMode = ENABLE;
  inj.InjecOversampling.Ratio = ADC_OVERSAMPLING_RATIO_4;
  inj.InjecOversampling.RightBitShift = ADC_RIGHTBITSHIFT_2;
  for (uint32_t i = 0U; i < 4U; ++i)
  {
    inj.InjectedChannel = channels[i];
    inj.InjectedRank = ranks[i];
    Check(HAL_ADCEx_InjectedConfigChannel(&ol_adc1, &inj));
  }
  Adc_Base(&ol_adc2, ADC2);
}

static void Uart_Init(void)
{
  GPIO_InitTypeDef pin = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  pin.Pin = GPIO_PIN_4 | GPIO_PIN_5;
  pin.Mode = GPIO_MODE_AF_PP;
  pin.Pull = GPIO_PULLUP;
  pin.Speed = GPIO_SPEED_FREQ_HIGH;
  pin.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOC, &pin);
  ol_uart.Instance = USART1;
  ol_uart.Init.BaudRate = 115200U;
  ol_uart.Init.WordLength = UART_WORDLENGTH_8B;
  ol_uart.Init.StopBits = UART_STOPBITS_1;
  ol_uart.Init.Parity = UART_PARITY_NONE;
  ol_uart.Init.Mode = UART_MODE_TX_RX;
  ol_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  ol_uart.Init.OverSampling = UART_OVERSAMPLING_16;
  ol_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  ol_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  Check(HAL_UART_Init(&ol_uart));
  HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void Pwm_Init(void)
{
  HRTIM_ADCTriggerCfgTypeDef adc_trigger = {0};
  HRTIM_TimeBaseCfgTypeDef timebase = {0};
  HRTIM_TimerCtlTypeDef timer_ctl = {0};
  HRTIM_TimerCfgTypeDef timer_cfg = {0};
  HRTIM_CompareCfgTypeDef compare_cfg = {0};
  HRTIM_DeadTimeCfgTypeDef deadtime_cfg = {0};
  HRTIM_OutputCfgTypeDef output_cfg = {0};

  __HAL_RCC_HRTIM1_CLK_ENABLE();

  ol_hrtim.Instance = HRTIM1;
  ol_hrtim.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
  ol_hrtim.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
  if (HAL_HRTIM_Init(&ol_hrtim) != HAL_OK) Error_Handler();
  if (HAL_HRTIM_DLLCalibrationStart(&ol_hrtim, HRTIM_CALIBRATIONRATE_3) != HAL_OK) Error_Handler();
  if (HAL_HRTIM_PollForDLLCalibration(&ol_hrtim, 100U) != HAL_OK) Error_Handler();

  /* ST B-G474E-DPOW1 pattern: HRTIM compare event triggers the injected ADC
     sequence at a deterministic point inside the active buck pulse. */
  adc_trigger.UpdateSource = HRTIM_ADCTRIGGERUPDATE_TIMER_A;
  adc_trigger.Trigger = HRTIM_ADCTRIGGEREVENT24_TIMERB_CMP2;
  if (HAL_HRTIM_ADCTriggerConfig(&ol_hrtim, HRTIM_ADCTRIGGER_2,
                                 &adc_trigger) != HAL_OK) Error_Handler();
  /* Divide the 200kHz compare events by four. A 50kHz trigger leaves 20us
     for the four-channel, 4x-oversampled injected sequence. */
  if (HAL_HRTIM_ADCPostScalerConfig(&ol_hrtim, HRTIM_ADCTRIGGER_2,
                                    3U) != HAL_OK) Error_Handler();

  timebase.Period = BUCK_PWM_PERIOD_COUNTS;
  timebase.RepetitionCounter = 3U; /* Timer A only: ADC update clock */
  timebase.PrescalerRatio = HRTIM_PRESCALERRATIO_MUL32;
  timebase.Mode = HRTIM_MODE_CONTINUOUS;
  if (HAL_HRTIM_TimeBaseConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_A, &timebase) != HAL_OK) Error_Handler();
  timebase.RepetitionCounter = 0U; /* one shadow latch / complete PWM cycle */
  if (HAL_HRTIM_TimeBaseConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B, &timebase) != HAL_OK) Error_Handler();

  timer_ctl.UpDownMode = HRTIM_TIMERUPDOWNMODE_UP;
  timer_ctl.TrigHalf = HRTIM_TIMERTRIGHALF_DISABLED;
  timer_ctl.GreaterCMP1 = HRTIM_TIMERGTCMP1_EQUAL;
  timer_ctl.DualChannelDacEnable = HRTIM_TIMER_DCDE_DISABLED;
  if (HAL_HRTIM_WaveformTimerControl(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_A, &timer_ctl) != HAL_OK) Error_Handler();
  if (HAL_HRTIM_WaveformTimerControl(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B, &timer_ctl) != HAL_OK) Error_Handler();

  timer_cfg.InterruptRequests = HRTIM_TIM_IT_NONE;
  timer_cfg.DMARequests = HRTIM_TIM_DMA_NONE;
  timer_cfg.DMASrcAddress = 0U;
  timer_cfg.DMADstAddress = 0U;
  timer_cfg.DMASize = 1U;
  timer_cfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
  timer_cfg.InterleavedMode = HRTIM_INTERLEAVED_MODE_DISABLED;
  timer_cfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
  timer_cfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
  timer_cfg.DACSynchro = HRTIM_DACSYNC_NONE;
  timer_cfg.PreloadEnable = HRTIM_PRELOAD_ENABLED;
  timer_cfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
  timer_cfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
  timer_cfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_ENABLED;
  timer_cfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
  timer_cfg.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
  timer_cfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
  timer_cfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_ENABLED;
  timer_cfg.DelayedProtectionMode = HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
  timer_cfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
  timer_cfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;
  timer_cfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
  timer_cfg.ReSyncUpdate = HRTIM_TIMERESYNC_UPDATE_UNCONDITIONAL;
  if (HAL_HRTIM_WaveformTimerConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_A, &timer_cfg) != HAL_OK) Error_Handler();
  /* SW1 input bridge: TB1/TB2 are a hardware-generated complementary pair. */
  timer_cfg.InterruptRequests = HRTIM_TIM_IT_REP;
  timer_cfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_ENABLED;
  if (HAL_HRTIM_WaveformTimerConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B, &timer_cfg) != HAL_OK) Error_Handler();

  compare_cfg.AutoDelayedMode = HRTIM_AUTODELAYEDMODE_REGULAR;
  compare_cfg.AutoDelayedTimeout = 0U;
  /* Timer A drives SW2. PA8/TA1 is wired to LI, so TA1 is asserted only
     from CMP1 to PER; complementary TA2/PA9 (HI) is active for the first
     90% of the cycle in DCM. */
  compare_cfg.CompareValue = BUCK_OUTPUT_HI_END_DCM;
  if (HAL_HRTIM_WaveformCompareConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_A,
                                      HRTIM_COMPAREUNIT_1, &compare_cfg) != HAL_OK) Error_Handler();
  /* Timer B drives SW1. PA10/TB1 is wired to HI and is soft-started. */
  compare_cfg.CompareValue = BUCK_INPUT_DUTY_START;
  if (HAL_HRTIM_WaveformCompareConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B,
                                      HRTIM_COMPAREUNIT_1, &compare_cfg) != HAL_OK) Error_Handler();
  compare_cfg.CompareValue = OL_ADC_COMPARE; /* independent of zero/PDM duty */
  if (HAL_HRTIM_WaveformCompareConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B,
                                      HRTIM_COMPAREUNIT_2, &compare_cfg) != HAL_OK) Error_Handler();

  deadtime_cfg.Prescaler = HRTIM_TIMDEADTIME_PRESCALERRATIO_MUL8;
  deadtime_cfg.RisingValue = BUCK_DEADTIME_COUNTS;
  deadtime_cfg.RisingSign = HRTIM_TIMDEADTIME_RISINGSIGN_POSITIVE;
  deadtime_cfg.RisingLock = HRTIM_TIMDEADTIME_RISINGLOCK_WRITE;
  deadtime_cfg.RisingSignLock = HRTIM_TIMDEADTIME_RISINGSIGNLOCK_READONLY;
  deadtime_cfg.FallingValue = BUCK_DEADTIME_COUNTS;
  deadtime_cfg.FallingSign = HRTIM_TIMDEADTIME_FALLINGSIGN_POSITIVE;
  deadtime_cfg.FallingLock = HRTIM_TIMDEADTIME_FALLINGLOCK_WRITE;
  deadtime_cfg.FallingSignLock = HRTIM_TIMDEADTIME_FALLINGSIGNLOCK_READONLY;
  if (HAL_HRTIM_DeadTimeConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_A, &deadtime_cfg) != HAL_OK) Error_Handler();
  if (HAL_HRTIM_DeadTimeConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B, &deadtime_cfg) != HAL_OK) Error_Handler();

  output_cfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
  output_cfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
  output_cfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
  output_cfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_INACTIVE;
  output_cfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
  output_cfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;

  /* SW2: TA1 is the physical low-side command. Keep it low for the first
     90% in DCM, then set it at CMP1; the dead-time unit generates TA2 as HI. */
  output_cfg.SetSource = HRTIM_OUTPUTSET_TIMCMP1;
  output_cfg.ResetSource = HRTIM_OUTPUTRESET_TIMPER;
  if (HAL_HRTIM_WaveformOutputConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_A,
                                     HRTIM_OUTPUT_TA1, &output_cfg) != HAL_OK) Error_Handler();

  /* With dead-time insertion enabled, output 2 is generated as the
     complementary signal from output 1. It must not have its own events. */
  output_cfg.SetSource = HRTIM_OUTPUTSET_NONE;
  output_cfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
  if (HAL_HRTIM_WaveformOutputConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_A,
                                     HRTIM_OUTPUT_TA2, &output_cfg) != HAL_OK) Error_Handler();

  /* SW1: TB1 is the physical high-side command, active PER-to-CMP1. */
  output_cfg.SetSource = HRTIM_OUTPUTSET_TIMPER;
  output_cfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
  if (HAL_HRTIM_WaveformOutputConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B,
                                     HRTIM_OUTPUT_TB1, &output_cfg) != HAL_OK) Error_Handler();
  /* TB2 is generated as TB1's dead-time-protected complement. */
  output_cfg.SetSource = HRTIM_OUTPUTSET_NONE;
  output_cfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
  if (HAL_HRTIM_WaveformOutputConfig(&ol_hrtim, HRTIM_TIMERINDEX_TIMER_B,
                                     HRTIM_OUTPUT_TB2, &output_cfg) != HAL_OK) Error_Handler();

  /* Transfer preloaded values before the pins are handed to HRTIM. */
  if (HAL_HRTIM_SoftwareUpdate(&ol_hrtim,
      HRTIM_TIMERUPDATE_A | HRTIM_TIMERUPDATE_B) != HAL_OK) Error_Handler();

  /* Deliberate safety delta: retain GPIO LOW until the human START.
     PA10/PA11 are handed to HRTIM at PWM entry; PA8/PA9 stay GPIO LOW. */
  Board_GatesOff();
  HAL_NVIC_SetPriority(ADC1_2_IRQn, 0U, 0U);
  HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
  HAL_NVIC_SetPriority(HRTIM1_TIMB_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(HRTIM1_TIMB_IRQn);
}

/* Caller owns the interrupt-masked START transaction; boot has held CMP1=0
 * for >=200ms. No runtime SoftwareUpdate and no OEN-based pulse modulation. */
void Board_ArmZero(void)
{
  if (!__get_PRIMASK() ||
      HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_B].CMP1xR != 0U ||
      (HRTIM1->sCommonRegs.OENR & OL_ALL_OUTPUTS)) Board_FatalOff();
  /* Called by TimerB at zero command. Only PA10/11 change; PA8/9 stay low.
   * Fixed writes avoid HAL GPIO loops inside the timing-critical arm step. */
  GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0x0000FF00U) | 0x0000DD00U;
  GPIOA->OSPEEDR |= 0x00F00000U;
  GPIOA->MODER = (GPIOA->MODER & ~0x00F00000U) | 0x00A00000U;
  HRTIM1->sCommonRegs.OENR = OL_PWM_OUTPUTS;
}

void Board_Init(void)
{
  Clock_Init();
  Adc_Init();
  Uart_Init();
  Pwm_Init();
}

void Board_StartSampling(void)
{
  Check(HAL_ADCEx_InjectedStart_IT(&ol_adc1));
  Check(HAL_HRTIM_WaveformCountStart_IT(&ol_hrtim, HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B));
}

void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_DisableUCPDDeadBattery();
}
