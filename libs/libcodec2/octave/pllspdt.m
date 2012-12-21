% pllspdt.m
% Copyright David Rowe 2010
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Test script to plot differences in LSps between frames

function pllspdt(rawfile,dumpfile_prefix_lsp,lspn, start_f, end_f)
  
  fs=fopen(rawfile,"rb");
  s=fread(fs,Inf,"short");

  lsp_name = strcat(dumpfile_prefix_lsp,"_lsp.txt");
  lsps = load(lsp_name);
  [m,n]=size(lsps);
  lsp  = lsps(1:2:m,:);
  lsp_ = lsps(2:2:m,:);
  lspdt = lsp(2:m/2,:) - lsp(1:m/2-1,:);

  figure(1);
  clf;
  sp = s((start_f-2)*80:(end_f-2)*80);
  plot(sp);

  figure(2);
  plot((4000/pi)*lspdt((start_f+1):end_f,lspn));
endfunction
