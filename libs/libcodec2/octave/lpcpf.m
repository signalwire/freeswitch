% lpcpf.m
% David Rowe Aug 27 2012
% Experiments with LPC post filtering

function lpcpf(ak_filename, f)
  aks = load(ak_filename);
	 
  ak = aks(f,:);
  [tmp p] = size(ak);
  p -= 1;

  A = freqz(1,ak, 4000);	 
  AdB = 20*log10(abs(A));

  gamma = 0.5;
  gammas = gamma .^ (0:p);
  W = freqz(ak .* gammas,1, 4000);
  WdB = 20*log10(abs(W));

  beta = 0.2;
  R = abs(freqz(ak .* gammas, ak, 4000));
  %P = (R/max(R)) .^ beta;
  P = R .^ beta;
  AP = abs(A) .* P;

  eA = sum(abs(A) .^ 2);
  eAP = sum(AP .^ 2);
  gain = sqrt(eA/eAP)
  AP *= gain;

  PdB = 20*log10(P);

  APdB = 20*log10(AP);
  10*log10(sum(AP .^ 2))/10*log10(sum(abs(A) .^ 2))

  figure(1);
  clf;
  plot(AdB);
  hold on;
  plot(WdB,'g');
  plot(PdB,'r');
  plot(APdB,'b.');
  hold off;

endfunction

