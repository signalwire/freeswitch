% fdmdv_demod_c.m
%
% Plots Octave dump file information from C FDMDV demodulator program,
% to give a similar set of plots to fdmdv_demod.m.  Useful for off
% line analysis of demod performance.
%
% Copyright David Rowe 2012
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%

function fdmdv_demod_c(dumpfilename, bits)

  fdmdv; % include modem code

  frames = bits/(Nc*Nb);

  load(dumpfilename);

  % BER stats

  total_bit_errors = 0;
  total_bits = 0;
  bit_errors_log = [];
  sync_log = [];
  test_frame_sync_log = [];
  test_frame_sync_state = 0;

  % Run thru received bits to look for test pattern

  bits_per_frame = Nc*Nb;

  for f=1:frames

    rx_bits = rx_bits_log_c((f-1)*bits_per_frame+1:f*bits_per_frame);

    % count bit errors if we find a test frame

    [test_frame_sync bit_errors] = put_test_bits(rx_bits);
    if (test_frame_sync == 1)
      total_bit_errors = total_bit_errors + bit_errors;
      total_bits = total_bits + Ntest_bits;
      bit_errors_log = [bit_errors_log bit_errors/Ntest_bits];
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
  % Plots
  % ---------------------------------------------------------------------

  xt = (1:frames)/Rs;
  secs = frames/Rs;

  figure(1)
  clf;
  plot(real(rx_symbols_log_c(1:Nc+1,15:frames)),imag(rx_symbols_log_c(1:Nc+1,15:frames)),'+')
  axis([-2 2 -2 2]);
  title('Scatter Diagram');

  figure(2)
  clf;
  subplot(211)
  plot(xt, rx_timing_log_c(1:frames))
  title('timing offset (samples)');
  subplot(212)
  plot(xt, foff_log_c(1:frames), '-;freq offset;')
  hold on;
  plot(xt, coarse_fine_log_c(1:frames)*75, 'r;course-fine;');
  hold off;
  title('Freq offset (Hz)');
  grid

  figure(3)
  clf;
  subplot(211)
  b = M*frames;
  xt1 = (1:b)/Fs;
  plot(xt1, rx_fdm_log_c(1:b));
  title('Rx FDM Signal');
  subplot(212)
  spec(rx_fdm_log_c(1:b),8000);
  title('FDM Rx Spectrogram');

  figure(4)
  clf;
  subplot(311)
  stem(xt, sync_bit_log_c(1:frames))
  axis([0 secs 0 1.5]);
  title('BPSK Sync')
  subplot(312)
  stem(xt, bit_errors_log);
  title('Bit Errors for test frames')
  subplot(313)
  plot(xt, test_frame_sync_log);
  axis([0 secs 0 1.5]);
  title('Test Frame Sync')

  figure(5)
  clf;
  plot(xt, snr_est_log_c(1:frames));
  title('SNR Estimates')

endfunction
