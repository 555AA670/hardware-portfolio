#ifndef INPUT_CURRENT_CASCADE_H
#define INPUT_CURRENT_CASCADE_H
#include <stdint.h>
/* OFFLINE CANDIDATE. Inputs are zero-corrected PORT currents in amperes.
 * No measured IL is available. This is not a cycle-by-cycle peak limiter. */
typedef struct {
 float reference,voltage,vin,voltage_integral,voltage_e,voltage_pi,voltage_lp;
 float current_reference,current_integral,current_e,current_pi,current_lp;
 float notch_x1,notch_x2,notch_y1,notch_y2;
 float feedforward,current_correction,previous_voltage,voltage_rate,filtered_iout;
 float requested_duty,applied_duty;
 uint32_t divider,hold,light_load,low_count,current_active,current_reference_limited;
 uint32_t request,unclamped,final,updates;
 /* Offline delay injection only; zero in the intended implementation. */
 uint32_t simulation_outer_delay,reference_index;
 float reference_queue[4];
} CascadeControl;
void Cascade_Reset(CascadeControl *c,float output);
uint32_t Cascade_Sample(CascadeControl *c,float measured,float vin,float iout,float iin);
/* Current-only test: same inner controller, externally supplied ampere reference. */
uint32_t Cascade_CurrentOnly(CascadeControl *c,float iref,float iin,float feedforward);
uint32_t Cascade_Output(const CascadeControl *c,uint32_t delayed_request);
uint32_t Cascade_Pulse(uint32_t request,uint32_t enabled,uint32_t *credit);
#endif
