% glottal.m
% David Rowe 12 Sep 2009
% Matlab script to generate the phase spectra of a glottal pulse

% lpc10 pulse from spandsp.  When the file glottal.c was used as a part of the
% excitation phase component in phase.c, phase_synth_zero_order(), no difference 
% in speech quality was apparent.  So left out of code for now.

sh=12
kexc = [ 8,  -16,   26, -48,  86, -162, 294, -502, 718, -728, 184 672, -610, -672, 184, 728,  718, 502,  294, 162,   86,  48, 26, 16, 8];
kexc = shift(kexc,sh);
kexc = [kexc(1:sh) zeros(1,512-25) kexc(sh+1:25)];
figure(1)
clf
plot(kexc)
figure(2)
G = fft(kexc);
subplot(211)
plot((1:256)*(4000/256),unwrap(angle(G(1:256))))
subplot(212)
plot(20*log10(abs(G)))

f=fopen("glottal.c","wt");
fprintf(f,"const float glottal[]={\n");
for m=1:255
  fprintf(f,"  %f,\n",angle(G(m)));
endfor
fprintf(f,"  %f};\n",angle(G(256)));
fclose(f);
