% Copyright David Rowe 2009
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plot postfilter doing its thing

function postfilter(samname)	 
    p = load(samname);
    figure(1);
    plot(p(:,1),";energy;");
    hold on;
    plot(p(:,2),";bg_est;");
    hold off;
    grid;
    pngname=sprintf("%s_postfilter_1", samname);
    png(pngname);

    figure(2);
    plot(p(:,3),";% unvoiced;");
    grid;    
    pngname=sprintf("%s_postfilter_2", samname);
    png(pngname);
endfunction
  
