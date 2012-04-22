% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plot NLP states from dump files.

function plnlp(samname, f)
  
  sn_name = strcat(samname,"_sn.txt");
  Sn = load(sn_name);

  sw_name = strcat(samname,"_sw.txt");
  Sw = load(sw_name);

  fw_name = strcat(samname,"_fw.txt");
  if (file_in_path(".",fw_name))
    fw = load(fw_name);
  endif

  e_name = strcat(samname,"_e.txt");
  if (file_in_path(".",e_name))
    e = load(e_name);
  endif

  p_name = strcat(samname,".p");
  if (file_in_path(".",p_name))
    p = load(p_name);
  endif

  sq_name = strcat(samname,"_sq.txt");
  if (file_in_path(".",sq_name))
    sq = load(sq_name);
  endif

  dec_name = strcat(samname,"_dec.txt");
  if (file_in_path(".",dec_name))
    dec = load(dec_name);
  endif

  do 
    figure(1);
    clf;
    s = [ Sn(2*f-1,:) Sn(2*f,:) ];
    plot(s, ";Sn;");
    grid
    axis([1 length(s) -20000 20000]);

    figure(2);
    plot((0:255)*4000/256, Sw(f,:),";Sw;");
    grid
    axis([1 4000 -10 80]);
    hold on;

    f0 = 8000/p(f);
    Wo = 2*pi/p(f);
    L = floor(pi/Wo);
    f0_label = sprintf("b;P=%3.1f F0=%3.0f;",p(f),f0);
    for m=1:L-1
      plot([ m*Wo*4000/pi m*Wo*4000/pi], [10 60], 'b');
    endfor
    plot([ L*Wo*4000/pi L*Wo*4000/pi], [10 60], f0_label);

    hold off;

    if (file_in_path(".",fw_name))
      figure(3);
      if (file_in_path(".",e_name))
         subplot(211);
      endif
      plot((0:255)*800/256, fw(f,:)/max(fw(f,:)), ";Fw;");
      axis([1 400 0 1]);
      if (file_in_path(".",e_name))
        subplot(212);
        e_concat = [ e(2*f-1,:) e(2*f,:) ];
        plot(e_concat(1:400)/max(e_concat(1:400)), "+;MBE E(f);");
        axis([1 400 0 1]);
      endif
    endif

    if (file_in_path(".",sq_name))
      figure(4);
      sq_concat = [ sq(2*f-1,:) sq(2*f,:) ];
      axis
      plot(sq_concat, ";sq;");
    endif

    if (file_in_path(".",dec_name))
      figure(5);
      plot(dec(f,:), ";dec;");
    endif
    
    figure(2);

    % interactive menu

    printf("\rframe: %d  menu: n-next  b-back  p-png  q-quit ", f);
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
    
      pngname = sprintf("%s_%d",samname,f);

      % small image

      __gnuplot_set__ terminal png size 420,300
      ss = sprintf("__gnuplot_set__ output \"%s.png\"", pngname);
      eval(ss)
      replot;

      % larger image

      __gnuplot_set__ terminal png size 800,600
      ss = sprintf("__gnuplot_set__ output \"%s_large.png\"", pngname);
      eval(ss)
      replot;

      % for some reason I need this to stop large plot getting wiped
      __gnuplot_set__ output "/dev/null"

    endif

  until (k == 'q')
  printf("\n");

endfunction
