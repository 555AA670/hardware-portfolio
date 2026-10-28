/* Execute old target C and new candidate C against an event-driven switched
 * LC + Q3 diode model. No serial, debugger or board interfaces. */
#include "candidate_control.h"
#include "bench_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct {double i,v;} State;
typedef struct {
 const char *name;double L,C,R,r,esr,Vin,gain,vf,initial;
 int event,delay,noise,impossible;
} Scenario;
typedef struct {double a,b,c,d,half,omega,discriminant,alpha,R,L,C,r,esr;} Plant;
static void plant(Plant *p,const Scenario *s,double R)
{
 p->L=s->L;p->C=s->C;p->R=R;p->r=s->r;p->esr=s->esr;
 p->alpha=1/(1+s->esr/R);
 p->a=-(s->r+s->esr*p->alpha)/s->L;p->b=-p->alpha/s->L;
 p->c=p->alpha/s->C;p->d=-1/((R+s->esr)*s->C);
 p->half=(p->a+p->d)/2;
 p->discriminant=p->a*p->d-p->b*p->c-p->half*p->half;
 p->omega=sqrt(fabs(p->discriminant));
}
static double voltage(const Plant *p,State x){return p->alpha*(x.v+p->esr*x.i);}
static State evolve(const Plant *p,State x,double dt,double U)
{
 double is=U/(p->R+p->r),vs=p->R*is,ei=x.i-is,ev=x.v-vs;
 double q=p->omega*dt,ss,cc,ex=exp(p->half*dt);
 if(p->discriminant>=0){ss=sin(q)/p->omega;cc=cos(q);}
 else{ss=sinh(q)/p->omega;cc=cosh(q);}
 State y;
 y.i=is+ex*((cc+(p->a-p->half)*ss)*ei+p->b*ss*ev);
 y.v=vs+ex*(p->c*ss*ei+(cc+(p->d-p->half)*ss)*ev);
 return y;
}
static State segment(const Plant *p,State x,double dt,double U,double *charge,double *peak)
{
 State y,mid;double active,lo,hi;int j;
 *charge=0;if(dt<=0)return x;
 if(x.i<1e-10&&U<=voltage(p,x)){
  y.i=0;y.v=x.v*exp(-dt/((p->R+p->esr)*p->C));return y;
 }
 y=evolve(p,x,dt,U);active=dt;
 if(y.i<0){
  lo=0;hi=dt;
  for(j=0;j<22;j++){
   active=(lo+hi)/2;mid=evolve(p,x,active,U);
   if(mid.i>0)lo=active;else hi=active;
  }
  active=(lo+hi)/2;y=evolve(p,x,active,U);y.i=0;
 }
 mid=evolve(p,x,active/2,U);
 if(x.i>*peak)*peak=x.i;if(mid.i>*peak)*peak=mid.i;if(y.i>*peak)*peak=y.i;
 /* Simpson quadrature is only a source-current model, not an ADC waveform. */
 *charge=active*(x.i+4*mid.i+y.i)/6;
 if(active<dt)y.v*=exp(-(dt-active)/((p->R+p->esr)*p->C));
 assert(y.i>=-1e-6 && y.v==y.v);
 return y;
}
static long mv(double x){return (long)(x*1000+(x>=0?.5:-.5));}
static int run(const Scenario *s,int candidate,const char *directory,FILE *summary)
{
 const double T=5e-6,adcLSB=3.320/4095*20.113;
 Plant p;State x={0,0};CandidateControl c;CleanControl old;
 uint32_t pending=0,active=0,credit=0,request=0,queue[12]={0};
 double R=s->R,vin=s->Vin,vo,source=0,peak=0,startpeak=0,maxv=0,ref=0;
 double tailmin=1e9,tailmax=-1e9,tailmean=0,windowmin=1e9,windowmax=-1e9;
 double lastbad=3.0,iref=0,med=0,filteredInput=0,eventmin=1e9;
 int n,k,fault=0,bad=0,tailn=0,emittedOff=0,exceed=0,allowed;
 char filename[512];FILE *trace;
 x.v=s->initial;plant(&p,s,R);
 CleanControl_Reset(&old,(float)x.v);Candidate_Reset(&c,(float)x.v);
 sprintf(filename,"%s/%s-%s.csv",directory,candidate?"candidate":"baseline",s->name);
 trace=fopen(filename,"w");assert(trace);
 fprintf(trace,"time_ms,vout_mv,reference_mv,request_counts,active_cmp,source_ma,il_ma,hold,fault\n");
 for(n=0;n<1600000;n++){
  double t=n*T,ton,q,unused,currentPeak=0;
  if(n==600000){
   if(s->event==1)R=10;
   if(s->event==2)R=10000;
   if(s->event==3)vin=45;
   if(s->event==4)R=20;
   plant(&p,s,R);
  }
  vo=voltage(&p,x);
  /* 50kHz ADC with deterministic LSB alternation; no random favorable seed. */
  if(n%4==0){
   med=floor(vo/adcLSB+.5)*adcLSB;
   if(s->noise)med+=(n/4%2?1:-1)*adcLSB;
   if(!fault){
    request=candidate?Candidate_Sample(&c,(float)med,(float)vin,(float)(vo/R+.024),(float)(filteredInput+.024)):CleanControl_Sample(&old,(float)med);
    ref=candidate?c.pi.reference:old.reference;
    if(med>=39.0)fault=3;
    if(filteredInput>=8.0)fault=2;
    if(t>=5 && med<35.5)fault=12; /* acquire-failure test, not shipped protection */
   }
   for(k=11;k>0;k--)queue[k]=queue[k-1];queue[0]=request;
  }
  /* Request publication delay plus one complete-cycle preload latency. */
  allowed=!fault;active=pending;
  {uint32_t r=queue[s->delay];
   if(candidate)r=Candidate_Output(&c,r);
   pending=candidate?Candidate_Pulse(r,allowed,&credit):CleanPulse_Next(r,allowed,&credit);}
  assert(pending!=0xFFFFFFFFU);
  if(!allowed){active=0;if(pending)emittedOff++;}
  ton=active?(active-656.0)/27200*T:0;
  assert(ton>=0 && ton<=(candidate?.90:.85)*T+1e-12);
  x=segment(&p,x,ton,vin*s->gain-s->vf,&q,&currentPeak);
  x=segment(&p,x,T-ton,-s->vf-.04,&unused,&currentPeak);
  source=q/T;filteredInput+=(1-exp(-T/45e-6))*(source-filteredInput);
  vo=voltage(&p,x);
  if(currentPeak>peak)peak=currentPeak;
  if(t<2.8&&currentPeak>startpeak)startpeak=currentPeak;
  if(vo>maxv)maxv=vo;
  if(t>=3 && fabs(vo-36)>.36)lastbad=t;
  if(t>=3&&t<3.3&&vo<eventmin)eventmin=vo;
  if(t>=2.5&&t<3){if(vo<windowmin)windowmin=vo;if(vo>windowmax)windowmax=vo;}
  if(t>=7.5){if(vo<tailmin)tailmin=vo;if(vo>tailmax)tailmax=vo;tailmean+=vo;tailn++;}
  if(n%200==0)fprintf(trace,"%ld,%ld,%ld,%lu,%lu,%ld,%ld,%lu,%d\n",
   mv(t),mv(vo),mv(ref),(unsigned long)request,(unsigned long)active,
   mv(filteredInput),mv(x.i),(unsigned long)(candidate?c.hold:0),fault);
 }
 fclose(trace);tailmean/=tailn;iref=tailmax-tailmin;
 if(s->impossible)bad=!(fault==12&&active==0);
 else bad=fault||fabs(tailmean-36)>.10||iref>.25||maxv>37.0||peak>8.0||emittedOff;
 if(!s->impossible&&s->event&&lastbad>3.3)bad=1;
 if(!s->impossible&&s->event&&eventmin<32.4)bad=1;
 fprintf(summary,"%s,%s,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
  candidate?"candidate":"baseline",s->name,!bad,fault,mv(tailmean),mv(maxv),mv(peak),
  mv(startpeak),mv(iref),mv(lastbad-3),mv(windowmax-windowmin),mv(request/27200.0),mv(eventmin));
 printf("%s %-28s pass=%d fault=%d final=%ldmV max=%ldmV ILmax=%ldmA ripple=%ldmV recovery=%ldms\n",
  candidate?"candidate":"baseline",s->name,!bad,fault,mv(tailmean),mv(maxv),mv(peak),mv(iref),mv(lastbad-3));
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
  {"delay_100us",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,5,1,0},
  {"delay_200us",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,10,1,0},
  {"prebias_060",15e-6,901e-6,10,.015,.01,48,1,.8,.6,0,1,0,0},
  {"noload_ADC_noise",15e-6,901e-6,10000,.015,.01,48,1,.8,0,0,2,1,0},
  {"apply_gain_0937",15e-6,901e-6,10000,.015,.01,48,.937,.8,0,1,1,0,0},
  {"apply_gain_090",15e-6,901e-6,10000,.015,.01,48,.90,.8,0,1,1,0,0},
  {"apply_delay200us",15e-6,901e-6,10000,.015,.01,48,1,.8,0,1,10,1,0},
  {"infeasible",15e-6,901e-6,10,1.4,.01,45,.85,.8,0,0,2,0,1}
 };
 int k,mode,count=sizeof(cases)/sizeof(cases[0]),passed=0;FILE *summary;char file[512];
 assert(argc==2);sprintf(file,"%s/summary.csv",argv[1]);summary=fopen(file,"w");assert(summary);
 fprintf(summary,"algorithm,scenario,pass,fault,final_mv,max_mv,max_il_ma,startup_il_ma,ripple_mv,recovery_ms,pre_event_ripple_mv,final_duty_permille,event_min_mv\n");
 for(mode=0;mode<2;mode++)for(k=0;k<count;k++){
  int good=run(&cases[k],mode,argv[1],summary);if(mode)passed+=good;
 }
 fclose(summary);printf("Candidate model cases passing %d/%d\n",passed,count);
 return 0; /* MATLAB consumes every pass/fail; failed cases are never hidden. */
}
