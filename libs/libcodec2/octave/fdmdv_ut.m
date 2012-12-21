% fdmdv_ut.m
%
% Unit Test program for FDMDV modem.  Useful for general development as it has
% both tx and rx sides, and basic AWGN channel simulation.
%
% Copyright David Rowe 2012
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%

fdmdv;               % load modem code
 
% Simulation Parameters --------------------------------------

frames = 25;
EbNo_dB = 7.3;
Foff_hz = 0;
modulation = 'dqpsk';
hpa_clip = 150;

% ------------------------------------------------------------

tx_filt = zeros(Nc,M);
rx_symbols_log = [];
rx_phase_log = 0;
rx_timing_log = 0;
tx_pwr = 0;
noise_pwr = 0;
rx_fdm_log = [];
rx_baseband_log = [];
rx_bits_offset = zeros(Nc*Nb*2);
prev_tx_symbols = ones(Nc+1,1);
prev_rx_symbols = ones(Nc+1,1);
ferr = 0;
foff = 0;
foff_log = [];
tx_baseband_log = [];
tx_fdm_log = [];

% BER stats

total_bit_errors = 0;
total_bits = 0;
bit_errors_log = [];
sync_log = [];
test_frame_sync_log = [];
test_frame_sync_state = 0;

% SNR estimation states

sig_est = zeros(Nc+1,1);
noise_est = zeros(Nc+1,1);

% fixed delay simuation

Ndelay = M+20;
rx_fdm_delay = zeros(Ndelay,1);

% ---------------------------------------------------------------------
% Eb/No calculations.  We need to work out Eb/No for each FDM carrier.
% Total power is sum of power in all FDM carriers
% ---------------------------------------------------------------------

C = 1; % power of each FDM carrier (energy/sample).  Total Carrier power should = Nc*C = Nc
N = 1; % total noise power (energy/sample) of noise source across entire bandwidth

% Eb  = Carrier power * symbol time / (bits/symbol)
%     = C *(1/Rs) / 2
Eb_dB = 10*log10(C) - 10*log10(Rs) - 10*log10(2);

No_dBHz = Eb_dB - EbNo_dB;

% Noise power = Noise spectral density * bandwidth
% Noise power = Noise spectral density * Fs/2 for real signals
N_dB = No_dBHz + 10*log10(Fs/2);
Ngain_dB = N_dB - 10*log10(N);
Ngain = 10^(Ngain_dB/20);

% C/No = Carrier Power/noise spectral density
%      = power per carrier*number of carriers / noise spectral density
CNo_dB = 10*log10(C)  + 10*log10(Nc) - No_dBHz;

% SNR in equivalent 3000 Hz SSB channel

B = 3000;
SNR = CNo_dB - 10*log10(B);

% freq offset simulation states

phase_offset = 1;
freq_offset = exp(j*2*pi*Foff_hz/Fs);
foff_phase = 1;
t = 0;
foff = 0;
fest_state = 0;
track = 0;
track_log = [];


% ---------------------------------------------------------------------
% Main loop 
% ---------------------------------------------------------------------

