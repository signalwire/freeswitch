% pitch_test.m
% David Rowe Sep 2009
% Constructs a sequence to test the pitch estimator

function pitch_test(samname)
  M=320;
  F=200;

  fs=fopen(samname,"wb");

  f0 = 100;
  for f=1:200
    Wo=2*pi*f0/8000;
    P=2*pi/Wo;
    L = floor(pi/Wo);
    A = 10000/L;
    phi = zeros(1,L);
    s = zeros(1,M);

    for m=1:L
      s = s + A*cos(m*Wo*(0:(M-1)) + phi(m));
    endfor

    figure(1);
    clf;
    plot(s);

    fwrite(fs,s,"short");

    f0 = f0 + 5;
    if (f0 > 400)
      f0 = 100;    
    endif
  endfor

  fclose(fs);

endfunction

