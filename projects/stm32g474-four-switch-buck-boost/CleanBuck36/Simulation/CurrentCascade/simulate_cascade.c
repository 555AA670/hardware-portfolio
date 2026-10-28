/* Reuse the independently checked v4 switched LC/diode solver unchanged. */
#define main preserved_v4_model_main
#include "../Candidate/simulate_candidate.c"
#undef main
#include "cascade_control.h"

static int cascade_run(const Scenario *s,const char *directory,FILE *summary,
                       double tau,double sensorGain,double sensorBias,int currentOnly,int innerDelay)
{
 const double T=5e-6,vLSB=3.320/4095*20.113,iLSB=3.320/4095*10;
 Plant p;State x={0,0};CascadeControl c;
 uint32_t request=0,active=0,pending=0,credit=0,queue[12]={0};
 double R=s->R,vin=s->Vin,vo=0,filteredInput=0,source=0;
 double peak=0,maxv=0,tailmin=1e9,tailmax=-1e9,tailmean=0,eventmin=1e9,lastbad=3;
 double tailcurrent=0,iref=0,currentErrorMax=0;
 int n,k,tailn=0,fault=0,bad=0,offPulses=0;
 char filename[512];FILE *trace;
 plant(&p,s,R);x.v=s->initial;Cascade_Reset(&c,(float)x.v);
 if(s->delay>=5){c.simulation_outer_delay=s->delay==5?1:2;innerDelay=1;}
 sprintf(filename,"%s/cascade-%s.csv",directory,s->name);trace=fopen(filename,"w");assert(trace);
 fprintf(trace,"time_ms,vout_mv,reference_mv,iin_ma,iout_ma,iref_ma,current_error_ma,feedforward_counts,requested_counts,slew_counts,delayed_counts,final_counts,active_cmp,il_ma,current_active,iref_limited,hold,fault,sensed_iin_ma\n");
 for(n=0;n<1600000;n++){
  double t=n*T,ton,q,unused,ip=0,measured,ic;
  if(n==600000){
   if(s->event==1)R=10;if(s->event==2)R=10000;
   if(s->event==3)vin=45;if(s->event==4)R=20;
   plant(&p,s,R);
  }
  vo=voltage(&p,x);
  if(n%4==0){
   measured=floor(vo/vLSB+.5)*vLSB;if(s->noise)measured+=(n/4%2?1:-1)*vLSB;
   ic=floor((filteredInput*sensorGain+sensorBias)/iLSB+.5)*iLSB;
   if(currentOnly==4&&t>=3)ic=8.5; /* ADC fault injection, not real power. */
   if(currentOnly==5&&t>=3)measured=39.5;
   if(!fault){
    if(currentOnly==1||currentOnly==3){
     iref=t<1?2*t:(t<3?2:2.5);
     /* Negative control: removing measured current must destroy regulation. */
     request=Cascade_CurrentOnly(&c,(float)iref,(float)(currentOnly==3?0:ic),.65f);
    }else request=Cascade_Sample(&c,(float)measured,(float)vin,
               (float)(floor((vo/R)/iLSB+.5)*iLSB),(float)ic);
    /* Guard uses the same observed ADC current, never hidden plant IL/Iin.
     * Gain/offset uncertainty therefore affects BOTH loop and sampled trip. */
    if(measured>=39)fault=3;if(ic>=8)fault=2;
    if(!currentOnly&&t>=5&&measured<35.5)fault=12;
   }
   for(k=11;k>0;k--)queue[k]=queue[k-1];queue[0]=request;
  }
  active=pending;
  /* Inner command latency is independently swept at 20..60us. Scenario's
   * old outer delay is applied to reference publication in a separate test,
   * not falsely called an inner-loop timing margin. */
  c.final=(currentOnly==1||currentOnly==3)?queue[innerDelay]:Cascade_Output(&c,queue[innerDelay]);
  pending=Cascade_Pulse(c.final,!fault,&credit);assert(pending!=0xFFFFFFFFU);
  if(fault){active=0;if(pending)offPulses++;}
  ton=active?(active-656.0)/27200*T:0;assert(ton>=0&&ton<=.9*T+1e-12);
  x=segment(&p,x,ton,vin*s->gain-s->vf,&q,&ip);
  x=segment(&p,x,T-ton,-s->vf-.04,&unused,&ip);
  source=q/T;filteredInput+=(1-exp(-T/tau))*(source-filteredInput);
  vo=voltage(&p,x);if(ip>peak)peak=ip;if(vo>maxv)maxv=vo;
  if(t>=3&&fabs(vo-36)>.36)lastbad=t;
  if(t>=3&&t<3.3&&vo<eventmin)eventmin=vo;
  if(t>=7.5){
   if(vo<tailmin)tailmin=vo;if(vo>tailmax)tailmax=vo;
   tailmean+=vo;tailcurrent+=filteredInput;tailn++;
   if(fabs(filteredInput-iref)>currentErrorMax)currentErrorMax=fabs(filteredInput-iref);
  }
  if(n%200==0)fprintf(trace,"%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%lu,%lu,%lu,%lu,%lu,%ld,%lu,%lu,%lu,%d,%ld\n",
   mv(t),mv(vo),mv(c.reference),mv(filteredInput),mv(vo/R),mv(c.current_reference),mv(c.current_reference-ic),
   mv(c.feedforward*27.2),(unsigned long)c.unclamped,(unsigned long)c.request,
   (unsigned long)queue[innerDelay],(unsigned long)c.final,(unsigned long)active,mv(x.i),
   (unsigned long)c.current_active,(unsigned long)c.current_reference_limited,(unsigned long)c.hold,fault,mv(ic));
 }
 fclose(trace);tailmean/=tailn;tailcurrent/=tailn;
 if(currentOnly==4||currentOnly==5)bad=!(fault==(currentOnly==4?2:3)&&active==0&&offPulses==0);
 else if(currentOnly==3)bad=!(fault==3&&active==0&&maxv>38&&offPulses==0);
 else if(currentOnly==2)bad=fault||tailcurrent<3.9||tailcurrent>4.1||tailmean<25||tailmean>35||!c.current_reference_limited||peak>8||offPulses;
 else if(currentOnly==1)bad=fault||currentErrorMax>.05||peak>8||offPulses;
 else if(s->impossible)bad=!(fault==12&&active==0);
 else bad=fault||fabs(tailmean-36)>.10||tailmax-tailmin>.25||maxv>37||peak>8||offPulses;
 if(!currentOnly&&!s->impossible&&s->event&&(lastbad>3.3||eventmin<32.4))bad=1;
 fprintf(summary,"cascade,%s,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
  s->name,!bad,fault,mv(tailmean),mv(maxv),mv(peak),mv(tailmax-tailmin),mv(lastbad-3),mv(eventmin),
  mv(tailcurrent),mv(iref),mv(currentErrorMax),(long)offPulses,mv(tau*1000000),mv(sensorGain));
 printf("cascade %-28s pass=%d fault=%d final=%ldmV max=%ldmV peak=%ldmA ripple=%ldmV recover=%ldms Iin=%ldmA\n",
  s->name,!bad,fault,mv(tailmean),mv(maxv),mv(peak),mv(tailmax-tailmin),mv(lastbad-3),mv(tailcurrent));
 return !bad;
}
int main(int argc,char **argv)
{
 Scenario cases[]={
  {"startup_10ohm",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,1,0,0},
  {"startup_20ohm",15e-6,901e-6,20,.015,.01,48,1,.8,0,0,1,0,0},
  {"startup_noload",15e-6,901e-6,10000,.015,.01,48,1,.8,0,0,1,0,0},
  {"apply_10ohm",15e-6,901e-6,10000,.015,.01,48,1,.8,0,1,1,0,0},
  {"remove_10ohm",15e-6,901e-6,10,.015,.01,48,1,.8,0,2,1,0,0},
  {"input_48to45",15e-6,901e-6,10,.015,.01,48,1,.8,0,3,1,0,0},
  {"load_10to20",15e-6,901e-6,10,.015,.01,48,1,.8,0,4,1,0,0},
  {"small_LC",10.5e-6,630e-6,10,.003,.003,48,1,.8,0,0,2,1,0},
  {"large_LC",22e-6,1171e-6,10,.03,.04,48,1,.8,0,0,2,1,0},
  {"actuation_gain_090",15e-6,901e-6,10,.015,.01,48,.90,.8,0,0,2,0,0},
  {"actuation_gain_0937",15e-6,901e-6,10,.015,.01,48,.937,.8,0,0,2,0,0},
  {"inner_delay_60us",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,3,1,0},
  {"prebias_060",15e-6,901e-6,10,.015,.01,48,1,.8,.6,0,1,0,0},
  {"noload_ADC_noise",15e-6,901e-6,10000,.015,.01,48,1,.8,0,0,2,1,0},
  {"apply_gain_0937",15e-6,901e-6,10000,.015,.01,48,.937,.8,0,1,1,0,0},
  {"apply_gain_090",15e-6,901e-6,10000,.015,.01,48,.90,.8,0,1,1,0,0},
  {"apply_inner60us",15e-6,901e-6,10000,.015,.01,48,1,.8,0,1,3,1,0},
  {"outer_delay100us",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,5,1,0},
  {"outer_delay200us",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,10,1,0},
  {"apply_outer200us",15e-6,901e-6,10000,.015,.01,48,1,.8,0,1,10,1,0},
  {"infeasible",15e-6,901e-6,10,1.4,.01,45,.85,.8,0,0,2,0,1}
 };
 int k,passed=0,total=0;Scenario s;char file[512];FILE *summary;
 assert(argc==2);sprintf(file,"%s/summary.csv",argv[1]);summary=fopen(file,"w");assert(summary);
 fprintf(summary,"algorithm,scenario,pass,fault,final_mv,max_mv,max_il_ma,ripple_mv,recovery_ms,event_min_mv,final_iin_ma,iref_ma,current_error_max_ma,off_pulses,tau_ns,sensor_gain_permille\n");
 for(k=0;k<(int)(sizeof(cases)/sizeof(cases[0]));k++){
  passed+=cascade_run(&cases[k],argv[1],summary,45e-6,1,0,0,cases[k].delay);total++;
 }
 s=cases[0];s.name="sensor_tau100us";passed+=cascade_run(&s,argv[1],summary,100e-6,1,0,0,3);total++;
 s.name="sensor_gain_090";passed+=cascade_run(&s,argv[1],summary,45e-6,.9,0,0,1);total++;
 s.name="sensor_gain_110";passed+=cascade_run(&s,argv[1],summary,45e-6,1.1,0,0,1);total++;
 s.name="sensor_offset_024A";passed+=cascade_run(&s,argv[1],summary,45e-6,1,.024,0,1);total++;
 s.name="current_only_2to2p5A";passed+=cascade_run(&s,argv[1],summary,45e-6,1,0,1,1);total++;
 s.name="current_only_tau100us";passed+=cascade_run(&s,argv[1],summary,100e-6,1,0,1,3);total++;
 s.name="current_feedback_disconnected";passed+=cascade_run(&s,argv[1],summary,45e-6,1,0,3,1);total++;
 s=cases[0];s.name="current_limit_5ohm";s.R=5;passed+=cascade_run(&s,argv[1],summary,45e-6,1,0,2,1);total++;
 s=cases[0];s.name="inject_current_trip";passed+=cascade_run(&s,argv[1],summary,45e-6,1,0,4,1);total++;
 s.name="inject_voltage_trip";passed+=cascade_run(&s,argv[1],summary,45e-6,1,0,5,1);total++;
 fclose(summary);printf("Cascade nonlinear checks %d/%d passing (failures retained)\n",passed,total);return 0;
}
