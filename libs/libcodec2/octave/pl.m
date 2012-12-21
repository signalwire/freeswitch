% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plots a raw speech sample file, you can optionally specify the start and end
% samples and create a large and small PNGs

function pl(samname1, start_sam, end_sam, pngname)
  
  fs=fopen(samname1,"rb");
  s=fread(fs,Inf,"short");

  st = 1;
  en = length(s);
  if (nargin >= 2)
    st = start_sam;
  endif
  if (nargin >= 3)
    en = end_sam;
  endif

  figure(1);
  clf;
  plot(s(st:en));
  axis([1 en-st 1.1*min(s) 1.1*max(s)]);
 
  if (nargin == 4)

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

  endif

endfunction
