% lsp_pdf.m
% David Rowe 2 Oct 2009
% Plots histograms (PDF estimates) of LSP training data

function lsp_pdf(lsp)
  [r,c] = size(lsp);

  % LSPs

  figure(3);
  clf;
  [x,y] = hist(lsp(:,1),100);
  plot(y*4000/pi,x,";1;");
  hold on;
  for i=2:c
    [x,y] = hist(lsp(:,i),100);
    legend = sprintf(";%d;",i);
    plot(y*4000/pi,x,legend);
  endfor
  hold off;
  grid;

  % LSP differences

  figure(4);
  clf;
  subplot(211)
  [x,y] = hist(lsp(:,1),100);
  plot(y,x,";1;");
  hold on;
  for i=2:5
    [x,y] = hist(lsp(:,i) - lsp(:,i-1),100);
    legend = sprintf(";%d;",i);
    plot(y,x,legend);
  endfor
  hold off;
  grid;

  subplot(212)
  [x,y] = hist(lsp(:,6)-lsp(:,5),100);
  plot(y,x,";6;");
  hold on;
  for i=7:c
    [x,y] = hist(lsp(:,i) - lsp(:,i-1),100);
    legend = sprintf(";%d;",i);
    plot(y,x,legend);
  endfor
  hold off;
  grid;
endfunction
