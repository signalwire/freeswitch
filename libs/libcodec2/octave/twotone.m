% twotone.m
% David Rowe Aug 2012
% Used to experiment with combining phase of two tones

function cbphase

  Wo = 100.0*pi/4000;
  L = floor(pi/Wo);
  phi = zeros(1,L);

  % two harmonics 

  a = 20; b = 21;

  % set up phases to whatever
   
  phi(a) = -pi;
  phi(b) = -pi/2;

  % synthesis the two-tone signal

  N = 16000;
  Nplot = 250;
  s = zeros(1,N);

  for m=a:b
    s_m = cos(m*Wo*(0:(N-1)) + phi(m));
    s = s + s_m;
  endfor

  % now our theory says that this signal should be the same perceptually

  phi_(a) = (phi(a) - phi(b))/2;
  phi_(b) = (phi(b) - phi(a))/2;

  s_ = zeros(1,N);
  for m=a:b
    s_m = cos(m*Wo*(0:(N-1)) + phi_(m));
    s_ = s_ + s_m;
  endfor

  % plot them and see if envelope has the same phase, but "carriers"
  % have different phase

  figure(1);
  clf;
  subplot(211);
  plot(s(1:Nplot));
  subplot(212);
  plot(s_(1:Nplot),'r');
endfunction

