/*---------------------------------------------------------------------------*\
                                                                          
  FILE........: tcontphase.c                                                  
  AUTHOR......: David Rowe                                            
  DATE CREATED: 11/9/09                                        
                                                               
  Test program for developing continuous phase track synthesis algorithm.
  However while developing this it was discovered that synthesis_mixed()
  worked just as well.
                                                                   
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

#define N  80		/* frame size          */
#define F 160           /* frames to synthesis */
#define P  10           /* LPC order           */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "sine.h"
#include "dump.h"
#include "synth.h"
#include "phase.h"

int   frames;

float ak[] = {
 1.000000,
-1.455836,
 1.361841,
-0.879267,
 0.915985,
-1.002202,
 0.944103,
-0.743094,
 1.053356,
-0.817491,
 0.431222
};


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
    FILE *fout;
    short buf[N];
    int   i,j; 
    int   dump;
    float phi_prev[MAX_AMP];
    float Wo_prev, ex_phase, G;
    //float ak[P+1];
    COMP  H[MAX_AMP];  
    float f0;

    if (argc < 3) {
	printf("\nusage: %s OutputRawSpeechFile F0\n", argv[0]);
        exit(1);
    }

    /* Output file */

    if ((fout = fopen(argv[1],"wb")) == NULL) {
      printf("Error opening output speech file: %s\n",argv[1]);
      exit(1);
    }

    f0 = atof(argv[2]);

    dump = switch_present("--dump",argc,argv);
    if (dump) 
      dump_on(argv[dump+1]);

    init_decoder();

    for(i=0; i<MAX_AMP; i++)
	phi_prev[i] = 0.0;
    Wo_prev = 0.0;
	
    model.Wo = PI*(f0/4000.0);
    G = 1000.0;
    model.L = floor(PI/model.Wo);
    
    //aks_to_H(&model, ak, G , H, P);
    //for(i=1; i<=model.L; i++)
	model.A[i] = sqrt(H[i].real*H[i].real + H[i].imag*H[i].imag);
    //printf("L = %d\n", model.L);
    //model.L = 10;
    for(i=1; i<=model.L; i++) {
      model.A[i]   = 1000/model.L;
      model.phi[i] = 0;
      H[i].real = 1.0; H[i].imag = 0.0;
    }

    //ak[0] = 1.0;
    //for(i=1; i<=P; i++)
    //  ak[i] = 0.0;

    frames = 0;
    for(j=0; j<F; j++) {
	frames++;

	#ifdef SWAP
	/* lets make phases bounce around from frame to frame.  This
	   could happen if H[m] is varying, for example due to frame
	   to frame Wo variations, or non-stationary speech.
	   Continous model generally results in smooth phase track
	   under these circumstances. */
	if (j%2){
	    H[1].real = 1.0; H[1].imag = 0.0;
	    model.phi[1] = 0.0;
	}
	else {
	    H[1].real = 0.0; H[1].imag = 1.0;
	    model.phi[1] = PI/2;
	}
	#endif

	//#define CONT
	#ifdef CONT
	synthesise_continuous_phase(Pn, &model, Sn_, 1, &Wo_prev, phi_prev);
	#else
	phase_synth_zero_order(5.0, H, &Wo_prev, &ex_phase);
	synthesise_mixed(Pn,&model,Sn_,1);
	#endif

	for(i=0; i<N; i++)
	    buf[i] = Sn_[i];
	fwrite(buf,sizeof(short),N,fout);
    }

    fclose(fout);
    if (dump) dump_off();

    return 0;
}
 

