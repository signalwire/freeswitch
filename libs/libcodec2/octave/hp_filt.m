% hp_filt.m
% David Rowe 20 Feb 2012

function hp_filt(in_file, out_file)
  fin = fopen(in_file,"rb");
  s = fread(fin,Inf,"short");
  b = fir1(256, 300/4000, "high");
  freqz(b);
  s_hpf = filter(b,1,s);
  fout = fopen(out_file,"wb");
  fwrite(fout, s_hpf, "short");
endfunction
