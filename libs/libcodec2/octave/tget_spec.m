% tget-spec.m
%
% Used in conjunction with src/fdmdv_demod to test the
% fdmdv_get_rx_spectrum() function.
%
% codec2-dev/src$ ./fdmdv_demod fdmdv_mod.raw tmp.c2 dump.txt
% octave:3> tget_spec("../src/dump.txt")
%
% Copyright David Rowe 2012
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%

function tfft_log(dumpfilename)

  load(dumpfilename);

  [rows cols] = size(rx_spec_log_c);
  Fs = 8000; low_freq = 0; high_freq = 2500;
  res = (Fs/2)/cols;
  st_bin = low_freq/res + 1;
  en_bin = high_freq/res;
  xaxis = (st_bin:en_bin)*res;

  f_start = 2; f_end = 100;
  beta = 0.1;

  av = zeros(f_end, en_bin-st_bin+1);
  for r=f_start:f_end
      x = (1-beta)*av(r-1,:) + beta*rx_spec_log_c(r,st_bin:en_bin);
      av(r,:) = x;
  end

  % spectrogram (waterfall)

  figure(1)
  clf;
  imagesc(av,[-40 0]);

  % animated spectrum display

  figure(2)
  clf;
  for r=f_start:f_end
      plot(xaxis, av(r,:))
      axis([ low_freq high_freq -40 0])
      sleep(0.1)
  end
endfunction
