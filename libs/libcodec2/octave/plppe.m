% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plot two sparse phase prediction error text files.
% Generate data from print_pred_error, print_pred_error_sparse_wo_correction1 etc

function plppe(ppe1_file, ppe2_file, f)
  
  ppe1 = load(ppe1_file);
  ppe2 = load(ppe2_file);

  std1 = std(nonzeros(ppe1(:,40:80)));
  std2 = std(nonzeros(ppe2(:,40:80)));

  printf("std dev for %s is %4.3f\n", ppe1_file, std1);
  printf("std dev for %s is %4.3f\n", ppe2_file, std2);
 
  figure(1);
  clf;
  subplot(211)
  hist(nonzeros(ppe1(:,40:80)),20);
  subplot(212)
  hist(nonzeros(ppe2(:,40:80)),20);
  
  k = ' ';
  do 
    figure(2);
    clf;
    subplot(211)
    L = length(nonzeros(ppe1(f,:)));
    x = (1:L)*4000/L;
    std1 = std(nonzeros(ppe1(f,:)));
    legend = sprintf(";std dev %4.3f;", std1);
    plot(x, nonzeros(ppe1(f,:)),legend);
    axis([0 4000 -pi pi]);
    subplot(212)
    std2 = std(nonzeros(ppe2(f,:)));
    legend = sprintf(";std dev %4.3f;", std2);
    plot(x, nonzeros(ppe2(f,:)),legend);
    axis([0 4000 -pi pi]);

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
       png(pngname);
    endif

  until (k == 'q')
  printf("\n");

endfunction
