#ifndef CLEAN_BUCK_CONTROL_H
#define CLEAN_BUCK_CONTROL_H
#include <stdint.h>
typedef struct {
 float integral,previous_pi,previous_u,previous_e,filtered,reference;
 uint32_t divider,request;
} CleanControl;
void CleanControl_Reset(CleanControl *c,float output);
uint32_t CleanControl_Sample(CleanControl *c,float measured);
uint32_t CleanPulse_Next(uint32_t request,uint32_t enabled,uint32_t *credit);
#endif
