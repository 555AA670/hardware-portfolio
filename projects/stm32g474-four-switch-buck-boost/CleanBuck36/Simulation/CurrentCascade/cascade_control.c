#include "cascade_control.h"
#include "cascade_coefficients.h"
#include <string.h>
static float limit(float x,float a,float b){return x<a?a:(x>b?b:x);}
void Cascade_Reset(CascadeControl *c,float output)
{
 memset(c,0,sizeof(*c));c->reference=output;c->voltage=output;
 c->vin=48;c->light_load=1;c->previous_voltage=output;
}
static uint32_t current_step(CascadeControl *c,float iin)
{
 float e,integral,pi,low,notch,total,u;
 e=c->current_reference-iin;
 integral=limit(c->current_integral+CURRENT_KI*.00001f*(e+c->current_e),-.4f,.4f);
 pi=CURRENT_KP*e+integral;
 low=CURRENT_LP_A*c->current_lp+CURRENT_LP_B*(pi+c->current_pi);
 notch=NOTCH_B0*low+NOTCH_B1*c->notch_x1+NOTCH_B2*c->notch_x2
       -NOTCH_A1*c->notch_y1-NOTCH_A2*c->notch_y2;
 total=c->feedforward+notch;
 u=limit(total,0,.9f);c->unclamped=(uint32_t)(limit(total,0,1)*27200+.5f);
 /* Upward rate bound applies to the operating command, never the burst-zero.
  * 32 counts/20us = 0.118 percentage point per current update. */
 if(u>c->applied_duty+32.0f/27200)u=c->applied_duty+32.0f/27200;
 c->current_integral=limit(integral+.05f*(u-total),-.4f,.4f);
 c->current_e=e;c->current_pi=pi;c->current_lp=low;
 c->notch_x2=c->notch_x1;c->notch_x1=low;
 c->notch_y2=c->notch_y1;c->notch_y1=notch;
 c->current_correction=notch;c->requested_duty=total;c->applied_duty=u;
 c->request=(uint32_t)(u*27200+.5f);return c->request;
}
uint32_t Cascade_CurrentOnly(CascadeControl *c,float iref,float iin,float feedforward)
{
 c->current_active=1;c->light_load=0;c->hold=0;
 c->current_reference=limit(iref,0,4);c->feedforward=feedforward;
 return current_step(c,iin);
}
uint32_t Cascade_Sample(CascadeControl *c,float measured,float vin,float iout,float iin)
{
 float e,ie,integral,pi,low,ff,total,iref;
 c->voltage+=.1180886217f*(measured-c->voltage);
 c->vin+=.1180886217f*(limit(vin,30,60)-c->vin);
 c->filtered_iout+=.1180886217f*(limit(iout,0,20)-c->filtered_iout);
 c->voltage_rate+=.5f*((measured-c->previous_voltage)*50000-c->voltage_rate);
 c->previous_voltage=measured;
 if(iout>.20f){c->light_load=0;c->low_count=0;}
 else if(iout<.10f){if(c->low_count<3)c->low_count++;if(c->low_count>=3)c->light_load=1;}
 else c->low_count=0;
 if(!c->light_load)c->hold=0;
 else if(measured>c->reference+.040f)c->hold=1;
 else if(measured<c->reference-.020f)c->hold=0;
 if(++c->divider>=5){
  c->divider=0;c->updates++;
  c->reference=limit(c->reference+.002f,0,36);
  e=c->reference-c->voltage;
  ie=e>.020f?e-.020f:(e<-.020f?e+.020f:0);
  integral=c->voltage_integral;
  if(c->light_load && c->reference>=36 && e<.2f && e>-.2f)integral*=.995f;
  integral=limit(integral+(c->hold?0:VOLTAGE_KI*.00005f*(ie+c->voltage_e)),-4,4);
  pi=VOLTAGE_KP*e+integral;
  low=VOLTAGE_LP_A*c->voltage_lp+VOLTAGE_LP_B*(pi+c->voltage_pi);
  ff=(c->reference+.8f)*limit(c->filtered_iout,0,10)/c->vin;
  total=ff+low;iref=limit(total,0,4);
  c->current_reference_limited=iref!=total;
  c->voltage_integral=limit(integral+.1f*(iref-total),-4,4);
  c->voltage_e=ie;c->voltage_pi=pi;c->voltage_lp=low;
  c->reference_queue[c->reference_index]=iref;
  c->current_reference=c->reference_queue[(c->reference_index+4-c->simulation_outer_delay)%4];
  c->reference_index=(c->reference_index+1)%4;
 }
 c->feedforward=(c->reference+.8f)/c->vin;
 c->current_active=!c->light_load;
 if(c->current_active)current_step(c,limit(iin,0,20));
 else {
  /* Burst current is unobservable as a continuous-loop DC operating point.
   * Track the feedforward here; don't wind an inner integrator against OFF. */
  c->current_integral*=.995f;c->current_e=0;c->current_pi=c->current_integral;
  c->current_lp=c->current_integral;
  c->notch_x1=c->notch_x2=c->current_integral;
  c->notch_y1=c->notch_y2=c->current_integral;
  c->applied_duty=limit(c->feedforward+c->current_integral,0,.9f);
  c->request=(uint32_t)(c->applied_duty*27200+.5f);c->unclamped=c->request;
 }
 return c->hold?0:c->request;
}
uint32_t Cascade_Output(const CascadeControl *c,uint32_t delayed_request)
{
 float brake,u;
 if(c->hold||!delayed_request)return 0;
 /* Preserve v4's bounded final fast rise brake. It has no DC duty penalty. */
 brake=limit(.60f*.000901f*limit(c->voltage_rate-800,0,100000)/c->vin,0,.05f);
 u=limit((float)delayed_request/27200-brake,0,.9f);
 return (uint32_t)(u*27200+.5f);
}
uint32_t Cascade_Pulse(uint32_t request,uint32_t enabled,uint32_t *credit)
{
 if(!enabled||!request){*credit=0;return 0;}
 if(request>24480){*credit=0;return 0xFFFFFFFFU;}
 if(request<1360){*credit+=request;if(*credit<1360)return 0;*credit-=1360;return 2016;}
 *credit=0;return request+656;
}
