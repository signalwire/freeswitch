% pulse.m
% David Rowe August 2009
%
% Experiments with human pulse perception for sinusoidal codecs

function pulse(samname)

  A = 1000;
  K = 16000;
  N = 80;
  frames = K/N;
  s = zeros(1,K);

  for f=1:frames
    % lets try placing np random pulses in every frame

    P = 20 + (160-20)*rand(1,1);
    Wo = 2*pi/P;
    L = floor(pi/Wo);
    sf = zeros(1,N);
    for m=1:L/2:L
      pos = floor(rand(1,1)*N)+1;
      %pos = 50;
      for l=m:m+L/2-1
        sf = sf + A*cos(l*Wo*((f-1)*N+1:f*N) - pos*l*Wo);
      endfor
    endfor
    s((f-1)*N+1:f*N) = sf;
  endfor

  plot(s(1:250));

  fs=fopen(samname,"wb");
  fwrite(fs,s,"short");
  fclose(fs);
endfunction

