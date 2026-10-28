#include "cascade_control.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
int main(int argc,char **argv)
{
 CascadeControl c;uint32_t r,credit,p,j,checks=0;unsigned long charge;
 FILE *f;assert(argc==2);f=fopen(argv[1],"w");assert(f);
 fprintf(f,"sample,error_nanoamp,correction_picoduty\n");
 Cascade_Reset(&c,36);c.applied_duty=.65f;
 for(j=0;j<10000;j++){
  float ref=j==0?2.01f:2.0f;
  Cascade_CurrentOnly(&c,ref,2,.65f);
  fprintf(f,"%lu,%ld,%ld\n",(unsigned long)j,(long)((ref-2)*1e9),
      (long)(c.current_correction*1e12f));
  assert(c.request<=24480);checks++;
 }
 fclose(f);
 /* Charge conservation below minimum pulse, plus no pulses while disabled. */
 for(r=0;r<=24480;r++){
  credit=0;charge=0;
  for(j=0;j<100;j++){
   p=Cascade_Pulse(r,1,&credit);
   assert(!p||(p>=2016&&p<=25136));if(p)charge+=p-656;
   assert(credit<1360);checks+=2;
  }
  assert(charge+credit==100*r);checks++;
  assert(Cascade_Pulse(r,0,&credit)==0&&credit==0);checks++;
 }
 credit=123;assert(Cascade_Pulse(24481,1,&credit)==0xFFFFFFFFU&&credit==0);checks++;
 /* Stop/hold overrides any delayed nonzero command. */
 Cascade_Reset(&c,36);c.hold=1;assert(Cascade_Output(&c,24480)==0);checks++;
 printf("Cascade invariants passed: %lu checks\n",(unsigned long)checks);return 0;
}
