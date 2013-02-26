% twotone1.m
% David Rowe Aug 17 2012
%
% Used to experiment with combining phase of two tones.  This version
% sets up a complete synthetic speech signal then tries to combine the
% phase of high frequency tones.  Lets see if we can do this and keep perceptual 
% nature of signal the same.

function twotone1
 
  % hts1a frame 47

  Wo = 0.093168;
  L = 33;
  %A = [69.626907 460.218536 839.677429 2577.498047 972.647888 712.755066 489.048553 364.830536 409.230652 371.767487 489.112854	893.127014 2447.596680 752.878113 475.720520 234.452271	248.161606 232.171051 202.669891 323.914490 678.749451 362.958038 211.652512 170.764435	148.631790 169.261673 272.254150 176.872375 67.344391 99.022301	60.812035 34.319073 14.864757];
  A  = zeros(1,L)*100;
  phi = [1.560274 1.508063 -1.565184 1.289117 -2.547365	1.412528 -1.303992 3.121130 1.087573 -1.158161 -2.928007 0.995093 -2.614023 0.246136 -2.267406 2.143802	-0.273431 -2.266897 1.685171 -0.668712 2.699722	-1.151891 2.406379 -0.046192 -2.718611 0.761067	-2.305014 0.133172 -1.428978 1.492630 -1.668385	1.539734 -1.336615];
  %phi = zeros(1,L);
  st = floor(L/2);
  %st = 1;

  A(st:st+5) = 1000;

  % now set up phase of signal with phase of upper frequency harmonic
  % pairs combined

  phi_ = phi;
  for m=floor(L/2):2:L
    phi_(m)   = (phi(m) - phi(m+1))/2;
    phi_(m+1) = (phi(m+1) - phi(m))/2;
    %phi_(m+1) = 0;
  end

  % synthesise the signals

  N = 16000;
  Nplot = 250;

  s = zeros(1,N);
  for m=st:L
    s_m = A(m)*cos(m*Wo*(0:(N-1)) + phi(m));
    s = s + s_m;
  endfor

  s_ = zeros(1,N);
  for m=st:L
    s_m = A(m)*cos(m*Wo*(0:(N-1)) + phi_(m));
    s_ = s_ + s_m;
  endfor

  % plot them, expect to see similar time domain waveforms

  figure(1);
  clf;
  subplot(211);
  plot(s(1:Nplot));
  subplot(212);
  plot(s_(1:Nplot),'r');

  figure(2);
  clf;
  subplot(211);
  plot(s(1:Nplot)-s_(1:Nplot));

  % save to disk

  gain = 1;
  fs=fopen("twotone1_orig.raw","wb");
  fwrite(fs,gain*s,"short");
  fclose(fs);
  fs=fopen("twotone1_comb.raw","wb");
  fwrite(fs,gain*s_,"short");
  fclose(fs);
  
endfunction