for f=1:frames

  % -------------------
  % Modulator
  % -------------------

  tx_bits = get_test_bits(Nc*Nb);
  tx_symbols = bits_to_qpsk(prev_tx_symbols, tx_bits, modulation);
  prev_tx_symbols = tx_symbols;
  tx_baseband = tx_filter(tx_symbols);
  tx_baseband_log = [tx_baseband_log tx_baseband];
  tx_fdm = fdm_upconvert(tx_baseband);
  tx_pwr = 0.9*tx_pwr + 0.1*real(tx_fdm)*real(tx_fdm)'/(M);

  % -------------------
  % Channel simulation
  % -------------------

  % frequency offset

  %Foff_hz += 1/Rs;
  Foff = Foff_hz;
  for i=1:M
    % Time varying freq offset
    %Foff = Foff_hz + 100*sin(t*2*pi/(300*Fs));
    %t++;
    freq_offset = exp(j*2*pi*Foff/Fs);
    phase_offset *= freq_offset;
    tx_fdm(i) = phase_offset*tx_fdm(i);
  end

  tx_fdm = real(tx_fdm);

  % HPA non-linearity

  tx_fdm(find(abs(tx_fdm) > hpa_clip)) = hpa_clip;
  tx_fdm_log = [tx_fdm_log tx_fdm];

  rx_fdm = tx_fdm;

  % AWGN noise

  noise = Ngain*randn(1,M);
  noise_pwr = 0.9*noise_pwr + 0.1*noise*noise'/M;
  rx_fdm += noise;
  rx_fdm_log = [rx_fdm_log rx_fdm];

  % Delay

  rx_fdm_delay(1:Ndelay-M) = rx_fdm_delay(M+1:Ndelay);
  rx_fdm_delay(Ndelay-M+1:Ndelay) = rx_fdm;
  %rx_fdm_delay = rx_fdm;

  % -------------------
  % Demodulator
  % -------------------

  % frequency offset estimation and correction, need to call rx_est_freq_offset even in track
  % mode to keep states updated

  [pilot prev_pilot pilot_lut_index prev_pilot_lut_index] = get_pilot(pilot_lut_index, prev_pilot_lut_index, M);
  [foff_course S1 S2] = rx_est_freq_offset(rx_fdm_delay, pilot, prev_pilot, M);
  if track == 0
    foff = foff_course;
  end
  foff_log = [ foff_log foff ];
  foff_rect = exp(j*2*pi*foff/Fs);

  for i=1:M
    foff_phase *= foff_rect';
    rx_fdm_delay(i) = rx_fdm_delay(i)*foff_phase;
  end

  % baseband processing

  rx_baseband = fdm_downconvert(rx_fdm_delay(1:M), M);
  rx_baseband_log = [rx_baseband_log rx_baseband];
  rx_filt = rx_filter(rx_baseband, M);

  [rx_symbols rx_timing] = rx_est_timing(rx_filt, rx_baseband, M);
  rx_timing_log = [rx_timing_log rx_timing];

  %rx_phase = rx_est_phase(rx_symbols);
  %rx_phase_log = [rx_phase_log rx_phase];
  %rx_symbols = rx_symbols*exp(j*rx_phase);

  [rx_bits sync foff_fine pd] = qpsk_to_bits(prev_rx_symbols, rx_symbols, modulation);
  if strcmp(modulation,'dqpsk')
    %rx_symbols_log = [rx_symbols_log rx_symbols.*conj(prev_rx_symbols)*exp(j*pi/4)];
    rx_symbols_log = [rx_symbols_log pd];
  else
    rx_symbols_log = [rx_symbols_log rx_symbols];
  endif
  foff -= 0.5*ferr;
  prev_rx_symbols = rx_symbols;
  sync_log = [sync_log sync];
  
  % freq est state machine

  [track fest_state] = freq_state(sync, fest_state);
  track_log = [track_log track];

  % Update SNR est

  [sig_est noise_est] = snr_update(sig_est, noise_est, pd);

  % count bit errors if we find a test frame
  % Allow 15 frames for filter memories to fill and time est to settle

  [test_frame_sync bit_errors] = put_test_bits(rx_bits);
  if test_frame_sync == 1
    total_bit_errors = total_bit_errors + bit_errors;
    total_bits = total_bits + Ntest_bits;
    bit_errors_log = [bit_errors_log bit_errors];
    else
      bit_errors_log = [bit_errors_log 0];
  end
 
  % test frame sync state machine, just for more informative plots
    
  next_test_frame_sync_state = test_frame_sync_state;
  if (test_frame_sync_state == 0)
    if (test_frame_sync == 1)      
      next_test_frame_sync_state = 1;
      test_frame_count = 0;
    end
  end

  if (test_frame_sync_state == 1)
    % we only expect another test_frame_sync pulse every 4 symbols
    test_frame_count++;
    if (test_frame_count == 4)
      test_frame_count = 0;
      if ((test_frame_sync == 0))      
        next_test_frame_sync_state = 0;
      end
    end
  end
  test_frame_sync_state = next_test_frame_sync_state;
  test_frame_sync_log = [test_frame_sync_log test_frame_sync_state];
end

% ---------------------------------------------------------------------
% Print Stats
% ---------------------------------------------------------------------

ber = total_bit_errors / total_bits;

% Peak to Average Power Ratio calcs from http://www.dsplog.com

papr = max(tx_fdm_log.*conj(tx_fdm_log)) / mean(tx_fdm_log.*conj(tx_fdm_log));
papr_dB = 10*log10(papr);

% Note Eb/No set point is for Nc data carriers only, exclduing pilot.
% This is convenient for testing BER versus Eb/No.  Measured Eb/No
% includes power of pilot.  Similar for SNR, first number is SNR excluding
% pilot pwr for Eb/No set point, 2nd value is measured SNR which will be a little
% higher as pilot power is included.

printf("Eb/No (meas): %2.2f (%2.2f) dB\n", EbNo_dB, 10*log10(0.25*tx_pwr*Fs/(Rs*Nc*noise_pwr)));
printf("bits........: %d\n", total_bits);
printf("errors......: %d\n", total_bit_errors);
printf("BER.........: %1.4f\n",  ber);
printf("PAPR........: %1.2f dB\n", papr_dB);
printf("SNR...(meas): %2.2f (%2.2f) dB\n", SNR, calc_snr(sig_est, noise_est));

% ---------------------------------------------------------------------
% Plots
% ---------------------------------------------------------------------

figure(1)
clf;
[n m] = size(rx_symbols_log);
plot(real(rx_symbols_log(1:Nc+1,15:m)),imag(rx_symbols_log(1:Nc+1,15:m)),'+')
axis([-3 3 -3 3]);
title('Scatter Diagram');

figure(2)
clf;
subplot(211)
plot(rx_timing_log)
title('timing offset (samples)');
subplot(212)
plot(foff_log, '-;freq offset;')
hold on;
plot(track_log*75, 'r;course-fine;');
hold off;
title('Freq offset (Hz)');

figure(3)
clf;
subplot(211)
plot(real(tx_fdm_log));
title('FDM Tx Signal');
subplot(212)
Nfft=Fs;
S=fft(rx_fdm_log,Nfft);
SdB=20*log10(abs(S));
plot(SdB(1:Fs/4))
title('FDM Rx Spectrum');

figure(4)
clf;
subplot(311)
stem(sync_log)
axis([0 frames 0 1.5]);
title('BPSK Sync')
subplot(312)
stem(bit_errors_log);
title('Bit Errors for test frames')
subplot(313)
plot(test_frame_sync_log);
axis([0 frames 0 1.5]);
title('Test Frame Sync')

