function report=validate_input_cascade
% Final report from the native C run and independent linear screening.
root=fileparts(mfilename('fullpath'));folder=fullfile(root,'Results','input-cascade');
native=fullfile(folder,'nonlinear');
design=jsondecode(fileread(fullfile(folder,'linear-design.json')));
linear=jsondecode(fileread(fullfile(folder,'expanded-linear.json')));
manifest=jsondecode(fileread(fullfile(native,'source-manifest.json')));
for k=1:numel(manifest.Sources)
 h=manifest.Sources(k);assert(strcmp(hash(fullfile(root,'CurrentCascade',h.File)),h.SHA256),'C simulation source changed: %s',h.File);
end
assert(strcmp(hash(fullfile(root,'CurrentCascade','simulate_cascade.exe')),manifest.ExecutableSHA256),'Native binary changed');
t=readtable(fullfile(native,'summary.csv'),'TextType','string');
% The C harness's iref/error accumulator is the independent current-only
% test stimulus, not the cascade's outer-loop reference in other scenarios.
t.Properties.VariableNames{'iref_ma'}='current_only_target_ma';
t.Properties.VariableNames{'current_error_max_ma'}='current_only_max_true_error_ma';
currentOnly=startsWith(t.scenario,'current_only_');
t.current_only_target_ma(~currentOnly)=NaN;
t.current_only_max_true_error_ma(~currentOnly)=NaN;
t.trace_tail_mean_iref_ma=zeros(height(t),1);t.trace_tail_mean_sensed_error_ma=zeros(height(t),1);
for k=1:height(t)
 d=readtable(fullfile(native,"cascade-"+t.scenario(k)+".csv"));last=d.time_ms>=7500;
 t.trace_tail_mean_iref_ma(k)=mean(d.iref_ma(last));
 t.trace_tail_mean_sensed_error_ma(k)=mean(d.current_error_ma(last));
end
normal=~startsWith(t.scenario,'current_')&~startsWith(t.scenario,'inject_')&t.scenario~="infeasible";
events=ismember(t.scenario,["apply_10ohm","remove_10ohm","input_48to45","load_10to20","apply_gain_0937","apply_gain_090","apply_inner60us","apply_outer200us"]);
cap=901e-6*ones(height(t),1);esr=.01*ones(height(t),1);
cap(t.scenario=="small_LC")=630e-6;esr(t.scenario=="small_LC")=.003;
cap(t.scenario=="large_LC")=1171e-6;esr(t.scenario=="large_LC")=.04;
allowance=ceil(t.max_il_ma.*(esr+5e-6./cap));
t.vout_peak_upper_mv=t.max_mv+allowance;t.ripple_upper_mv=t.ripple_mv+allowance;
t.event_min_lower_mv=t.event_min_mv-allowance;
boundpass=all(t.vout_peak_upper_mv(normal)<37000)&all(t.ripple_upper_mv(normal)<=250)&all(t.event_min_lower_mv(events)>=32400);

% Execute actual factored float C impulse vs independent MATLAB state-space
% response. No direct-form fourth-order polynomial comparison or shared code.
imp=readtable(fullfile(native,'current-impulse.csv'));
s=tf('s');i=design.innerSelected;w=2*pi*i.notchHz;
Ci=c2d(ss(i.kp+i.ki/s)*ss(1/(1+s/(2*pi*i.poleHz)))*...
 ss((s^2+2*i.notchZeroDamping*w*s+w^2)/(s^2+2*i.notchPoleDamping*w*s+w^2)),20e-6,'tustin');
y=lsim(Ci,imp.error_nanoamp*1e-9,imp.sample*20e-6);
err=max(abs(y-imp.correction_picoduty*1e-12));
relativeError=err/max(abs(y));
numericPass=err<1e-7&&relativeError<.01;
fprintf('Float C vs MATLAB impulse: maximum duty error %.3g; relative %.3g\n',err,relativeError);

