% Copyright David Rowe 2012
% This program is distributed under the terms of the GNU General Public License 
% Version 2
%
% codec2_demo.m

% Designed as an educational tool to explain the operation of Codec 2
% for conference and user group presentations on a projector.  An
% alternative to static overhead slides.
%
% Derived from codec2-dev/octave/plamp.m
%
% usage:
%   octave:1> plamp("../src/hts2a",40)
%
% Then press:
%   c - to cycle through the wavform being displayed on the figure
%   n - next frame
%   b - back one frame
%
%   tip: hold down n or b to animate the display
%
% The text files used as input are generated using c2sim:
%
%   /codec2-dev/src$ c2sim ../raw/hts2a.raw --dump hts2a
%
% The Codec 2 README explains how to build c2sim with dump files
% enabled.

function codec2_demo(samname, f)
  
  sn_name = strcat(samname,"_sn.txt");
  Sn = load(sn_name);

  sw_name = strcat(samname,"_sw.txt");
  Sw = load(sw_name);

  model_name = strcat(samname,"_model.txt");
  model = load(model_name);
  
  figure(1);

  k = ' ';
  wf = "Sn";
  do 
   
    if strcmp(wf,"Sn")
      clf;
      s = [ Sn(2*f-1,:) Sn(2*f,:) ];
      plot(s);
      axis([1 length(s) -20000 20000]);
    end

    if (strcmp(wf,"Sw"))
      clf;
      plot((0:255)*4000/256, Sw(f,:),";Sw;");
    end
  
    if strcmp(wf,"SwAm")
      Wo = model(f,1);
      L = model(f,2);
      Am = model(f,3:(L+2));
      plot((0:255)*4000/256, Sw(f,:),";Sw;");
      hold on;
      plot((1:L)*Wo*4000/pi, 20*log10(Am),"+;Am;r");
      axis([1 4000 -10 80]);
      hold off;
    end

    if strcmp(wf,"Am")
      Wo = model(f,1);
      L = model(f,2);
      Am = model(f,3:(L+2));
      plot((1:L)*Wo*4000/pi, 20*log10(Am),"+;Am;r");
      axis([1 4000 -10 80]);
    end

    % interactive menu

    printf("\rframe: %d  menu: n-next  b-back  w-cycle window  q-quit", f);
    fflush(stdout);
    k = kbhit();
    if (k == 'n')
      f = f + 1;
    end
    if (k == 'b')
      f = f - 1;
    end
    if (k == 'w') 
      if strcmp(wf,"Sn")
        next_wf = "Sw";
      end
      if strcmp(wf,"Sw")
        next_wf = "SwAm";
      end
      if strcmp(wf,"SwAm")
        next_wf = "Am";
      end
      if strcmp(wf,"Am")
        next_wf = "Sn";
      end
      wf = next_wf;
    end

  until (k == 'q')
  printf("\n");

endfunction
