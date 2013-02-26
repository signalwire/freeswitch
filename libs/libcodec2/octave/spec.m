% spec.m
% Jean Marc Valin
%
% Spectrogram function for Octave 
%
%   Copyright (c) John-Marc Valin 2012
%
%   Redistribution and use in source and binary forms, with or without
%   modification, are permitted provided that the following conditions
%   are met:
%   
%   - Redistributions of source code must retain the above copyright
%   notice, this list of conditions and the following disclaimer.
%   
%   - Redistributions in binary form must reproduce the above copyright
%   notice, this list of conditions and the following disclaimer in the
%   documentation and/or other materials provided with the distribution.
%   
%   - Neither the name of Jean Marc Valin nor the names of its
%   contributors may be used to endorse or promote products derived from
%   this software without specific prior written permission.
%   
%   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
%   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
%   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
%   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
%   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
%   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
%   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
%   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
%   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
%   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
%   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function X = spec(x, Fs, framesize, start, stop)


gr=[zeros(1024,1),[0:1023]'/1023,.68*[0:1023]'/1023];

%gr=[.4*[0:1023]'/1023,[0:1023]'/1023,.68*[0:1023]'/1023];

%t=[0:1023]'/1023;
%t=(1+.25*t-cos(pi*t))/2.25;
%gr = [zeros(1024,1),t,.68*t];


%colormap(gr);

if nargin < 2 || isempty(Fs)
   Fs = 44100;
end

if nargin < 3 || isempty(framesize)
   framesize = 2048;
endif

offset = framesize/4;

X = 20*log10(abs(specgram(x, framesize, 48000, blackmanharris(framesize)', framesize-offset)));

XM=max(max(X));
X = max(XM-120,X);
%size(X)
F = -[framesize/2-1:-1:0]/framesize*Fs;
%F = [0:127]/128*24000;
T=[1:size(X,2)]/Fs*offset;
%imagesc(X(end:-1:1,:));

if nargin < 4 || isempty(start)
   istart=1;
else
   istart = round(start*Fs/offset);
end

if nargin < 5 || isempty(stop)
   istop = size(X,2);
else
   istop = round(stop*Fs/offset);
endif

istart = max(1,istart);
istop = min(istop, size(X,2));

imagesc(T(1+istart:istop), F, X(end:-1:1,1+istart:istop));

X = X(:,1+istart:istop);
