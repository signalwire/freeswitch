% sd.m
% David Rowe Aug 2012
% Plots the spectal distorion between twofiles of LPCs.  Used for LSP 
% quantisation tuning.

function sd(raw_filename, dump_file_prefix, f)

  ak1_filename = sprintf("%s_ak.txt", dump_file_prefix);
  ak2_filename = sprintf("%s_ak_.txt", dump_file_prefix);
  ak1 = load(ak1_filename);
  ak2 = load(ak2_filename);

  [ak1_r, ak1_c] = size(ak1);
  [ak2_r, ak2_c] = size(ak1);

  frames = max([ak1_r ak2_r]);
  sd = zeros(1,frames);
  Ndft = 512;
  A1 = zeros(frames, Ndft);
  A2 = zeros(frames, Ndft);

  % initial helicopter view of all frames

  for i = 1:frames
    A1(i,:) = -20*log10(abs(fft(ak1(i,:),Ndft)));
    A2(i,:) = -20*log10(abs(fft(ak2(i,:),Ndft)));
    sd(i) = sum((A1(i,:) - A2(i,:)).^2)/Ndft;
  end
  printf("sd av %3.2f dB*dB\n", sum(sd)/frames);

  figure(1);
  clf;
  subplot(211)
  fs=fopen(raw_filename,"rb");
  s = fread(fs,Inf,"short");
  plot(s);
  subplot(212)
  plot(sd);

  % now enter single step mode so we can analyse each frame

  sn_name = strcat(dump_file_prefix,"_sn.txt");
  Sn = load(sn_name);

  lsp1_filename = sprintf("%s_lsp.txt", dump_file_prefix);
  lsp2_filename = sprintf("%s_lsp_.txt", dump_file_prefix);
  lsp1 = load(lsp1_filename);
  lsp2 = load(lsp2_filename);

  weights_filename = sprintf("%s_weights.txt", dump_file_prefix); 
  if file_in_path(".",weights_filename)
    weights = load(weights_filename);
  end

  k = ' ';
  do 
    figure(2);
    clf;
    subplot(211)
    s = [ Sn(2*f-1,:) Sn(2*f,:) ];
    size(s);
    plot(s);
    axis([1 length(s) -20000 20000]);

    subplot(212);
    plot((1:Ndft/2)*4000/(Ndft/2), A1(f,1:(Ndft/2)),";A1;r");
    axis([1 4000 -20 40]);
    hold on;
    plot((1:Ndft/2)*4000/(Ndft/2), A2(f,1:(Ndft/2)),";A2;");
    if file_in_path(".",weights_filename)
      plot(lsp1(f,:)*4000/pi, weights(f,:),";weights;g+");
    end

    for l=1:10
        plot([lsp1(f,l)*4000/pi lsp1(f,l)*4000/pi], [0  -10], 'r');
        plot([lsp2(f,l)*4000/pi lsp2(f,l)*4000/pi], [-10 -20], 'b');
    endfor
    plot(0,0,';lsp1;r');
    plot(0,0,';lsp2;b');
    sd_str = sprintf(";sd %3.2f dB*dB;", sd(f));
    plot(0,0,sd_str);
   
    hold off;

    % interactive menu

    printf("\rframe: %d  menu: n-next  b-back  q-quit", f);
    fflush(stdout);
    k = kbhit();
    if (k == 'n')
      f = f + 1;
    endif
    if (k == 'b')
      f = f - 1;
    endif

  until (k == 'q')
  printf("\n");

endfunction

