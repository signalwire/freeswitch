% fdmdv.m
%
% Functions that implement a Frequency Divison Multiplexed Modem for
% Digital Voice (FDMDV) over HF channels.
%
% Copyright David Rowe 2012
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%

% reqd to mak sure we get same random bits at mod and demod

rand('state',1); 
randn('state',1);

% Constants

global Fs = 8000;      % sample rate in Hz
global T  = 1/Fs;      % sample period in seconds
global Rs = 50;        % symbol rate in Hz
global Nc = 14;        % number of carriers
global Nb = 2;         % Bits/symbol for QPSK modulation
global Rb = Nc*Rs*Nb;  % bit rate
global M  = Fs/Rs;     % oversampling factor
global Nsym  = 6;      % number of symbols to filter over
global Fsep  = 75;     % Separation between carriers (Hz)
global Fcentre = 1200; % Centre frequency, Nc/2 carriers below this, N/c carriers above (Hz)
global Nt = 5;         % number of symbols we estimate timing over
global P = 4;          % oversample factor used for rx symbol filtering
global Nfilter = Nsym*M;
global Nfiltertiming = M+Nfilter+M;
alpha = 0.5;
global snr_coeff = 0.9 % SNR est averaging filter coeff

% root raised cosine (Root Nyquist) filter 

global gt_alpha5_root;
gt_alpha5_root = gen_rn_coeffs(alpha, T, Rs, Nsym, M);


% Functions ----------------------------------------------------


% generate Nc+1 QPSK symbols from vector of (1,Nc*Nb) input bits.  The
% Nc+1 symbol is the +1 -1 +1 .... BPSK sync carrier

function tx_symbols = bits_to_qpsk(prev_tx_symbols, tx_bits, modulation)
  global Nc;
  global Nb;
  global pilot_bit;

  % re-arrange as (Nc,Nb) matrix

  tx_bits_matrix = zeros(Nc,Nb);
  tx_bits_matrix(1:Nc,1) = tx_bits(1:Nb:Nb*Nc);
  tx_bits_matrix(1:Nc,2) = tx_bits(2:Nb:Nb*Nc);

  if (strcmp(modulation,'dqpsk')) 
 
    % map to (Nc,1) DQPSK symbols

    for c=1:Nc
      msb = tx_bits_matrix(c,1); lsb = tx_bits_matrix(c,2);

      if ((msb == 0) && (lsb == 0))
	  tx_symbols(c) = prev_tx_symbols(c);
      endif  
      if ((msb == 0) && (lsb == 1))
         tx_symbols(c) = j*prev_tx_symbols(c);
      endif  
      if ((msb == 1) && (lsb == 0))
         tx_symbols(c) = -prev_tx_symbols(c);
      endif  
      if ((msb == 1) && (lsb == 1))
         tx_symbols(c) = -j*prev_tx_symbols(c);
      endif 
    end
  else
    % QPSK mapping
    tx_symbols = -1 + 2*tx_bits_matrix(:,1) - j + 2j*tx_bits_matrix(:,2);
  endif

  % +1 -1 +1 -1 BPSK sync carrier, once filtered becomes two spectral
  % lines at +/- Rs/2
 
  if pilot_bit
     tx_symbols(Nc+1) = -prev_tx_symbols(Nc+1);
  else
     tx_symbols(Nc+1) = prev_tx_symbols(Nc+1);
  end
  if pilot_bit 
    pilot_bit = 0;
  else
    pilot_bit = 1;
  end

endfunction


% Given Nc*Nb bits construct M samples (1 symbol) of Nc filtered
% symbols streams

function tx_baseband = tx_filter(tx_symbols)
  global Nc;
  global M;
  global tx_filter_memory;
  global Nfilter;
  global gt_alpha5_root;

  tx_baseband = zeros(Nc+1,M);

  % tx filter each symbol, generate M filtered output samples for each symbol.
  % Efficient polyphase filter techniques used as tx_filter_memory is sparse

  tx_filter_memory(:,Nfilter) = sqrt(2)/2*tx_symbols;

  for i=1:M
    tx_baseband(:,i) = M*tx_filter_memory(:,M:M:Nfilter) * gt_alpha5_root(M-i+1:M:Nfilter)';
  end
  tx_filter_memory(:,1:Nfilter-M) = tx_filter_memory(:,M+1:Nfilter);
  tx_filter_memory(:,Nfilter-M+1:Nfilter) = zeros(Nc+1,M);

