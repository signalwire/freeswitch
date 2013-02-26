% phasesecord.m
% David Rowe Aug 2012
% Used to experiment with aproximations of phase of 2nd order systems

function phasesecord(w,beta)

  a = [1 -2*cos(w)*beta beta*beta];
  b = 1;

  [h w1] = freqz(b,a);

  figure(1)
  subplot(211)
  plot(abs(h))
  subplot(212)
  plot(angle(h))

  % for beta close to 1, we approximate 3 dB points as 1-beta above
  % and below the resonance freq. Note this fails if w=0 as there is a
  % double pole.  Lets sample the freq response at the 3dB points and
  % w:

  ws = [w-(1-beta) w w+(1-beta)];
  [h w1] = freqz(b,a,ws);

  % gain as a fraction of max, should be 3dB. Within 1.3 dB or for w > pi/8,
  % gets innacurate near w=0 due to 2nd pole

  printf("mag measured...:"); printf("% 4.3f ", abs(h)/max(abs(h)));

  % measured angle, 45 deg from angle at w

  printf("\nangle measured.: "); printf("% 5.3f ", angle(h));

  % Our estimate of angle, (pi+w) is phase at resonance, at lower 3dB
  % phase is pi/4 ahead, at upper 3B pi/4 behind.  -pi/2 is contribution of
  % other pole at at -w to phase

  ph_lower = (pi+w) + pi/4 - pi/2;
  ph_res   =(pi+w) - pi/2;
  ph_upper = (pi+w) - pi/4 - pi/2;
  ph_ests = [ph_lower ph_res ph_upper];
  ph_ests = ph_ests - 2*pi*(floor(ph_ests/(2*pi)) + 0.5);
  printf("\nangle estimated:"); printf("% 5.3f ", ph_ests);
  printf("\n");
endfunction

