function report=design_input_cascade(reuseGrid)
% Design against measured PORT input current, not an unavailable IL sensor.
% OFFLINE ONLY: no serial/debugger/firmware writes.
if nargin<1,reuseGrid=false;end
here=fileparts(mfilename('fullpath'));out=fullfile(here,'Results','input-cascade');
if ~isfolder(out),mkdir(out);end
Ti=20e-6;Tv=100e-6;s=tf('s');f=logspace(-1,log10(24900),650);z=exp(2i*pi*f*Ti);
% Start with independent corners, retaining a representative nominal case.
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
models=cell(size(specs,1),1);plants=cell(size(specs,1),1);curves=cell(size(specs,1),1);
for k=1:size(specs,1)
 p=specs(k,:);esr=.01;alpha=1/(1+esr/p(3));vo=[esr*alpha alpha];
 A=[-(p(4)+esr*alpha)/p(1) -alpha/p(1);alpha/p(2) -1/((p(3)+esr)*p(2))];B=[p(5)*p(6)/p(1);0];
 D=(36+.8+p(4)*36/p(3))/(p(5)*p(6));IL=36/p(3);
 % Iin = D*IL. Its perturbation contains BOTH D*dIL and IL*dD.
 AA=[A zeros(2);[D 0]/p(7) -1/p(7) 0;2*pi*1000*vo 0 -2*pi*1000];
 BB=[B;IL/p(7);0];
 models{k}=c2d(ss(AA,BB,[0 0 1 0;0 0 0 1],[0;0]),Ti,'zoh');
 plants{k}=models{k}(1,1)*ss(tf(1,[1 zeros(1,p(8))],Ti));
 curves{k}=squeeze(freqresp(plants{k},2*pi*f)).';
end
fprintf('Prepared %d measured-port current plants\n',numel(models));
accepted=struct([]);tested=0;
if reuseGrid
 % Cache is the previously completed1296-point INNER grid, unchanged here.
 % The selected candidate is still rechecked independently against2313cases.
 cached=jsondecode(fileread(fullfile(out,'linear-design-fast-rejected.json')));
 assert(cached.testedCurrentControllers==1296&&cached.plantCases==257);
 accepted=cached.currentCandidates.';tested=cached.testedCurrentControllers;
else
for kp=[0 .0005 .002]
for ki=[75 100 150 200 300 500]
for fp=[15 25 50 75]
for fn=[1250 1450 1650]
for zz=[.03 .1]
for zp=[.8 1.2 2]
 wn=2*pi*fn;notch=(s^2+2*zz*wn*s+wn^2)/(s^2+2*zp*wn*s+wn^2);
 Ci=c2d(ss(kp+ki/s)*ss(1/(1+s/(2*pi*fp)))*ss(notch),Ti,'tustin');[b,a]=tfdata(tf(Ci),'v');
 st=2/Ti*(z-1)./(z+1);
 cc=(kp+ki./st)./(1+st/(2*pi*fp)).*(st.^2+2*zz*wn*st+wn^2)./(st.^2+2*zp*wn*st+wn^2);
 [pm0,gm0,fc0]=margins(f,curves{1}.*cc);
 tested=tested+1;
 if pm0<50||gm0<8||fc0<50||fc0>3000,continue;end
 worstPM=pm0;worstGM=gm0;pass=true;radius=0;tracking=Inf;
 for k=1:numel(plants)
  rad=max(abs(pole(feedback(plants{k}*Ci,1))));radius=max(radius,rad);
  [pm,gm]=margins(f,curves{k}.*cc);worstPM=min(worstPM,pm);worstGM=min(worstGM,gm);
  closedResponse=curves{k}.*cc./(1+curves{k}.*cc);
  firstDrop=find(abs(closedResponse)<sqrt(.5),1);if isempty(firstDrop),track=f(end);else,track=f(firstDrop);end
  tracking=min(tracking,track);
  if rad>=1-1e-8||pm<50||gm<8||track<50||max(abs(closedResponse))>2,pass=false;break;end
 end
 if pass
  row=struct('kp',kp,'ki',ki,'poleHz',fp,'notchHz',fn,'notchZeroDamping',zz,'notchPoleDamping',zp,'b',b,'a',a,'nominalFcHz',fc0,...
   'minPM',worstPM,'minGM',worstGM,'maxPoleRadius',radius,'minimumTrackingBandwidthHz',tracking);
  if isempty(accepted),accepted=row;else,accepted(end+1)=row;end %#ok<AGROW>
 end
end
end
end
end
end
end
fprintf('Current loop: %d/%d parameter sets robustly pass\n',numel(accepted),tested);
end
report=struct('date','2026-10-27','family','Current PI plus lowpass and broad LC notch; voltage PI plus rolloff; state-space poles and contiguous tracking checked','innerSample_s',Ti,'outerSample_s',Tv,'plantCases',size(specs,1),...
 'testedCurrentControllers',tested,'passingCurrentControllers',numel(accepted),'currentCandidates',accepted,...
 'hardwareValidated',false,'flashed',false,'sensor','Average input PORT current, not IL');
if isempty(accepted)
 writeReport(report);error('No robust port-current controller: do not implement guessed gains');
