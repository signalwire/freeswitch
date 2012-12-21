% av_imp.m
% David Rowe Aug 2012
% Averages the impulse response samples

function imp = av_imp(imp_filename, period_in_secs, st, en)
  f = fopen(imp_filename,"rb");
  s = fread(f, Inf, "short")';

  Fs = 8000;
  n = period_in_secs * Fs;

  [r c] = size(s);

  imp = zeros(1,n);
  for i=1:n:c-n
    imp = imp + s(i:i+n-1);
  endfor
  
  % user supplies start and end samples after viweing plot

  if (nargin == 4)
    imp = imp(st:en);
  end

  % normalise

  imp /= sqrt(sum(imp .^ 2));

  [h w] = freqz(imp, 1, 4000);

  figure(1);
  clf;
  plot(imp);

  figure(2);
  clf;
  subplot(211)
  plot(10*log10(abs(h)))
  subplot(212)
  plot(angle(h))

endfunction