endfunction


% Construct FDM signal by frequency shifting each filtered symbol
% stream.  Returns complex signal so we can apply frequency offsets
% easily.

function tx_fdm = fdm_upconvert(tx_filt)
  global Fs;
  global M;
  global Nc;
  global Fsep;
  global phase_tx;
  global freq;

  tx_fdm = zeros(1,M);

  % Nc/2 tones below centre freq
  
  for c=1:Nc/2
      for i=1:M
        phase_tx(c) = phase_tx(c) * freq(c);
	tx_fdm(i) = tx_fdm(i) + tx_filt(c,i)*phase_tx(c);
      end
  end
  
  % Nc/2 tones above centre freq

  for c=Nc/2+1:Nc
      for i=1:M
        phase_tx(c) = phase_tx(c) * freq(c);
	tx_fdm(i) = tx_fdm(i) + tx_filt(c,i)*phase_tx(c);
      end
  end

  % add centre pilot tone 

  c = Nc+1;
  for i=1:M
    phase_tx(c) = phase_tx(c) * freq(c);
    pilot(i) = 2*tx_filt(c,i)*phase_tx(c);
    tx_fdm(i) = tx_fdm(i) + pilot(i);
  end
 
  % Scale such that total Carrier power C of real(tx_fdm) = Nc.  This
  % excludes the power of the pilot tone.
  % We return the complex (single sided) signal to make frequency
  % shifting for the purpose of testing easier

  tx_fdm = 2*tx_fdm;
endfunction


% Frequency shift each modem carrier down to Nc+1 baseband signals

function rx_baseband = fdm_downconvert(rx_fdm, nin)
  global Fs;
  global M;
  global Nc;
  global Fsep;
  global phase_rx;
  global freq;

  rx_baseband = zeros(1,nin);

  % Nc/2 tones below centre freq
  
  for c=1:Nc/2
      for i=1:nin
        phase_rx(c) = phase_rx(c) * freq(c);
	rx_baseband(c,i) = rx_fdm(i)*phase_rx(c)';
      end
  end

  % Nc/2 tones above centre freq  

  for c=Nc/2+1:Nc
      for i=1:nin
        phase_rx(c) = phase_rx(c) * freq(c);
	rx_baseband(c,i) = rx_fdm(i)*phase_rx(c)';
      end
  end

  % Pilot

  c = Nc+1;
  for i=1:nin
    phase_rx(c) = phase_rx(c) * freq(c);
    rx_baseband(c,i) = rx_fdm(i)*phase_rx(c)';
  end

endfunction


% Receive filter each baseband signal at oversample rate P

function rx_filt = rx_filter(rx_baseband, nin)
  global Nc;
  global M;
  global P;
  global rx_filter_memory;
  global Nfilter;
  global gt_alpha5_root;
  global Fsep;

  rx_filt = zeros(Nc+1,nin*P/M);

  % rx filter each symbol, generate P filtered output samples for each symbol.
  % Note we keep memory at rate M, it's just the filter output at rate P

  N=M/P;
  j=1;
  for i=1:N:nin
    rx_filter_memory(:,Nfilter-N+1:Nfilter) = rx_baseband(:,i:i-1+N);
    rx_filt(:,j) = rx_filter_memory * gt_alpha5_root';
    rx_filter_memory(:,1:Nfilter-N) = rx_filter_memory(:,1+N:Nfilter);
    j+=1;
  end
endfunction


% LPF and peak pick part of freq est, put in a function as we call it twice

function [foff imax pilot_lpf S] = lpf_peak_pick(pilot_baseband, pilot_lpf, nin)
  global M;
  global Npilotlpf;
  global Npilotcoeff;
  global Fs;
  global Mpilotfft;
  global pilot_coeff;

  % LPF cutoff 200Hz, so we can handle max +/- 200 Hz freq offset

  pilot_lpf(1:Npilotlpf-nin) = pilot_lpf(nin+1:Npilotlpf);
  j = 1;
  for i = Npilotlpf-nin+1:Npilotlpf
    pilot_lpf(i) = pilot_baseband(j:j+Npilotcoeff-1) * pilot_coeff';
    j++;
  end

  % decimate to improve DFT resolution, window and DFT

  Mpilot = Fs/(2*200);  % calc decimation rate given new sample rate is twice LPF freq
  h = hanning(Npilotlpf);
  s = pilot_lpf(1:Mpilot:Npilotlpf) .* h(1:Mpilot:Npilotlpf)';
  s = [s zeros(1,Mpilotfft-Npilotlpf/Mpilot)];
  S = fft(s, Mpilotfft);

  % peak pick and convert to Hz

  [imax ix] = max(S);
  r = 2*200/Mpilotfft;     % maps FFT bin to frequency in Hz
  
  if ix > Mpilotfft/2
    foff = (ix - Mpilotfft - 1)*r;
  else
    foff = (ix - 1)*r;
  endif

