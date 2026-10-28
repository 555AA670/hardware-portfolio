#ifndef CLEAN_BUCK_BOARD_H
#define CLEAN_BUCK_BOARD_H
#include "stm32g4xx_hal.h"
#include "bench_config.h"
#define OL_GATE_PINS (GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11)
#define OL_ALL_OUTPUTS (HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2|HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2)
#define OL_PWM_OUTPUTS (HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2)
extern ADC_HandleTypeDef ol_adc1,ol_adc2;
extern HRTIM_HandleTypeDef ol_hrtim;
extern UART_HandleTypeDef ol_uart;
extern uint32_t ol_vdda_mv;
void Board_GatesOff(void);
void Board_ArmZero(void);
void Board_Init(void);
void Board_StartSampling(void);
uint16_t Board_ReadNtc(uint32_t index);
void Board_FatalOff(void);
void Error_Handler(void);
#endif
