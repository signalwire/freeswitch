% phase.m
% David Rowe August 2009
% experiments with phase for sinusoidal codecs

function phase(samname, F0, png)
  Wo=2*pi*F0/8000;
  P=2*pi/Wo;
  L = floor(pi/Wo);
  Nsam = 16000;
  N = 80;
  F = Nsam/N;
  A = 10000/L;
  phi = zeros(1,L);
  s = zeros(1,Nsam);
  
  for m=floor(L/2):L
    phi_off(m) = -m*Wo*8;
  end

  for f=1:F
    phi(1) = phi(1) + Wo*N;
    phi(1) = mod(phi(1),2*pi);
  
    for m=1:L
      phi(m) = m*phi(1);
    end

    x = zeros(1,N);
    for m=1:L
      x = x + A*cos(m*Wo*(0:(N-1)) + phi(m));
    endfor
    s((f-1)*N+1:f*N) = x;
  endfor

  figure(1);
  clf;
  plot(s(1:250));

  fs=fopen(samname,"wb");
  fwrite(fs,s,"short");
  fclose(fs);

  if (nargin == 3)
      % small image to fit blog

      __gnuplot_set__ terminal png size 450,300
      ss = sprintf("__gnuplot_set__ output \"%s.png\"", samname);
      eval(ss)
      replot;

      % for some reason I need this to stop large plot getting wiped
      __gnuplot_set__ output "/dev/null"
  endif

endfunction