endfunction


% Estimate frequency offset of FDM signal using BPSK pilot.  This is quite
% sensitive to pilot tone level wrt other carriers

function [foff S1 S2] = rx_est_freq_offset(rx_fdm, pilot, pilot_prev, nin)
  global M;
  global Npilotbaseband;
  global pilot_baseband1;
  global pilot_baseband2;
  global pilot_lpf1;
  global pilot_lpf2;

  % down convert latest nin samples of pilot by multiplying by
  % ideal BPSK pilot signal we have generated locally.  This
  % peak of the resulting signal is sensitive to the time shift between 
  % the received and local version of the pilot, so we do it twice at
  % different time shifts and choose the maximum.
 
  pilot_baseband1(1:Npilotbaseband-nin) = pilot_baseband1(nin+1:Npilotbaseband);
  pilot_baseband2(1:Npilotbaseband-nin) = pilot_baseband2(nin+1:Npilotbaseband);
  for i=1:nin
    pilot_baseband1(Npilotbaseband-nin+i) = rx_fdm(i) * conj(pilot(i)); 
    pilot_baseband2(Npilotbaseband-nin+i) = rx_fdm(i) * conj(pilot_prev(i)); 
  end

  [foff1 max1 pilot_lpf1 S1] = lpf_peak_pick(pilot_baseband1, pilot_lpf1, nin);
  [foff2 max2 pilot_lpf2 S2] = lpf_peak_pick(pilot_baseband2, pilot_lpf2, nin);

  if max1 > max2
    foff = foff1;
  else
    foff = foff2;
  end  
endfunction


% Estimate optimum timing offset, re-filter receive symbols

function [rx_symbols rx_timing env] = rx_est_timing(rx_filt, rx_baseband, nin)
  global M;
  global Nt;
  global Nc;
  global rx_filter_mem_timing;
  global rx_baseband_mem_timing;
  global P;
  global Nfilter;
  global Nfiltertiming;
  global gt_alpha5_root;

  % nin  adjust 
  % --------------------------------
  % 120  -1 (one less rate P sample)
  % 160   0 (nominal)
  % 200   1 (one more rate P sample)

  adjust = P - nin*P/M;

  % update buffer of Nt rate P filtered symbols

  rx_filter_mem_timing(:,1:(Nt-1)*P+adjust) = rx_filter_mem_timing(:,P+1-adjust:Nt*P);
  rx_filter_mem_timing(:,(Nt-1)*P+1+adjust:Nt*P) = rx_filt(:,:);

  % sum envelopes of all carriers

  env = sum(abs(rx_filter_mem_timing(:,:))); % use all Nc+1 carriers for timing
  %env = abs(rx_filter_mem_timing(Nc+1,:));  % just use BPSK pilot
  [n m] = size(env);

  % The envelope has a frequency component at the symbol rate.  The
  % phase of this frequency component indicates the timing.  So work out
  % single DFT at frequency 2*pi/P

  x = env * exp(-j*2*pi*(0:m-1)/P)';
  
  % map phase to estimated optimum timing instant at rate M
  % the M/4 part was adjusted by experiment, I know not why....

  rx_timing = angle(x)*M/(2*pi) + M/4;
  if (rx_timing > M)
     rx_timing -= M;
  end
  if (rx_timing < -M)
     rx_timing += M;
  end

  % rx_baseband_mem_timing contains M + Nfilter + M samples of the
  % baseband signal at rate M this enables us to resample the filtered
  % rx symbol with M sample precision once we have rx_timing

  rx_baseband_mem_timing(:,1:Nfiltertiming-nin) = rx_baseband_mem_timing(:,nin+1:Nfiltertiming);
  rx_baseband_mem_timing(:,Nfiltertiming-nin+1:Nfiltertiming) = rx_baseband;

  % sample right in the middle of the timing estimator window, by filtering
  % at rate M

  s = round(rx_timing) + M;
  rx_symbols = rx_baseband_mem_timing(:,s+1:s+Nfilter) * gt_alpha5_root';

