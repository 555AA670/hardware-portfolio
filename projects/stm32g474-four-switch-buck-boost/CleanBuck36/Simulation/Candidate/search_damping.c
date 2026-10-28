#define main simulation_main_unused
#include "simulate_candidate.c"
#undef main
int main(int argc,char **argv)
{
 CandidateParameters choices[]={
  {.003f,4,.20f,4.0f,3.6f,3.2f,32,16,8,.40f},
  {.003f,4,.20f,4.0f,3.6f,3.2f,32,16,8,.60f},
  {.003f,4,.20f,4.0f,3.6f,3.2f,32,64,32,.40f},
  {.003f,4,.20f,4.0f,3.6f,3.2f,32,128,64,.40f},
  {.003f,4,.20f,4.0f,3.6f,3.2f,32,128,64,.20f},
  {.003f,4,.20f,3.8f,3.4f,3.2f,32,64,32,.40f},
  {.003f,4,.20f,3.8f,3.4f,3.2f,32,128,64,.10f},
  {.003f,4,.20f,3.8f,3.4f,3.2f,32,128,64,.60f}
 };
 Scenario scenarios[]={
  {"apply_delay200us",15e-6,901e-6,10000,.015,.01,48,1,.8,0,1,10,1,0},
  {"apply_gain_0937",15e-6,901e-6,10000,.015,.01,48,.937,.8,0,1,1,0,0},
  {"apply_gain_090",15e-6,901e-6,10000,.015,.01,48,.90,.8,0,1,1,0,0},
  {"startup_10ohm",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,1,0,0},
  {"remove_10ohm",15e-6,901e-6,10,.015,.01,48,1,.8,0,2,1,0,0},
  {"small_LC",10.5e-6,630e-6,10,.003,.003,48,1,.8,0,0,2,1,0}
 };
 char dir[512],path[512];FILE *f,*results;int choice,k,pass,total=0;assert(argc==2);
 sprintf(path,"%s/search.csv",argv[1]);results=fopen(path,"w");assert(results);
 fprintf(results,"choice,damping_mohm,backoff,release,pass,tested\n");
 for(choice=0;choice<8;choice++){
  candidate_parameters=choices[choice];sprintf(dir,"%s/t%02d",argv[1],choice);
  sprintf(path,"%s/summary.csv",dir);f=fopen(path,"w");assert(f);
  fprintf(f,"algorithm,scenario,pass,fault,final_mv,max_mv,max_il_ma,startup_il_ma,ripple_mv,recovery_ms,pre_event_ripple_mv,final_duty_permille,event_min_mv\n");
  pass=1;
  for(k=0;k<6;k++){if(!run(&scenarios[k],1,dir,f)){pass=0;k++;break;}}
  fclose(f);total+=pass;
  fprintf(results,"%d,%ld,%lu,%lu,%d,%d\n",choice,mv(choices[choice].damping_resistance),
    (unsigned long)choices[choice].backoff,(unsigned long)choices[choice].release,pass,k);
  fflush(results);printf("DAMPING %d pass=%d checked=%d\n",choice,pass,k);
 }
 fclose(results);printf("Damping search passing %d/8 parameter sets\n",total);return 0;
}
