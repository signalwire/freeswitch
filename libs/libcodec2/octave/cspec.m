% cspec.m
% David Rowe Aug 2012
% Used to compare spectromgrams while experimenting with phase

function cspec(s1,s2)
  f1 = fopen(s1,"rb");
  s1 = fread(f1,Inf,"short");
  f2 = fopen(s2,"rb");
  s2 = fread(f2,Inf,"short");

  Fs = 8000;
  spec_win = 512;

  state = 's1';
  do 
    if strcmp(state,'s1')
      spec(s1,Fs,spec_win);
      %title(s1);
    end
    if strcmp(state,'s2')
      spec(s2,Fs,spec_win);
      %title(s2);
    end
    if strcmp(state,'diff')
      spec(s1-s2,Fs,spec_win);
      %title("difference");
    end

    printf("\rstate: %s  space-toggle d-diff q-quit", state);
    fflush(stdout);
    k = kbhit();
    
    if k == ' '
      if strcmp(state,"diff")
        next_state = 's1';
      end
      if strcmp(state,"s1")
        next_state = 's2';
      end
      if strcmp(state,'s2')
        next_state = 's1';
      end
    end

    if k == 'd'
      next_state = 'diff';
    end

    state = next_state;
  until (k == 'q')

  printf("\n");

endfunction
