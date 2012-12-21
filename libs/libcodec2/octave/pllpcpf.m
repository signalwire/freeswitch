% Copyright David Rowe 2012
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plot amplitude modelling information from dump files to test and develop
% LPC post filter.

function pllpcpf(samname, f)
  
  % switch some stuff off to unclutter display

  plot_Am = 0;
  plot_Amq = 0;
  plot_err = 0;
  plot_lsp = 0;
  plot_snr = 0;
  plot_vsnr = 0;
  plot_sw = 0;
  plot_pw = 1;
  plot_pwb = 1;
  plot_rw = 1;

  sn_name = strcat(samname,"_sn.txt");
  Sn = load(sn_name);

  sw_name = strcat(samname,"_sw.txt");
  Sw = load(sw_name);

  sw__name = strcat(samname,"_sw_.txt");
  if (file_in_path(".",sw__name))
    Sw_ = load(sw__name);
  endif

  model_name = strcat(samname,"_model.txt");
  model = load(model_name);

  modelq_name = strcat(samname,"_qmodel.txt");
  if (file_in_path(".",modelq_name))
    modelq = load(modelq_name);
  endif

  % Pw (LPC synth filter spectrum) before post filter

  pwb_name = strcat(samname,"_pwb.txt");
  if (file_in_path(".",pwb_name))
    Pwb = load(pwb_name);
  endif

  % Rw (Post filter spectrum)

  rw_name = strcat(samname,"_rw.txt");
  if (file_in_path(".",rw_name))
    Rw = load(rw_name);
  endif

  % Pw (LPC synth filter spectrum) after post filter

  pw_name = strcat(samname,"_pw.txt");
  if (file_in_path(".",pw_name))
    Pw = load(pw_name);
  endif


  Ew_on = 1;
  k = ' ';
  do 
    figure(1);
    clf;
    s = [ Sn(2*f-1,:) Sn(2*f,:) ];
    size(s);
    plot(s);
    axis([1 length(s) -20000 20000]);

    figure(2);
    clf;
    Wo = model(f,1);
    L = model(f,2);
    Am = model(f,3:(L+2));
    if plot_Am
      plot((1:L)*Wo*4000/pi, 20*log10(Am),";Am;r");
    end
    axis([1 4000 -10 80]);
    hold on;
    if plot_sw
      plot((0:255)*4000/256, Sw(f,:),";Sw;");
    end
 
    if (file_in_path(".",modelq_name))

      Amq = modelq(f,3:(L+2));
      if plot_Amq
        plot((1:L)*Wo*4000/pi, 20*log10(Amq),";Amq;g" );
      end

      if (file_in_path(".",pwb_name) && plot_pwb)
        plot((0:255)*4000/256, 10*log10(Pwb(f,:)),";Pwb;r");
      endif	

      if (file_in_path(".",rw_name) && plot_rw)
        plot((0:255)*4000/256, 10*log10(Rw(f,:)),";Rw;b");
      endif	

      if (file_in_path(".",pw_name) && plot_pw)
        plot((0:255)*4000/256, 10*log10(Pw(f,:)),";Pw;g.");
      endif	

      signal = Am * Am';
      noise = (Am-Amq) * (Am-Amq)'; 
      snr1 = 10*log10(signal/noise);
      Am_err_label = sprintf(";Am error SNR %4.2f dB;m",snr1);
      if plot_err
        plot((1:L)*Wo*4000/pi, 20*log10(Amq) - 20*log10(Am), Am_err_label);
      end
    endif


    hold off;

    % interactive menu

    printf("\rframe: %d  menu: n-next  b-back  p-png  q-quit", f);
    fflush(stdout);
    k = kbhit();
    if (k == 'n')
      f = f + 1;
    endif
    if (k == 'b')
      f = f - 1;
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
