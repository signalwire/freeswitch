/*---------------------------------------------------------------------------*\
                                                                          
  FILE........: tnlp.c                                                  
  AUTHOR......: David Rowe                                            
  DATE CREATED: 23/3/93                                        
                                                               
  Test program for non linear pitch estimation functions.  
                                                                   
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2009 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#define N 80		/* frame size */
#define M 320		/* pitch analysis window size */
#define PITCH_MIN 20
#define PITCH_MAX 160
#define TNLP

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "defines.h"
#include "dump.h"
#include "sine.h"
#include "nlp.h"
#include "kiss_fft.h"

int   frames;

/*---------------------------------------------------------------------------*\
                                                                             
 switch_present()                                                            
                                                                             
 Searches the command line arguments for a "switch".  If the switch is       
 found, returns the command line argument where it ws found, else returns    
 NULL.                                                                       
                                                                             
\*---------------------------------------------------------------------------*/

int switch_present(sw,argc,argv)
  char sw[];     /* switch in string form */
  int argc;      /* number of command line arguments */
  char *argv[];  /* array of command line arguments in string form */
{
  int i;       /* loop variable */

  for(i=1; i<argc; i++)
    if (!strcmp(sw,argv[i]))
      return(i);

  return 0;
}

/*---------------------------------------------------------------------------*\

                                    MAIN

\*---------------------------------------------------------------------------*/

int main(argc,argv)
int argc;
char *argv[];
{
    FILE *fin,*fout;
    short buf[N];
    float Sn[M];	        /* float input speech samples */
    kiss_fft_cfg  fft_fwd_cfg;
    COMP  Sw[FFT_ENC];	        /* DFT of Sn[] */
    float w[M];	                /* time domain hamming window */
    COMP  W[FFT_ENC];	        /* DFT of w[] */
    float pitch;
    int   i; 
    float prev_Wo;
    void  *nlp_states;
#ifdef DUMP
    int   dump;
#endif

    if (argc < 3) {
	printf("\nusage: tnlp InputRawSpeechFile OutputPitchTextFile "
	       "[--dump DumpFile]\n");
        exit(1);
    }

    /* Input file */

    if ((fin = fopen(argv[1],"rb")) == NULL) {
      printf("Error opening input speech file: %s\n",argv[1]);
      exit(1);
    }

    /* Output file */

    if ((fout = fopen(argv[2],"wt")) == NULL) {
      printf("Error opening output text file: %s\n",argv[2]);
      exit(1);
    }

#ifdef DUMP
    dump = switch_present("--dump",argc,argv);
    if (dump) 
      dump_on(argv[dump+1]);
#else
/// TODO
/// #warning "Compile with -DDUMP if you expect to dump anything."
#endif

    nlp_states = nlp_create();
    fft_fwd_cfg = kiss_fft_alloc(FFT_ENC, 0, NULL, NULL);
    make_analysis_window(fft_fwd_cfg, w, W);

    frames = 0;
    prev_Wo = 0;
    while(fread(buf,sizeof(short),N,fin)) {
      printf("%d\n", frames++);

      /* Update input speech buffers */

      for(i=0; i<M-N; i++)
        Sn[i] = Sn[i+N];
      for(i=0; i<N; i++)
        Sn[i+M-N] = buf[i];
      dft_speech(fft_fwd_cfg, Sw, Sn, w);
#ifdef DUMP
      dump_Sn(Sn); dump_Sw(Sw); 
#endif

      nlp(nlp_states,Sn,N,M,PITCH_MIN,PITCH_MAX,&pitch,Sw,W, &prev_Wo);
      prev_Wo = TWO_PI/pitch;

      fprintf(fout,"%f\n",pitch);
    }

    fclose(fin);
    fclose(fout);
#ifdef DUMP
    if (dump) dump_off();
#endif
    nlp_destroy(nlp_states);

    return 0;
}
 

