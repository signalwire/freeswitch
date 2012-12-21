% gen_rn_coeffs.m
% David Rowe 13 april 2012
%
% Generate root raised cosine (Root Nyquist) filter coefficients
% thanks http://www.dsplog.com/db-install/wp-content/uploads/2008/05/raised_cosine_filter.m

function coeffs = gen_rn_coeffs(alpha, T, Rs, Nsym, M)

  Ts = 1/Rs;

  n = -Nsym*Ts/2:T:Nsym*Ts/2;
  Nfilter = Nsym*M;
  Nfiltertiming = M+Nfilter+M;

  sincNum = sin(pi*n/Ts); % numerator of the sinc function
  sincDen = (pi*n/Ts);    % denominator of the sinc function
  sincDenZero = find(abs(sincDen) < 10^-10);
  sincOp = sincNum./sincDen;
  sincOp(sincDenZero) = 1; % sin(pix/(pix) =1 for x =0

  cosNum = cos(alpha*pi*n/Ts);
  cosDen = (1-(2*alpha*n/Ts).^2);
  cosDenZero = find(abs(cosDen)<10^-10);
  cosOp = cosNum./cosDen;
  cosOp(cosDenZero) = pi/4;
  gt_alpha5 = sincOp.*cosOp;
  Nfft = 4096;
  GF_alpha5 = fft(gt_alpha5,Nfft)/M;

  % sqrt causes stop band to be amplified, this hack pushes it down again

  for i=1:Nfft
    if (abs(GF_alpha5(i)) < 0.02)
      GF_alpha5(i) *= 0.001;
    endif
  end
  GF_alpha5_root = sqrt(abs(GF_alpha5)) .* exp(j*angle(GF_alpha5));
  ifft_GF_alpha5_root = ifft(GF_alpha5_root);
  coeffs = real((ifft_GF_alpha5_root(1:Nfilter)));
endfunction
