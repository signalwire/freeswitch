% phase2.m
% David Rowe Sep 2009
% experiments with phase for sinusoidal codecs, looking at phase
% of excitation with real Am samples from hts1

function phase2(samname, png)
  N = 16000;

  f=43;
  model = load("../src/hts1a_phase_model.txt");
  phase = load("../src/hts1a_phase_phase.txt");
  Wo = model(f,1);
  P=2*pi/Wo;
  L = model(f,2);
  A = model(f,3:(L+2));
  phi = phase(f,1:L);
  phi = zeros(1,L);
  phi(3) = -pi/2;
  phi(4) = -pi/4;
  phi(5) = pi/2;

  s = zeros(1,N);

  for m=3:5
    s_m = A(m)*cos(m*Wo*(0:(N-1)) + phi(m));
    s = s + s_m;
  endfor

  figure(1);
  clf;
  plot(s(1:250));

  figure(2);
  clf;
  subplot(211)
  plot((1:L)*Wo*4000/pi, 20*log10(A),'+');
  subplot(212)
  plot((1:L)*Wo*4000/pi, phi,'+');

  fs=fopen(samname,"wb");
  fwrite(fs,s,"short");
  fclose(fs);

  if (nargin == 2)
      % small image to fit blog

      __gnuplot_set__ terminal png size 450,300
      ss = sprintf("__gnuplot_set__ output \"%s.png\"", samname);
      eval(ss)
      replot;

      % for some reason I need this to stop large plot getting wiped
      __gnuplot_set__ output "/dev/null"
  endif

endfunction

