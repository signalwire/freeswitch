% Copyright David Rowe 2010
% This program is distributed under the terms of the GNU General Public License 
% Version 2
% plots the difference of two files

function plsub(samname1, samname2, start_sam, end_sam, pngname)
  
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
  l1 = strcat("r;",samname1,";");
  plot(s1(st:en) - s2(st:en), l1);
  %axis([1 en-st min(s1(st:en)) max(s1(st:en))]);
 
  if (nargin == 5)
    pngname = sprintf("%s.png",pngname);
    print(pngname, '-dpng', "-S500,500")
    pngname = sprintf("%s_large.png",pngname);
    print(pngname, '-dpng', "-S800,600")
  endif

endfunction