endfunction


% Phase estimation function - probably won't work over a HF channel
% Tries to operate over a single symbol but uses phase information from
% all Nc carriers which should increase the SNR of phase estimate.
% Maybe phase is coherent over a couple of symbols in HF channel,not
% sure but it's worth 3dB so worth experimenting or using coherent as
% an option.

function rx_phase = rx_est_phase(prev_rx_symbols, rx_symbols)

  % modulation strip

  rx_phase = angle(sum(rx_symbols .^ 4))/4;
 
endfunction


% convert symbols back to an array of bits

function [rx_bits sync_bit f_err phase_difference] = qpsk_to_bits(prev_rx_symbols, rx_symbols, modulation)
  global Nc;
  global Nb;
  global Nb;

  if (strcmp(modulation,'dqpsk')) 
    % extra 45 degree clockwise lets us use real and imag axis as
    % decision boundaries

    phase_difference = zeros(Nc+1,1);
    phase_difference(1:Nc) = rx_symbols(1:Nc) .* conj(prev_rx_symbols(1:Nc)) * exp(j*pi/4);
  
    % map (Nc,1) DQPSK symbols back into an (1,Nc*Nb) array of bits

    for c=1:Nc
      d = phase_difference(c);
      if ((real(d) >= 0) && (imag(d) >= 0))
         msb = 0; lsb = 0;
      endif  
      if ((real(d) < 0) && (imag(d) >= 0))
         msb = 0; lsb = 1;
      endif  
      if ((real(d) < 0) && (imag(d) < 0))
         msb = 1; lsb = 0;
      endif
      if ((real(d) >= 0) && (imag(d) < 0))
         msb = 1; lsb = 1;
      endif
      rx_bits(2*(c-1)+1) = msb;
      rx_bits(2*(c-1)+2) = lsb;
    end
 
    % Extract DBPSK encoded Sync bit

    phase_difference(Nc+1,1) = rx_symbols(Nc+1) .* conj(prev_rx_symbols(Nc+1));
    if (real(phase_difference(Nc+1)) < 0)
      sync_bit = 1;
      f_err = imag(phase_difference(Nc+1));
    else
      sync_bit = 0;
      f_err = -imag(phase_difference(Nc+1));
    end

    % pilot carrier gets an extra pi/4 rotation to make it consistent with
    % other carriers, as we need it for snr_update and scatter diagram

    phase_difference(Nc+1) *= exp(j*pi/4);
  else
    % map (Nc,1) QPSK symbols back into an (1,Nc*Nb) array of bits

    rx_bits(1:Nb:Nc*Nb) = real(rx_symbols) > 0;
    rx_bits(2:Nb:Nc*Nb) = imag(rx_symbols) > 0;
  endif

endfunction


% given phase differences update estimates of signal and noise levels

function [sig_est noise_est] = snr_update(sig_est, noise_est, phase_difference)
    global snr_coeff;
    global Nc;

    % mag of each symbol is distance from origin, this gives us a
    % vector of mags, one for each carrier.

    s = abs(phase_difference);

    % signal mag estimate for each carrier is a smoothed version
    % of instantaneous magntitude, this gives us a vector of smoothed
    % mag estimates, one for each carrier.

    sig_est = snr_coeff*sig_est + (1 - snr_coeff)*s;

    % noise mag estimate is distance of current symbol from average
    % location of that symbol.  We reflect all symbols into the first
    % quadrant for convenience.
    
    refl_symbols = abs(real(phase_difference)) + j*abs(imag(phase_difference));    
    n = abs(exp(j*pi/4)*sig_est - refl_symbols);
     
    % noise mag estimate for each carrier is a smoothed version of
    % instantaneous noise mag, this gives us a vector of smoothed
    % noise power estimates, one for each carrier.

    noise_est = snr_coeff*noise_est + (1 - snr_coeff)*n;
endfunction


% calculate current SNR estimate (3000Hz noise BW)

