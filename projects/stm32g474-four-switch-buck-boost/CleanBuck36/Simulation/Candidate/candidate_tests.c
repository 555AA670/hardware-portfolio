#include "candidate_control.h"
#include "bench_config.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main(void)
{
 CandidateControl c;unsigned long checks=0;uint32_t req,n,credit;
 for(req=0;req<=CANDIDATE_MAX_EFFECTIVE_COUNTS;req++){
  unsigned long energy=0;credit=0;
  for(n=0;n<100;n++){
   uint32_t cmp=Candidate_Pulse(req,1,&credit);
   assert(cmp==0 || (cmp>=2016&&cmp<=25136));
   if(cmp)energy+=cmp-656;
   assert(credit<1360);++checks;
  }
  if(req<1360){assert(energy<=100*req && 100*req-energy<1360);}
  else assert(energy==100*req);
  assert(Candidate_Pulse(req,0,&credit)==0&&credit==0);++checks;
 }
 assert(Candidate_Pulse(24481,1,&credit)==0xFFFFFFFFU);
 Candidate_Reset(&c,36);
 /* This invariant concerns a settled operating point, not a 36V prebiased
  * cold start with the command artificially forced to zero. */
 c.command=20853U;c.pi.request=c.command;
 for(n=0;n<1000000;n++){
  Candidate_Sample(&c,36.0f,48.0f,0,0);
  assert(c.pi.request<=24480 && c.pi.integral==c.pi.integral);
 }
 assert(fabs(c.pi.integral)<.001f);++checks;
 /* Fast inhibit must not wait for the slower 10kHz update. */
 assert(Candidate_Sample(&c,36.1f,48.0f,0,0)==0&&c.hold);++checks;
 Candidate_Sample(&c,35.9f,48,0,0);assert(!c.hold);++checks;
 Candidate_Reset(&c,0);
 for(n=0;n<500000;n++){
  uint32_t old=c.command;Candidate_Sample(&c,0,48,1,0);
  assert(c.command<=old+32 && c.command<=24480);++checks;
 }
 assert(c.command==24480);++checks;
 printf("%lu candidate unit/allocator checks passed; no hardware interfaces\n",checks);
 return 0;
}
