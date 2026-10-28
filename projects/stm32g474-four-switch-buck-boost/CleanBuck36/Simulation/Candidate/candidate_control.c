#include "candidate_control.h"
#include "bench_config.h"
CandidateParameters candidate_parameters={.003f,4.0f,.20f,4.0f,3.6f,3.2f,32U,16U,8U,.60f};
static float Bound(float x,float lo,float hi){return x<lo?lo:(x>hi?hi:x);}
void Candidate_Reset(CandidateControl *c,float output)
{
 CleanControl_Reset(&c->pi,output);
 c->hold=c->updates=c->prelimit=c->rise_limited=c->command=0;
 c->filtered_vin=48.0f;c->feedforward=0;
 c->light_load=1;c->low_load_count=0;
 c->current_cap=CANDIDATE_MAX_EFFECTIVE_COUNTS;c->soft_current_active=0;
 c->previous_voltage=output;c->voltage_rate=0;
}
uint32_t Candidate_Sample(CandidateControl *c,float measured,float vin,float iout,float iin)
{
 CleanControl *p=&c->pi;
 float e,ie,integral,pi,raw,total,u,correction;uint32_t request;
 p->filtered+=0.1180886217f*(measured-p->filtered);
 c->filtered_vin+=.1180886217f*(Bound(vin,30.0f,60.0f)-c->filtered_vin);
 c->voltage_rate+=.50f*((measured-c->previous_voltage)*50000.0f-c->voltage_rate);
 c->previous_voltage=measured;
 /* Port-current soft constraint, not a claimed inductor peak/current loop.
  * Fast 50kHz cap is separate from the slower voltage-command pipeline. */
 if(iin>candidate_parameters.soft_high){
  if(c->current_cap>c->command)c->current_cap=c->command;
  c->current_cap=c->current_cap>candidate_parameters.backoff?c->current_cap-candidate_parameters.backoff:0U;
 }else if(iin>candidate_parameters.soft_hold){if(c->current_cap>c->command)c->current_cap=c->command;}
 else if(iin<candidate_parameters.soft_release){
  c->current_cap+=candidate_parameters.release;if(c->current_cap>CANDIDATE_MAX_EFFECTIVE_COUNTS)c->current_cap=CANDIDATE_MAX_EFFECTIVE_COUNTS;
 }
 c->soft_current_active=c->current_cap<c->command;
 /* At 50kHz, discard energy request as soon as output exceeds ramp reference.
  * Hysteresis exceeds ADC quantization; zero means CMP=0, NOT 656 counts. */
 /* Port Iout only selects light-load pulse inhibition, NOT an inductor
  * current loop or zero-crossing detector. Three low samples reject a glitch. */
 if(iout>.20f){c->light_load=0;c->low_load_count=0;}
 else if(iout<.10f){if(c->low_load_count<3)c->low_load_count++;if(c->low_load_count>=3)c->light_load=1;}
 else c->low_load_count=0;
 if(!c->light_load)c->hold=0;
 else if(measured>p->reference+0.040f)c->hold=1U;
 else if(measured<p->reference-0.020f)c->hold=0U;
 if(++p->divider<5U){
  p->request=c->command<c->current_cap?c->command:c->current_cap;
  if(c->hold)p->request=0;return p->request;
 }
 p->divider=0;c->updates++;
 p->reference=Bound(p->reference+0.002f,0.0f,36.0f);
 e=p->reference-p->filtered;
 /* ~16mV/LSB at this board's divider: don't integrate an unattainable ADC
  * fraction forever at no load. Proportional response is not disabled. */
 ie=e>0.020f?e-0.020f:(e<-.020f?e+0.020f:0.0f);
 integral=p->integral;
 /* No-load pulse density cannot identify the CCM balance duty. Slowly remove
  * stale startup correction near target, rather than carrying a negative
  * correction into the first real load step. No discontinuous PI reset. */
 if(c->light_load && p->reference>=36.0f && e<.20f && e>-.20f)integral*=.995f;
 integral=Bound(integral+(c->hold?0.0f:candidate_parameters.ki*.00005f*(ie+p->previous_e)),-.30f,.30f);
 pi=candidate_parameters.kp*e+integral;
 raw=.8277396009f*p->previous_u+.08613019954f*(pi+p->previous_pi);
 /* Small load-current feedforward: resistance is a DESIGN compensation,
  * not identified inductor DCR. PI removes its steady-state error. */
 c->feedforward=(p->reference+.8f+candidate_parameters.rff*Bound(iout,0.0f,10.0f))/c->filtered_vin;
 total=c->feedforward+raw;
 u=Bound(total,0.0f,(float)CANDIDATE_MAX_EFFECTIVE_COUNTS/27200.0f);
 c->prelimit=(uint32_t)(u*27200.0f+.5f);
 request=(uint32_t)(u*27200.0f+.5f);
 /* Slew the operating command, not zeroed burst output. The ramped reference
  * changes FF smoothly; releasing voltage hold reuses an established pulse
  * width rather than spending 250ms ramping all the way from zero again. */
 if(request>c->command+candidate_parameters.slew)request=c->command+candidate_parameters.slew;
 c->command=request;
 if(request>c->current_cap)request=c->current_cap;
 u=(float)request/27200.0f;
 correction=u-c->feedforward;
 p->integral=Bound(integral+.25f*(u-total),-.30f,.30f);
 p->previous_pi=candidate_parameters.kp*e+p->integral;p->previous_u=correction;p->previous_e=ie;
 c->rise_limited=request;p->request=c->hold?0U:request;
 return p->request;
}
uint32_t Candidate_Pulse(uint32_t request,uint32_t enabled,uint32_t *credit)
{
 if(!enabled||request==0U){*credit=0;return 0;}
 if(request>CANDIDATE_MAX_EFFECTIVE_COUNTS){*credit=0;return 0xFFFFFFFFU;}
 if(request<BENCH_MIN_EFFECTIVE_COUNTS){
  *credit+=request;if(*credit<BENCH_MIN_EFFECTIVE_COUNTS)return 0;
  *credit-=BENCH_MIN_EFFECTIVE_COUNTS;return BENCH_MIN_EFFECTIVE_COUNTS+656U;
 }
 *credit=0;return request+656U;
}
uint32_t Candidate_Output(const CandidateControl *c,uint32_t delayed_request)
{
 float damping,u;uint32_t request;
 if(c->hold||!delayed_request)return 0;
 /* Fast capacitor-current estimate C*dVout/dt adds one-sided braking, not a
  * measured inductor-current loop. Bounded to 5% duty reduction, no DC loss. */
 /* One-sided braking avoids a delayed negative-slope boost at load turn-on.
  * 800V/s deadband exceeds the modeled +/-1LSB alternating-noise slope. */
 damping=Bound(candidate_parameters.damping_resistance*.000901f*
               Bound(c->voltage_rate-800.0f,0.0f,100000.0f)/c->filtered_vin,0.0f,.05f);
 u=Bound((float)delayed_request/27200.0f-damping,0.0f,.90f);
 request=(uint32_t)(u*27200.0f+.5f);
 return request>c->current_cap?c->current_cap:request;
}
