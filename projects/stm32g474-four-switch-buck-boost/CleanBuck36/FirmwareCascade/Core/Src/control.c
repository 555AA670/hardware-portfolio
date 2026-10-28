#include "control.h"
#include "bench_config.h"
/* New PI +300Hz Tustin pole,50kHz measurement/10kHz control. No old dyn,
 * current-backoff writer, feedforward floor, loaded skip or resume reset. */
void CleanControl_Reset(CleanControl *c,float output)
{
 c->integral=c->previous_pi=c->previous_u=c->previous_e=0.0f;
 c->filtered=output;
 c->reference=output<0.0f?0.0f:(output>36.0f?36.0f:output);
 c->divider=0U;c->request=0U;
}
uint32_t CleanControl_Sample(CleanControl *c,float measured)
{
 float e,candidate,p,unbounded,u;
 c->filtered+=0.1180886217f*(measured-c->filtered);
 if (++c->divider<5U) return c->request;
 c->divider=0U;c->reference+=0.002f;if(c->reference>36.0f)c->reference=36.0f;
 e=c->reference-c->filtered;
 candidate=c->integral+0.0002f*(e+c->previous_e);
 p=0.001f*e+candidate;
 unbounded=0.8277396009f*c->previous_u+0.08613019954f*(p+c->previous_pi);
 u=unbounded;if(u<0.0f)u=0.0f;if(u>0.85f)u=0.85f;
 c->integral=candidate+0.25f*(u-unbounded);
 c->previous_pi=0.001f*e+c->integral;c->previous_u=u;c->previous_e=e;
 c->request=(uint32_t)(u*27200.0f+0.5f);
 return c->request;
}
uint32_t CleanPulse_Next(uint32_t request,uint32_t enabled,uint32_t *credit)
{
 if(!enabled || request==0U){*credit=0U;return 0U;}
 if(request>BENCH_MAX_EFFECTIVE_COUNTS){*credit=0U;return 0xFFFFFFFFU;}
 if(request<BENCH_MIN_EFFECTIVE_COUNTS){
  *credit+=request;
  if(*credit<BENCH_MIN_EFFECTIVE_COUNTS)return 0U;
  *credit-=BENCH_MIN_EFFECTIVE_COUNTS;
  return BENCH_MIN_EFFECTIVE_COUNTS+BENCH_DEAD_PWM_COUNTS;
 }
 *credit=0U;return request+BENCH_DEAD_PWM_COUNTS;
}
