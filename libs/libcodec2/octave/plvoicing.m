% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plot voicing information from sample and dump files.
%
% samfilename is the raw source file, e.g. "../raw/hts1a.raw"
% samname is the dumpfile prefix, e.g. "../src/hts1a"
%
% There is a 160 sample (two frame delay) from the when a sample
% enters the input buffer until it is at the centre of the analysis window

function plvoicing(samfilename, samname, start_f, end_f, pngname)
  
  fs=fopen(samfilename,"rb");
  s=fread(fs,Inf,"short");

  snr_name = strcat(samname,"_snr.txt");
  snr = load(snr_name);
  model_name = strcat(samname,"_model.txt");
  model = load(model_name);

  Wo  = model((start_f+1):end_f,1);
  F0  = Wo*4000/pi;
  dF0 = F0(1:length(Wo)-1) - F0(2:length(Wo));

  % work out LP and HP energy

  for f=(start_f+1):end_f
    L = model(f,2);
    Am = model(f,3:(L+2));
    L2 = floor(L/2);
    elow = Am(1:L2) * Am(1:L2)';
    ehigh = Am(L2:L) * Am(L2:L)';
    erat(f-(start_f+1)+1) = 10*log10(elow/ehigh);
  endfor

  figure(1);
  clf;
  sp = s((start_f-2)*80:(end_f-2)*80);
  plot(sp);
  hold on;
  vhigh = snr((start_f+1):end_f) > 7;
  vlow = snr((start_f+1):end_f) > 4;

  % test correction based on erat

  vlowadj = vlow;

  for f=1:length(erat)-1
    if (vlow(f) == 0)
      if (erat(f) > 10)
        vlowadj(f) = 1;
      endif
    endif
    if (vlow(f) == 1)
      if (erat(f) < -10)
        vlowadj(f) = 0;
      endif
      if (abs(dF0(f)) > 15)
        vlowadj(f) = 0;
      endif
    endif
  endfor

  x = 1:(end_f-start_f);
  plot(x*80,snr((start_f+1):end_f)*1000,';SNRdB x 1000;g+');
  plot(x*80,-8000 + vhigh*2000,';7dB thresh;g');
  plot(x*80,-11000 + vlowadj*2000,';vlow with corr;g');
  plot(x*80,erat*1000,';elow/ehigh in dB;r');
  plot(x*80,-14000 + vlow*2000,';4dB thresh;r');
  hold off; 
  grid
  if (nargin == 5)
      print(pngname, "-dpng", "-S500,500")
  endif
    
  figure(2)
  Wo  = model((start_f+1):end_f,1);
  F0  = Wo*4000/pi;
  dF0 = F0(1:length(Wo)-1) - F0(2:length(Wo));
  %plot(dF0,'+--')
  %hold on;
  %plot([ 1 length(dF0) ], [10 10] ,'r')
  %plot([ 1 length(dF0) ], [-10 -10] ,'r')
  %axis([1 length(dF0) -50 50])
  %hold off;
  plot(F0,'+--')
endfunction
