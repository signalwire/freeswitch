% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plot ampltiude modelling information from dump files.

function plamp(samname, f, samname2)
  
  % switch some stuff off to unclutter display

  plot_lsp = 0;
  plot_snr = 0;
  plot_vsnr = 0;
  plot_sw = 0;
  plot_pw = 0;

  sn_name = strcat(samname,"_sn.txt");
  Sn = load(sn_name);

  sw_name = strcat(samname,"_sw.txt");
  Sw = load(sw_name);

  sw__name = strcat(samname,"_sw_.txt");
  if (file_in_path(".",sw__name))
    Sw_ = load(sw__name);
  endif

  ew_name = strcat(samname,"_ew.txt");
  if (file_in_path(".",ew_name))
    Ew = load(ew_name);
  endif

  rk_name = strcat(samname,"_rk.txt");
  if (file_in_path(".",rk_name))
    Rk = load(rk_name);
  endif

  model_name = strcat(samname,"_model.txt");
  model = load(model_name);

  modelq_name = strcat(samname,"_qmodel.txt");
  if (file_in_path(".",modelq_name))
    modelq = load(modelq_name);
  endif

  pw_name = strcat(samname,"_pw.txt");
  if (file_in_path(".",pw_name))
    Pw = load(pw_name);
  endif

  lsp_name = strcat(samname,"_lsp.txt");
  if (file_in_path(".",lsp_name))
    lsp = load(lsp_name);
  endif

  phase_name = strcat(samname,"_phase.txt");
  if (file_in_path(".",phase_name))
    phase = load(phase_name);
  endif

  phase_name_ = strcat(samname,"_phase_.txt");
  if (file_in_path(".",phase_name_))
    phase_ = load(phase_name_);
  endif

  snr_name = strcat(samname,"_snr.txt");
  if (file_in_path(".",snr_name))
    snr = load(snr_name);
  endif

  % optional second file, for exploring post filter

  model2q_name = " ";
  if nargin == 3
    model2q_name = strcat(samname2,"_qmodel.txt");
    if file_in_path(".",modelq_name)
      model2q = load(model2q_name);
    end
  end

  Ew_on = 1;
  k = ' ';
  do 
    figure(1);
    clf;
%    s = [ Sn(2*(f-2)-1,:) Sn(2*(f-2),:) ];
    s = [ Sn(2*f-1,:) Sn(2*f,:) ];
    size(s);
    plot(s);
    axis([1 length(s) -20000 20000]);

    figure(2);
    Wo = model(f,1);
    L = model(f,2);
    Am = model(f,3:(L+2));
    plot((1:L)*Wo*4000/pi, 20*log10(Am),";Am;r");
    axis([1 4000 -10 80]);
    hold on;
    if plot_sw
      plot((0:255)*4000/256, Sw(f,:),";Sw;");
    end
 
    if (file_in_path(".",modelq_name))
      Amq = modelq(f,3:(L+2));
      plot((1:L)*Wo*4000/pi, 20*log10(Amq),";Amq;g" );
      if (file_in_path(".",pw_name) && plot_pw)
        plot((0:255)*4000/256, 10*log10(Pw(f,:)),";Pw;c");
      endif	
      signal = Am * Am';
      noise = (Am-Amq) * (Am-Amq)'; 
      snr1 = 10*log10(signal/noise);
      Am_err_label = sprintf(";Am error SNR %4.2f dB;m",snr1);
      plot((1:L)*Wo*4000/pi, 20*log10(Amq) - 20*log10(Am), Am_err_label);
    endif

    if file_in_path(".",model2q_name)
      Amq2 = model2q(f,3:(L+2));
      plot((1:L)*Wo*4000/pi, 20*log10(Amq2),";Amq2;m" );
    end

    if (file_in_path(".",snr_name) && plot_vsnr)
      snr_label = sprintf(";Voicing SNR %4.2f dB;",snr(f));
      plot(1,1,snr_label);
    endif

    % phase model - determine SNR and error spectrum for phase model 1

    if (file_in_path(".",phase_name_))
      orig  = Am.*exp(j*phase(f,1:L));
      synth = Am.*exp(j*phase_(f,1:L));
      signal = orig * orig';
      noise = (orig-synth) * (orig-synth)';
      snr_phase = 10*log10(signal/noise);

      %phase_err_label = sprintf(";phase_err SNR %4.2f dB;",snr_phase);
      %plot((1:L)*Wo*4000/pi, 20*log10(orig-synth), phase_err_label);
    endif

    if (file_in_path(".",lsp_name) && plot_lsp)
      for l=1:10
        plot([lsp(f,l)*4000/pi lsp(f,l)*4000/pi], [60 80], 'r');
      endfor
    endif

    hold off;

    %if (file_in_path(".",phase_name))
      %figure(3);
      %plot((1:L)*Wo*4000/pi, phase(f,1:L), ";phase;");
      %axis;
      %if (file_in_path(".",phase_name_))
        %hold on;
        %plot((1:L)*Wo*4000/pi, phase_(f,1:L), ";phase_;");
	%hold off;
      %endif
      %figure(2);
    %endif

    % interactive menu

    printf("\rframe: %d  menu: n-next  b-back  p-png  q-quit e-toggle Ew", f);
    fflush(stdout);
    k = kbhit();
    if (k == 'n')
      f = f + 1;
    endif
    if (k == 'b')
      f = f - 1;
    endif
    if (k == 'e')
	if (Ew_on == 1)
	    Ew_on = 0;
        else
	    Ew_on = 1;
        endif
    endif

    % optional print to PNG

    if (k == 'p')
      figure(1);
      pngname = sprintf("%s_%d_sn.png",samname,f);
      print(pngname, '-dpng', "-S500,500")
      pngname = sprintf("%s_%d_sn_large.png",samname,f);
      print(pngname, '-dpng', "-S800,600")

      figure(2);
      pngname = sprintf("%s_%d_sw.png",samname,f);
      print(pngname, '-dpng', "-S500,500")
      pngname = sprintf("%s_%d_sw_large.png",samname,f);
      print(pngname, '-dpng', "-S1200,800")
    endif

  until (k == 'q')
  printf("\n");

endfunction