function snr_dB = calc_snr(sig_est, noise_est)
  global Rs;

  % find total signal power by summing power in all carriers

  S = sum(sig_est .^2);
  SdB = 10*log10(S);

  % Average noise mag across all carriers and square to get an average
  % noise power.  This is an estimate of the noise power in Rs = 50Hz of
  % BW (note for raised root cosine filters Rs is the noise BW of the
  % filter)

  N50 = mean(noise_est).^2;
  N50dB = 10*log10(N50);

  % Now multiply by (3000 Hz)/(50 Hz) to find the total noise power in
  % 3000 Hz

  N3000dB = N50dB + 10*log10(3000/Rs);

  snr_dB = SdB - N3000dB;

endfunction

% returns nbits from a repeating sequence of random data

function bits = get_test_bits(nbits)
  global Ntest_bits;       % length of test sequence
  global current_test_bit; 
  global test_bits;
 
  for i=1:nbits
    bits(i) = test_bits(current_test_bit++);
    if (current_test_bit > Ntest_bits)
      current_test_bit = 1;
    endif
  end
 
endfunction


% Accepts nbits from rx and attempts to sync with test_bits sequence.
% if sync OK measures bit errors

function [sync bit_errors] = put_test_bits(rx_bits)
  global Ntest_bits;       % length of test sequence
  global test_bits;
  global rx_test_bits_mem;

  % Append to our memory

  [m n] = size(rx_bits);
  rx_test_bits_mem(1:Ntest_bits-n) = rx_test_bits_mem(n+1:Ntest_bits);
  rx_test_bits_mem(Ntest_bits-n+1:Ntest_bits) = rx_bits;

  % see how many bit errors we get when checked against test sequence

  bit_errors = sum(xor(test_bits,rx_test_bits_mem));

  % if less than a thresh we are aligned and in sync with test sequence

  ber = bit_errors/Ntest_bits;
  
  sync = 0;
  if (ber < 0.2)
    sync = 1;
  endif
endfunction



% Generate M samples of DBPSK pilot signal for Freq offset estimation

function [pilot_fdm bit symbol filter_mem phase] = generate_pilot_fdm(bit, symbol, filter_mem, phase, freq)
  global M;
  global Nfilter;
  global gt_alpha5_root;

  % +1 -1 +1 -1 DBPSK sync carrier, once filtered becomes two spectral
  % lines at +/- Rs/2
 
  if bit
     symbol = -symbol;
  else
     symbol = symbol;
  end
  if bit 
    bit = 0;
  else
    bit = 1;
  end

  % filter DPSK symbol to create M baseband samples

  filter_mem(Nfilter) = (sqrt(2)/2)*symbol;
  for i=1:M
    tx_baseband(i) = M*filter_mem(M:M:Nfilter) * gt_alpha5_root(M-i+1:M:Nfilter)';
  end
  filter_mem(1:Nfilter-M) = filter_mem(M+1:Nfilter);
  filter_mem(Nfilter-M+1:Nfilter) = zeros(1,M);

  % upconvert

  for i=1:M
    phase = phase * freq;
    pilot_fdm(i) = sqrt(2)*2*tx_baseband(i)*phase;
  end

endfunction


% Generate a 4M sample vector of DBPSK pilot signal.  As the pilot signal
% is periodic in 4M samples we can then use this vector as a look up table
% for pilot signal generation in the demod.

function pilot_lut = generate_pilot_lut()
  global Nc;
  global Nfilter;
  global M;
  global freq;

  % pilot states

  pilot_rx_bit = 0;
  pilot_symbol = sqrt(2);
  pilot_freq = freq(Nc+1);
  pilot_phase = 1;
  pilot_filter_mem = zeros(1, Nfilter);
  %prev_pilot = zeros(M,1);

  pilot_lut = [];

  F=8;

  for f=1:F
    [pilot pilot_rx_bit pilot_symbol pilot_filter_mem pilot_phase] = generate_pilot_fdm(pilot_rx_bit, pilot_symbol, pilot_filter_mem, pilot_phase, pilot_freq);
    %prev_pilot = pilot;
    pilot_lut = [pilot_lut pilot];
  end

  % discard first 4 symbols as filter memory is filling, just keep last
  % four symbols

  pilot_lut = pilot_lut(4*M+1:M*F);

endfunction


% grab next pilot samples for freq offset estimation at demod

function [pilot prev_pilot pilot_lut_index prev_pilot_lut_index] = get_pilot(pilot_lut_index, prev_pilot_lut_index, nin)
  global M;
  global pilot_lut;

  for i=1:nin
    pilot(i) = pilot_lut(pilot_lut_index);
    pilot_lut_index++;
    if pilot_lut_index > 4*M
      pilot_lut_index = 1;
    end
    prev_pilot(i) = pilot_lut(prev_pilot_lut_index);
    prev_pilot_lut_index++;
    if prev_pilot_lut_index > 4*M
      prev_pilot_lut_index = 1;
    end
  end
