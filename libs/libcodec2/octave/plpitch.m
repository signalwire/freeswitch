% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% plpitch.m
% Plots two pitch tracks on top of each other, used for comparing pitch
% estimators

function plpitch(pitch1_name, pitch2_name, start_fr, end_fr)
  
  pitch1 = load(pitch1_name);
  pitch2 = load(pitch2_name);

  st = 1;
  en = length(pitch1);
  if (nargin >= 3)
    st = start_fr;
  endif
  if (nargin >= 4)
    en = end_fr;
  endif

  figure(1);
  clf;
  l1 = strcat("r;",pitch1_name,";")
  l1 
  st
  en
  plot(pitch1(st:en), l1);
  axis([1 en-st 20 160]);
  l2 = strcat("g;",pitch2_name,";");
  hold on;
  plot(pitch2(st:en),l2);
  hold off;
endfunction

