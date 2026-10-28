#define main simulation_main_unused
#include "simulate_candidate.c"
#undef main
int main(int argc,char **argv)
{
 Scenario cases[]={
  {"apply_gain_0937",15e-6,901e-6,10000,.015,.01,48,.937,.8,0,1,1,0,0},
  {"apply_gain_090",15e-6,901e-6,10000,.015,.01,48,.90,.8,0,1,1,0,0},
  {"apply_delay200us",15e-6,901e-6,10000,.015,.01,48,1,.8,0,1,10,1,0}
 };
 char file[512];FILE *f;int k,pass=0;assert(argc==2);
 sprintf(file,"%s/stress-summary.csv",argv[1]);f=fopen(file,"w");assert(f);
 fprintf(f,"algorithm,scenario,pass,fault,final_mv,max_mv,max_il_ma,startup_il_ma,ripple_mv,recovery_ms,pre_event_ripple_mv,final_duty_permille,event_min_mv\n");
 for(k=0;k<3;k++)pass+=run(&cases[k],1,argv[1],f);
 fclose(f);printf("Stress cases passing %d/3\n",pass);return 0;
}
