#ifndef CLEAN_BUCK_BENCH_CONFIG_H
#define CLEAN_BUCK_BENCH_CONFIG_H
#define BENCH_FW_ID "CLEANBUCK36-CASCADE-20261027-v7"
#define OL_CLOCK_HZ 170000000U
#define OL_PWM_HZ 200000U
#define OL_PERIOD 27200U
#define OL_ADC_COMPARE 13600U
#define BENCH_MIN_EFFECTIVE_COUNTS 1360U
#define BENCH_MAX_EFFECTIVE_COUNTS 24480U /* validated candidate90% effective */
#define BENCH_DEAD_PWM_COUNTS 656U
#define BENCH_VIN_MIN_MV 45000U
#define BENCH_VIN_MAX_MV 52000U
#define BENCH_TARGET_MV 36000U
#define BENCH_OVP_MV 39000U
#define BENCH_OCP_MA 8000U /* sampled port current, NOT inductor peak */
#define BENCH_ADC_PERIOD_CYCLES (OL_CLOCK_HZ/50000U) /*20us =3400cycles */
/* Wall time includes the higher-priority200kHz PWM IRQ. Reserve4us for
 * conversion callback bookkeeping / HAL return; do not use a fictitious10us
 * deadline for a20us control task. Full callback has an additional bound. */
#define BENCH_CONTROL_BUDGET_CYCLES (BENCH_ADC_PERIOD_CYCLES*4U/5U)
#define BENCH_ADC_CALLBACK_BUDGET_CYCLES (BENCH_ADC_PERIOD_CYCLES-400U)
#define BENCH_IIN_ZERO_RAW 3U
#define BENCH_IOUT_ZERO_RAW 3U
#define BENCH_ADC_TIMEOUT (OL_CLOCK_HZ/10000U)
#define BENCH_MAIN_TIMEOUT (OL_CLOCK_HZ/50U)
#define BENCH_PWM_BUDGET_CYCLES 500U
#define BENCH_PWM_PERIOD_CYCLES (OL_CLOCK_HZ/OL_PWM_HZ)
#define BENCH_NTC_MIN 100U
#define BENCH_NTC_MAX 3000U /* about55C with existing divider assumption */
#define BUCK_PWM_PERIOD_COUNTS OL_PERIOD
#define BUCK_INPUT_DUTY_START 0U
#define BUCK_OUTPUT_HI_END_DCM 24480U /* Timer A internal only, no outputs */
#define BUCK_DEADTIME_COUNTS 164U
#if BENCH_MAX_EFFECTIVE_COUNTS+BENCH_DEAD_PWM_COUNTS>25136U
#error Cascade raw compare exceeds validated90percent effective window
#endif
#endif
