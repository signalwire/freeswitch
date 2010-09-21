% load_raw.m
% David Rowe 7 Oct 2009

function s = load_raw(fn)
  fs=fopen(fn,"rb");
  s = fread(fs,Inf,"short");
  plot(s)
endfunction
