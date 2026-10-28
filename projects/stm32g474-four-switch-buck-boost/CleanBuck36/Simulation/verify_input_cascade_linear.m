function report=verify_input_cascade_linear
% Selected factored controller, additional ESR/sensor gain/timing uncertainty.
% No hardware IO. Sample periods and gains must match generated C header.
root=fileparts(mfilename('fullpath'));out=fullfile(root,'Results','input-cascade');
design=jsondecode(fileread(fullfile(out,'linear-design.json')));
i=design.innerSelected;v=design.outerSelected;Ti=20e-6;Tv=100e-6;s=tf('s');wn=2*pi*i.notchHz;
Ci=c2d(ss(i.kp+i.ki/s)*ss(1/(1+s/(2*pi*i.poleHz)))*...
 ss((s^2+2*i.notchZeroDamping*wn*s+wn^2)/(s^2+2*i.notchPoleDamping*wn*s+wn^2)),Ti,'tustin');
Cv=c2d(ss(v.kp+v.ki/s)*ss(1/(1+s/(2*pi*v.poleHz))),Tv,'tustin');
specs=[15e-6 901e-6 10 .015 48 1 45e-6 1];
for L=[10.5 22]*1e-6
for C=[630 1171]*1e-6
for R=[7.5 20]
for r=[.003 .03]
for Vin=[45 52]
for gain=[.9 1]
for tau=[20 100]*1e-6
for delay=[1 3]
 specs(end+1,:)=[L C R r Vin gain tau delay]; %#ok<AGROW>
end
end
end
end
end
end
end
end
fi=logspace(-1,log10(24900),1600);fv=logspace(-2,log10(4990),1400);
ci=squeeze(freqresp(Ci,2*pi*fi)).';cv=squeeze(freqresp(Cv,2*pi*fv)).';
count=0;failed=0;ipm=Inf;igm=Inf;vpm=Inf;vgm=Inf;ir=0;vr=0;firstFailure=[];
for esr=[.003 .01 .04]
for sense=[.9 1 1.1]
for k=1:size(specs,1)
 p=specs(k,:);alpha=1/(1+esr/p(3));vo=[esr*alpha alpha];
 A=[-(p(4)+esr*alpha)/p(1) -alpha/p(1);alpha/p(2) -1/((p(3)+esr)*p(2))];B=[p(5)*p(6)/p(1);0];
 D=(36+.8+p(4)*36/p(3))/(p(5)*p(6));IL=36/p(3);
 AA=[A zeros(2);sense*[D 0]/p(7) -1/p(7) 0;2*pi*1000*vo 0 -2*pi*1000];BB=[B;sense*IL/p(7);0];
 model=c2d(ss(AA,BB,[0 0 1 0;0 0 0 1],[0;0]),Ti,'zoh');
 % Add a whole 20us beyond selected delay. This conservatively covers the
 % additional 5us PWM preload latency used by the nonlinear model.
 delay=ss(tf(1,[1 zeros(1,p(8)+1)],Ti));G=model*delay;
 r1=max(abs(pole(feedback(G(1)*Ci,1))));
 [pm1,gm1]=margins(fi,squeeze(freqresp(G(1),2*pi*fi)).'.*ci);
 cl=feedback(G*Ci,1,1,1);[Ad,Bd,Cd,Dd]=ssdata(cl(2));
 A5=Ad^5;B5=zeros(size(Bd));for q=0:4,B5=B5+Ad^q*Bd;end
 lifted=ss(A5,B5,Cd,Dd,Tv);
 P=feedback(lifted,(36+.8)/(p(5)*p(3)),+1);
 P=P*ss(tf(1,[1 0 0],Tv)); % Extra 200us outer reference publication delay.
 r2=max(abs(pole(feedback(P*Cv,1))));
 [pm2,gm2]=margins(fv,squeeze(freqresp(P,2*pi*fv)).'.*cv);
 count=count+1;bad=r1>=1-1e-8||r2>=1-1e-8||pm1<50||gm1<8||pm2<50||gm2<8;
 if bad
  failed=failed+1;
  if isempty(firstFailure),firstFailure=struct('plant',p,'esr',esr,'sensorGain',sense,'innerPM',pm1,'innerGM',gm1,'outerPM',pm2,'outerGM',gm2,'innerRadius',r1,'outerRadius',r2);end
 end
 ipm=min(ipm,pm1);igm=min(igm,gm1);vpm=min(vpm,pm2);vgm=min(vgm,gm2);ir=max(ir,r1);vr=max(vr,r2);
 if mod(count,257)==0,fprintf('Expanded linear %d cases; failures %d\n',count,failed);end
end
end
end
report=struct('cases',count,'failures',failed,'minInnerPM_deg',ipm,'minInnerGM_dB',igm,'minOuterPM_deg',vpm,'minOuterGM_dB',vgm,...
 'maxInnerPoleRadius',ir,'maxOuterPoleRadius',vr,'firstFailure',firstFailure,'passed',failed==0,...
 'scope','CCM linear model; ESR3/10/40mohm, port current gain0.9/1/1.1, original257plants, one extra20us inner latency and extra200us outer publication; not measured hardware');
fid=fopen(fullfile(out,'expanded-linear.json'),'w');assert(fid>=0);fprintf(fid,'%s\n',jsonencode(report,PrettyPrint=true));fclose(fid);disp(report);
end
function [pm,gm]=margins(f,response)
mag=20*log10(abs(response));phase=unwrap(angle(response))*180/pi;pm=Inf;gm=Inf;found=false;
for k=find(mag(1:end-1).*mag(2:end)<=0)
 if mag(k+1)==mag(k),continue;end
 w=-mag(k)/(mag(k+1)-mag(k));pm=min(pm,180+phase(k)+w*(phase(k+1)-phase(k)));found=true;
end
for target=-180:-360:-1620
 for k=find((phase(1:end-1)-target).*(phase(2:end)-target)<=0)
  if phase(k+1)==phase(k),continue;end
  w=(target-phase(k))/(phase(k+1)-phase(k));gm=min(gm,-mag(k)-w*(mag(k+1)-mag(k)));
 end
end
if ~found,pm=-Inf;end
end
