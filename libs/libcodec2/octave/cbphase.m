% cbphase.m
% David Rowe Aug 2012
% Used to experiment with critical band phase perception and smoothing

function cbphase

  Wo = 100.0*pi/4000;
  L = floor(pi/Wo);

  A = zeros(1,L);
  phi = zeros(1,L);

  % three harmonics in this band

  b = 4; a = b-1; c = b+1;

  % set up phases and mags for 2nd order system (see phasesecord.m)
   
  wres = b*Wo;
  phi(a) = 3*pi/4 + wres;
  phi(b) = pi/2 + wres;
  phi(c) = pi/4 + wres;

  A(a) = 0.707;
  A(b) = 1;
  A(c) = 0.707;

  % add linear component

  phi(1) = pi;
  phi(2:L) = phi(2:L) + (2:L)*phi(1);
  phi = phi - 2*pi*(floor(phi/(2*pi)) + 0.5);

  N = 16000;
  Nplot = 250;
  s = zeros(1,N);

  for m=a:c
    s_m = A(m)*cos(m*Wo*(0:(N-1)) + phi(m));
    s = s + s_m;
  endfor

  figure(2);
  clf;
  subplot(211)
  plot((1:L)*Wo*4000/pi, A,'+');
  subplot(212)
  plot((1:L)*Wo*4000/pi, phi,'+');

  %v = A(a)*exp(j*phi(a)) + A(b)*exp(j*phi(b)) + A(c)*exp(j*phi(c));
  %compass(v,"r")
  %hold off;
  
  % est phi1

  diff = phi(b) - phi(a)
  sumi = sin(diff);
  sumr = cos(diff);
  diff = phi(c) - phi(b)
  sumi += sin(diff);
  sumr += cos(diff);
  phi1_ = atan2(sumi, sumr)
  s_v = cos(Wo*(0:(N-1)) + phi1_);

  figure(1);
  clf;
  subplot(211)
  plot(s(1:Nplot));
  hold on;
  plot(s_v(1:Nplot),"r");
  hold off;

  % build (hopefully) perceptually similar phase

  phi_(a) = a*phi1_;
  phi_(b) = b*phi1_;
  phi_(c) = c*phi1_;

  s_ = zeros(1,N);

  for m=a:c
    s_m = A(m)*cos(m*Wo*(0:(N-1)) + phi_(m));
    s_ = s_ + s_m;
  endfor
 
  subplot(212)
  plot(s_(1:Nplot));
  
  gain = 8000;
  fs=fopen("orig_ph.raw","wb");
  fwrite(fs,gain*s,"short");
  fclose(fs);
  fs=fopen("mod_ph.raw","wb");
  fwrite(fs,gain*s_,"short");
  fclose(fs);

endfunction

