% lspwarp.m
% David Rowe Sep 2012
%
% Experimenting with non-linear LSP frequency axis for LSP quantisation
% Plots a scaled mel axis.

1;

function mel = freq2mel(f)
  mel = 70*log10(1 + f/700);
endfunction

function freq = mel2freq(m)
  freq = 700*(10 ^ (m/70) - 1);
endfunction

x = []; y = [];

for freq = 100:25:4000
  mel = freq2mel(freq);
  x = [x freq];
  y = [y mel];
end

plot(x,y)
grid

mel_start = floor(freq2mel(100));
mel_end = floor(freq2mel(4000));

x = []; y = [];
for mel=mel_start:mel_end
  freq = mel2freq(mel);
  x = [x freq];
  y = [y mel];
end

hold on;
plot(x,y, '+')
hold off;