end
[~,order]=sort([accepted.nominalFcHz],'descend');
% Fastest original-grid choice failed the later ESR/sensor/timing screen.
% Prefer this broader notch / lower rolloff candidate, then independently
% rerun verify_input_cascade_linear. A grid pass is never hardware approval.
preferred=find([accepted.kp]==.002 & [accepted.ki]==75 & [accepted.poleHz]==50 & ...
 [accepted.notchHz]==1250 & [accepted.notchZeroDamping]==.1 & [accepted.notchPoleDamping]==2);
assert(numel(preferred)==1,'Conservative candidate missing');
order=[preferred order(order~=preferred)];
outerAccepted=struct([]);innerSelected=[];outerSelected=[];
% Also require a feasible outer loop.
for choice=order(1:min(8,numel(order)))
 inner=accepted(choice);wn=2*pi*inner.notchHz;
 notch=(s^2+2*inner.notchZeroDamping*wn*s+wn^2)/(s^2+2*inner.notchPoleDamping*wn*s+wn^2);
 Ci=c2d(ss(inner.kp+inner.ki/s)*ss(1/(1+s/(2*pi*inner.poleHz)))*ss(notch),Ti,'tustin');
 outerPlants=cell(size(models));outerCurves=cell(size(models));
 fv=logspace(-2,log10(4990),700);zv=exp(2i*pi*fv*Tv);
 for k=1:numel(models)
  delay=specs(k,8);G=models{k}*ss(tf(1,[1 zeros(1,delay)],Ti))*ss(Ci);
  % Close ONLY the measured input-current output (output 1).
  closed=feedback(G,1,1,1);[Ad,Bd,Cd,Dd]=ssdata(closed(2,1));
  A5=Ad^5;B5=zeros(size(Bd));for q=0:4,B5=B5+Ad^q*Bd;end
  lifted=ss(A5,B5,Cd,Dd,Tv);
  % Load power FF=(Vref+Vf)*Iout/Vin, sampled by the outer loop.
  positiveFF=(36+.8)/(specs(k,5)*specs(k,3));
  outerPlants{k}=feedback(lifted,positiveFF,+1);
  outerCurves{k}=squeeze(freqresp(outerPlants{k},2*pi*fv)).';
 end
 rows=struct([]);
 for kp=[.01 .02 .05 .1 .2 .4]
 for ki=[.1 .2 .5 1 2 5 10 20]
 for fp=[50 100 200]
  Cv=c2d(ss(kp+ki/s)*ss(1/(1+s/(2*pi*fp))),Tv,'tustin');[b,a]=tfdata(tf(Cv),'v');
  cc=squeeze(freqresp(Cv,2*pi*fv)).';
  [pm0,gm0,fc0]=margins(fv,outerCurves{1}.*cc);
  if pm0<50||gm0<8||fc0<2||fc0>inner.minimumTrackingBandwidthHz/5,continue;end
  worstPM=pm0;worstGM=gm0;radius=0;pass=true;
  for k=1:numel(outerPlants)
   rad=max(abs(pole(feedback(outerPlants{k}*Cv,1))));radius=max(radius,rad);
   [pm,gm]=margins(fv,outerCurves{k}.*cc);worstPM=min(worstPM,pm);worstGM=min(worstGM,gm);
   if rad>=1-1e-8||pm<50||gm<8,pass=false;break;end
  end
  if pass
   row=struct('kp',kp,'ki',ki,'poleHz',fp,'b',b,'a',a,'nominalFcHz',fc0,'minPM',worstPM,'minGM',worstGM,'maxPoleRadius',radius);
   if isempty(rows),rows=row;else,rows(end+1)=row;end %#ok<AGROW>
  end
 end
 end
 end
 if ~isempty(rows)
  [~,idx]=max([rows.nominalFcHz]);innerSelected=inner;outerSelected=rows(idx);outerAccepted=rows;break;
 end
end
report.innerSelected=innerSelected;report.outerSelected=outerSelected;report.outerCandidates=outerAccepted;
report.linearDesignPass=~isempty(outerSelected);
writeReport(report);disp(innerSelected);disp(outerSelected);
assert(report.linearDesignPass,'No stable cascaded outer loop; design not ready');

function writeReport(value)
 fid=fopen(fullfile(out,'linear-design.json'),'w');assert(fid>=0);fprintf(fid,'%s\n',jsonencode(value,PrettyPrint=true));fclose(fid);
end
end
function [pm,gm,fc]=margins(f,response)
 mag=20*log10(abs(response));phase=unwrap(angle(response))*180/pi;pm=Inf;gm=Inf;fc=Inf;
 for k=find(mag(1:end-1).*mag(2:end)<=0)
  if mag(k+1)==mag(k),continue;end
  w=-mag(k)/(mag(k+1)-mag(k));pm=min(pm,180+phase(k)+w*(phase(k+1)-phase(k)));
  fc=min(fc,10^(log10(f(k))+w*(log10(f(k+1))-log10(f(k)))));
 end
 for target=-180:-360:-1620
  for k=find((phase(1:end-1)-target).*(phase(2:end)-target)<=0)
   if phase(k+1)==phase(k),continue;end
   w=(target-phase(k))/(phase(k+1)-phase(k));gm=min(gm,-mag(k)-w*(mag(k+1)-mag(k)));
  end
 end
 if isinf(fc),fc=0;pm=-Inf;end
end
