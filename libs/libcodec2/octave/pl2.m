% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2

function pl2(samname1, samname2, start_sam, end_sam, pngname)
  
  fs1=fopen(samname1,"rb");
  s1=fread(fs1,Inf,"short");
  fs2=fopen(samname2,"rb");
  s2=fread(fs2,Inf,"short");

  st = 1;
  en = length(s1);
  if (nargin >= 3)
    st = start_sam;
  endif
  if (nargin >= 4)
    en = end_sam;
  endif

  figure(1);
  clf;
  subplot(211);
  l1 = strcat("r;",samname1,";");
  plot(s1(st:en), l1);
  axis([1 en-st min(s1(st:en)) max(s1(st:en))]);
  subplot(212);
  l2 = strcat("r;",samname2,";");
  plot(s2(st:en),l2);
  axis([1 en-st min(s1(st:en)) max(s1(st:en))]);
 
  if (nargin == 5)

    % small image

    __gnuplot_set__ terminal png size 420,300
    s = sprintf("__gnuplot_set__ output \"%s.png\"", pngname);
    eval(s)
    replot;

    % larger image

    __gnuplot_set__ terminal png size 800,600
    s = sprintf("__gnuplot_set__ output \"%s_large.png\"", pngname);
    eval(s)
    replot;

  endif

endfunction