fig=figure('Visible','off','Position',[30 30 1450 980]);tiledlayout(3,2);
names=["startup_10ohm","startup_noload","apply_10ohm","remove_10ohm","apply_gain_090","current_only_2to2p5A"];
for k=1:numel(names)
 nexttile;d=readtable(fullfile(native,"cascade-"+names(k)+".csv"));hold on;
 if k==6
  plot(d.time_ms/1000,d.iin_ma/1000,'DisplayName','actual port Iin');
  plot(d.time_ms/1000,d.iref_ma/1000,'--','DisplayName','Iin reference');ylabel('Current / A');xlim([2.9 3.3]);
 else
  plot(d.time_ms/1000,d.vout_mv/1000,'DisplayName','cascade C');
  old=fullfile(root,'Results','candidate-v4-final',"candidate-"+names(k)+".csv");
  if isfile(old),b=readtable(old);plot(b.time_ms/1000,b.vout_mv/1000,'DisplayName','archived voltage v4');end
  ylabel('Vout / V');yline(36,'k:','HandleVisibility','off');
  if k>=3,xlim([2.95 3.3]);end
 end
 grid on;xlabel('Time / s');title(names(k),'Interpreter','none');legend('Location','best');
end
sgtitle('48V -> 36V / 10 ohm: offline C controller and LC/diode model; NOT hardware validation');
exportgraphics(fig,fullfile(folder,'cascade-comparison.png'));close(fig);
fig=figure('Visible','off','Position',[30 30 1400 900]);tiledlayout(3,1);
d=readtable(fullfile(native,'cascade-apply_gain_090.csv'));
nexttile;plot(d.time_ms/1000,d.vout_mv/1000);ylabel('Vout / V');grid on;xlim([2.95 3.3]);
row=t(t.scenario=="apply_gain_090",:);
yline(row.event_min_mv/1000,'r--','5us-grid minimum (curve exported every1ms)');
nexttile;plot(d.time_ms/1000,[d.iin_ma d.iref_ma]/1000);ylabel('Port current / A');legend('actual Iin','Iin reference');grid on;xlim([2.95 3.3]);
nexttile;plot(d.time_ms/1000,[d.feedforward_counts d.requested_counts d.slew_counts d.final_counts]/27200*100);
ylabel('Effective duty / %');xlabel('Time / s');grid on;xlim([2.95 3.3]);legend('feedforward','inner requested','rise slew','final brake');
exportgraphics(fig,fullfile(folder,'cascade-control-path.png'));close(fig);

report=struct('date','2026-10-27','algorithm','Voltage PI -> average input-port current reference -> current PI/LP/notch -> duty',...
 'targetV',36,'loadOhm',10,'nominalOutputW',129.6,'pwmHz',200000,'deadtimeNs',120.588235294,'maxEffectiveDuty',.90,...
 'innerSampleHz',50000,'outerSampleHz',10000,'innerSelected',design.innerSelected,'outerSelected',design.outerSelected,...
 'linear',linear,'nativeCases',height(t),'nativePassing',sum(t.pass),'nativeChecksPass',all(t.pass),...
 'currentReferenceLimit_A',4,'modelTripIin_A',8,'modelOVP_V',39,'inductorPeakIsProtection',false,...
 'floatImpulseMaxDutyError',err,'floatImpulseRelativeError',relativeError,'floatImpulsePass',numericPass,...
 'intraCycleBoundPass',boundpass,'nativeResults',table2struct(t),...
 'allAlgorithmModelChecksPass',all(t.pass)&&linear.passed&&boundpass&&numericPass,...
 'hardwareValidated',false,'firmwareFlashed',false,'firmwareRelease',false,...
 'baseline','Baselines/CleanBuck36-voltage-v4-20261026-235606; SHA256 checked618CleanBuck36files plus shared Drivers',...
 'scope','LC/diode model, fixed200kHz and120.588ns; average port current not IL. No bootstrap/Coss/reverse-recovery/driver fault model. Current-only, deliberately disconnected feedback and injected ADC faults have separate criteria. Infeasible goal must shut off. Sampled trip uses observed ADC current including gain/offset, not hidden plant truth.',...
 'comparison','Archived voltage v4 traces use the same nominal plant but their old port observation/offset model; these are regression references, not a hardware A/B experiment');
fid=fopen(fullfile(folder,'matlab-report.json'),'w');assert(fid>=0);fprintf(fid,'%s\n',jsonencode(report,PrettyPrint=true));fclose(fid);
fprintf('Native %d/%d; bounds %d; float matching %d; expanded linear %d; overall %d\n',sum(t.pass),height(t),boundpass,numericPass,linear.passed,report.allAlgorithmModelChecksPass);
assert(report.allAlgorithmModelChecksPass,'Model validation NOT passed; no firmware release');
end
function h=hash(path)
fid=fopen(path,'rb');assert(fid>=0);data=fread(fid,Inf,'*uint8');fclose(fid);
md=java.security.MessageDigest.getInstance('SHA-256');md.update(data);
h=upper(reshape(dec2hex(typecast(md.digest(),'uint8'),2).',1,[]));
end
