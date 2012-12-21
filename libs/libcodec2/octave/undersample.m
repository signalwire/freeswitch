% undersample.m
% David Rowe 5 Nov 2012
% Testing algorithms for plotting undersampled data for fdmdv2 waveform displays

fs=fopen("../raw/hts1a.raw","rb");
s = fread(fs,Inf,"short");

Fs1=8000;
Fs2=200;

M=Fs1/Fs2;

samples=length(s)/M;
s1=zeros(1,2*samples);
for b=1:samples
    st = (b-1)*M + 1;
    en = b*M;
    s1(2*b-1) = max(s(st:en));
    s1(2*b) = min(s(st:en));
end

subplot(211)
plot(s)
subplot(212)
plot(s1);