endfunction



% Change the sample rate by a small amount, for example 1000ppm (ratio
% = 1.001).  Always returns nout samples in buf_out, but uses a
% variable number of input samples nin to accomodate the change in
% sample rate.  nin is nominally set to nout, but may use nout +/- 2
% samples to accomodate the different sample rates.  buf_in should be
% of length nout+6 samples to accomodate this, and buf_in should be
% updated externally based on the nin returned each time. "ratio" is
% Fs_in/Fs_out, for example 48048/48000 = 1.001 (+1000ppm) or
% 47952/48000 = 0.999 (-1000ppm).  Uses linear interpolation to
% perform the resampling.  This requires a highly over-sampled signal,
% for example 48000Hz sample rate for the modem signal centred on
% 1kHz, otherwise linear interpolation will have a low pass filter effect
% (for example an 8000Hz sample rate for modem signal centred on 1kHz
% would cause problems).

function [buf_out t nin] = resample(buf_in, t, ratio, nout)

  for i=1:nout
    c = floor(t);
    a = t - c;
    b = 1 - a;
    buf_out(i) = buf_in(c)*b + buf_in(c+1)*a;
    t += ratio;
  end

  t -= nout;
  
  % adjust nin and t so that on next call we start with 3 < t < 4,
  % this gives us +/- 2 samples room to move before we hit start or
  % end of buf_in

  delta = floor(t - 3);
  nin = nout + delta;
  t -= delta;

endfunction


% freq offset state machine.  Moves between acquire and track states based
% on BPSK pilot sequence.  Freq offset estimator occasionally makes mistakes
% when used continuously.  So we use it until we have acquired the BPSK pilot,
% then switch to a more robust tracking algorithm.  If we lose sync we switch
% back to acquire mode for fast-requisition.

function [track state] = freq_state(sync_bit, state)

  % acquire state, look for 6 symbol 010101 sequence from sync bit

  next_state = state;
  if state == 0
    if sync_bit == 0
      next_state = 1;
    end        
  end
  if state == 1
    if sync_bit == 1
      next_state = 2;
    else 
      next_state = 0;
    end        
  end
  if state == 2
    if sync_bit == 0
      next_state = 3;
    else 
      next_state = 0;
    end        
  end
  if state == 3
    if sync_bit == 1
      next_state = 4;
    else 
      next_state = 0;
    end        
  end
  if state == 4
    if sync_bit == 0
      next_state = 5;
    else 
      next_state = 0;
    end        
  end
  if state == 5
    if sync_bit == 1
      next_state = 6;
    else 
      next_state = 0;
    end        
  end

  % states 6 and above are track mode, make sure we keep getting 0101 sync bit sequence

  if state == 6
    if sync_bit == 0
      next_state = 7;
    else 
      next_state = 0;
    end        
  end
  if state == 7
    if sync_bit == 1
      next_state = 6;
    else 
      next_state = 0;
    end        
  end

  state = next_state;
  if state >= 6
    track = 1;
  else
    track = 0;
  end
endfunction


% Save test bits to a text file in the form of a C array

function test_bits_file(filename)
  global test_bits;
  global Ntest_bits;

  f=fopen(filename,"wt");
  fprintf(f,"/* Generated by test_bits_file() Octave function */\n\n");
  fprintf(f,"const int test_bits[]={\n");
  for m=1:Ntest_bits-1
    fprintf(f,"  %d,\n",test_bits(m));
  endfor
  fprintf(f,"  %d\n};\n",test_bits(Ntest_bits));
  fclose(f);
endfunction


% Saves RN filter coeffs to a text file in the form of a C array

function rn_file(filename)
  global gt_alpha5_root;
  global Nfilter;

  f=fopen(filename,"wt");
  fprintf(f,"/* Generated by rn_file() Octave function */\n\n");
  fprintf(f,"const float gt_alpha5_root[]={\n");
  for m=1:Nfilter-1
    fprintf(f,"  %g,\n",gt_alpha5_root(m));
  endfor
  fprintf(f,"  %g\n};\n",gt_alpha5_root(Nfilter));
  fclose(f);
