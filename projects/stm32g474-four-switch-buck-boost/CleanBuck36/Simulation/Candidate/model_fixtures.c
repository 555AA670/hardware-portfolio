#define main simulation_main_unused
#include "simulate_candidate.c"
#undef main
int main(int argc,char **argv)
{
 Scenario s={"fixture",15e-6,901e-6,10,.015,.01,48,1,.8,0,0,1,0,0};
 Plant p;State x,y;FILE *f;int a,b,c,d;assert(argc==2);f=fopen(argv[1],"w");assert(f);
 fprintf(f,"loss_mohm,i0_ma,v0_mv,dt_ns,input_mv,i1_ua,v1_uv\n");
 for(a=0;a<2;a++)for(b=0;b<3;b++)for(c=0;c<3;c++)for(d=0;d<4;d++){
  double dt=d%2?5e-6:250e-9,U=d<2?47.2:-.84;
  s.r=a?1.4:.015;plant(&p,&s,10);x.i=b*2.0;x.v=c*18.0;y=evolve(&p,x,dt,U);
  fprintf(f,"%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",mv(s.r),mv(x.i),mv(x.v),
   (long)(dt*1e9+.5),mv(U),(long)(y.i*1e6),(long)(y.v*1e6));
 }
 fclose(f);return 0;
}
