% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Replot current plot as a png, generates small and large versions

function png(pngname)
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

endfunction