endfunction

function pilot_coeff_file(filename)
  global pilot_coeff;
  global Npilotcoeff;

  f=fopen(filename,"wt");
  fprintf(f,"/* Generated by pilot_coeff_file() Octave function */\n\n");
  fprintf(f,"const float pilot_coeff[]={\n");
  for m=1:Npilotcoeff-1
    fprintf(f,"  %g,\n",pilot_coeff(m));
  endfor
  fprintf(f,"  %g\n};\n",pilot_coeff(Npilotcoeff));
  fclose(f);
endfunction


% Saves hanning window coeffs to a text file in the form of a C array

function hanning_file(filename)
  global Npilotlpf;

  h = hanning(Npilotlpf);

  f=fopen(filename,"wt");
  fprintf(f,"/* Generated by hanning_file() Octave function */\n\n");
  fprintf(f,"const float hanning[]={\n");
  for m=1:Npilotlpf-1
    fprintf(f,"  %g,\n", h(m));
  endfor
  fprintf(f,"  %g\n};\n", h(Npilotlpf));
  fclose(f);
endfunction


function png_file(fig, pngfilename)
  figure(fig);

  pngname = sprintf("%s.png",pngfilename);
  print(pngname, '-dpng', "-S500,500")
  pngname = sprintf("%s_large.png",pngfilename);
  print(pngname, '-dpng', "-S800,600")
endfunction

% Initialise ----------------------------------------------------

global pilot_bit;
pilot_bit = 0;     % current value of pilot bit

global tx_filter_memory;
tx_filter_memory = zeros(Nc+1, Nfilter);
global rx_filter_memory;
rx_filter_memory = zeros(Nc+1, Nfilter);

% phasors used for up and down converters

global freq;
freq = zeros(Nc+1,1);
for c=1:Nc/2
  carrier_freq = (-Nc/2 - 1 + c)*Fsep + Fcentre;
  freq(c) = exp(j*2*pi*carrier_freq/Fs);
end
for c=Nc/2+1:Nc
  carrier_freq = (-Nc/2 + c)*Fsep + Fcentre;
  freq(c) = exp(j*2*pi*carrier_freq/Fs);
end

freq(Nc+1) = exp(j*2*pi*Fcentre/Fs);

% Spread initial FDM carrier phase out as far as possible.  This
% helped PAPR for a few dB.  We don't need to adjust rx phase as DQPSK
% takes care of that.

global phase_tx;
%phase_tx = ones(Nc+1,1);
phase_tx = exp(j*2*pi*(0:Nc)/(Nc+1));
global phase_rx;
phase_rx = ones(Nc+1,1);

% Freq offset estimator constants

global Mpilotfft      = 256;
global Npilotcoeff    = 30;                             % number of pilot LPF coeffs
global pilot_coeff    = fir1(Npilotcoeff-1, 200/(Fs/2))'; % 200Hz LPF
global Npilotbaseband = Npilotcoeff + M + M/P;          % number of pilot baseband samples reqd for pilot LPF
global Npilotlpf      = 4*M;                            % number of samples we DFT pilot over, pilot est window

% pilot LUT, used for copy of pilot at rx
  
global pilot_lut;
pilot_lut = generate_pilot_lut();
pilot_lut_index = 1;
prev_pilot_lut_index = 3*M+1;

% Freq offset estimator states 

global pilot_baseband1;
global pilot_baseband2;
pilot_baseband1 = zeros(1, Npilotbaseband);             % pilot baseband samples
pilot_baseband2 = zeros(1, Npilotbaseband);             % pilot baseband samples
global pilot_lpf1
global pilot_lpf2
pilot_lpf1 = zeros(1, Npilotlpf);                       % LPF pilot samples
pilot_lpf2 = zeros(1, Npilotlpf);                       % LPF pilot samples

% Timing estimator states

global rx_filter_mem_timing;
rx_filter_mem_timing = zeros(Nc+1, Nt*P);
global rx_baseband_mem_timing;
rx_baseband_mem_timing = zeros(Nc+1, Nfiltertiming);

% Test bit stream constants

global Ntest_bits = Nc*Nb*4;     % length of test sequence
global test_bits = rand(1,Ntest_bits) > 0.5;

% Test bit stream state variables

global current_test_bit = 1;
current_test_bit = 1;
global rx_test_bits_mem;
rx_test_bits_mem = zeros(1,Ntest_bits);

