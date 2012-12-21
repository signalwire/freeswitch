% Copyright David Rowe 2010
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% Plots a bunch of information related to LSP quantisation:
%   - speech file 
%   - LSPs before and after quantisation
%   - SNR for each frame
%
% Note: there is a 160 sample (two frame delay) from the when a sample
% enters the input buffer until it is at the centre of the analysis window

function pllsp(rawfile, 
	       dumpfile_prefix_lpc_only, 
               dumpfile_prefix_lsp, 
               start_f, end_f)
  
  fs=fopen(rawfile,"rb");
  s=fread(fs,Inf,"short");

  lpc_snr_name = strcat(dumpfile_prefix_lpc_only,"_lpc_snr.txt");
  lpc10_snr = load(lpc_snr_name);
  lpc_snr_name = strcat(dumpfile_prefix_lsp,"_lpc_snr.txt");
  lsp_snr = load(lpc_snr_name);

  lsp_name = strcat(dumpfile_prefix_lsp,"_lsp.txt");
  lsps = load(lsp_name);
  [m,n]=size(lsps);
  lsp  = lsps(1:2:m,:);
  lsp_ = lsps(2:2:m,:);

  figure(1);
  clf;
  subplot(211);
  sp = s((start_f-2)*80:(end_f-2)*80);
  plot(sp);

  subplot(212);
  plot(lpc10_snr((start_f+1):end_f)-lsp_snr((start_f+1):end_f));

  figure(2);
  plot((4000/pi)*lsp((start_f+1):end_f,:));
  hold on;
  plot((4000/pi)*lsp_((start_f+1):end_f,:),'+-');
  hold off;
endfunction
