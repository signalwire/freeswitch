% lsp_pdf.m
% David Rowe 2 Oct 2009
% Plots histograms (PDF estimates) of LSP training data

function lsp_pdf(lsp)
  [r,c] = size(lsp);

  % LSPs

  figure(1);
  clf;
  [x,y] = hist(lsp(:,1),100);
  plot(y*4000/pi,x,"+;1;");
  hold on;
  for i=2:5
    [x,y] = hist(lsp(:,i),100);
    legend = sprintf("+%d;%d;",i,i);
    plot(y*4000/pi,x,legend);
  endfor
  for i=6:c
    [x,y] = hist(lsp(:,i),100);
    legend = sprintf("+%d;%d;",i-5,i);
    plot(y*4000/pi,x,legend);
  endfor
  hold off;
  grid;

  % LSP differences

  figure(2);
  clf;
  subplot(211)
  [x,y] = hist(lsp(:,1),100);
  plot(y*4000/pi,x,"1;1;");
  hold on;
  for i=2:5
    [x,y] = hist(lsp(:,i) - lsp(:,i-1),100);
    legend = sprintf("%d;%d;",i,i);
    plot(y*4000/pi,x,legend);
  endfor
  hold off;
  grid;

  subplot(212)
  [x,y] = hist(lsp(:,6)-lsp(:,5),100);
  plot(y*4000/pi,x,"1;6;");
  hold on;
  for i=7:c
    [x,y] = hist(lsp(:,i) - lsp(:,i-1),100);
    legend = sprintf("%d;%d;",i-5,i);
    plot(y*4000/pi,x,legend);
  endfor
  hold off;
  grid;

  % LSP differences delta from last frame

  lspd(:,1) = lsp(:,1);
  lspd(:,2:10) = lsp(:,2:10) - lsp(:,1:9);

  [m,n] = size(lspd);
  lspdd = lspd(5:m,:) -  lspd(1:m-4,:);
  
  figure(3);
  clf;
  subplot(211)
  for i=1:5
    [x,y] = hist(lspdd(:,i),100);
    legend = sprintf("%d;%d;",i,i);
    plot(y*4000/pi,x,legend);
    hold on;
  endfor
  hold off;
  grid;
  axis([-200 200 0 35000]);

  subplot(212)
  for i=6:10
    [x,y] = hist(lspdd(:,i),100);
    legend = sprintf("%d;%d;",i-5,i);
    plot(y*4000/pi,x,legend);
    hold on;
  endfor
  hold off;
  grid;
  axis([-200 200 0 16000]);

  figure(4);
  clf;
  plot((4000/pi)*(lsp(2:r,3)-lsp(1:r-1,3)))
endfunction
