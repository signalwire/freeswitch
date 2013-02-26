/*---------------------------------------------------------------------------*\
                                                                           
  FILE........: speexlsptest.c   
  AUTHOR......: David Rowe                                                      
  DATE CREATED: 24/8/09                                                   
                                                                          
  Test LPC to LSP conversion and quantisation using Speex LSP quantiser.
                                                                          
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <lpc.h>
#include <lsp.h>
#include <sd.h>

#define N 160
#define P 10

#define LPC_FLOOR 0.0002        /* autocorrelation floor */
#define LSP_DELTA1 0.2          /* grid spacing for LSP root searches */
#define NDFT    256             /* DFT size for SD calculation          */

/* Speex lag window */

const float lag_window[11] = {
   1.00000, 0.99716, 0.98869, 0.97474, 0.95554, 0.93140, 0.90273, 0.86998, 
   0.83367, 0.79434, 0.75258
};

/*---------------------------------------------------------------------------*\
                                                                            
  find_aks_for_lsp()
                                                          
  This function takes a frame of samples, and determines the linear           
  prediction coefficients for that frame of samples.  Modified version of
  find_aks from lpc.c to include autocorrelation noise floor and lag window
  to match Speex processing steps prior to LSP conversion.
                                                                            
\*---------------------------------------------------------------------------*/

void find_aks_for_lsp(
  float Sn[],	/* Nsam samples with order sample memory */
  float a[],	/* order+1 LPCs with first coeff 1.0 */
  int Nsam,	/* number of input speech samples */
  int order,	/* order of the LPC analysis */
  float *E	/* residual energy */
)
{
  float Wn[N];	/* windowed frame of Nsam speech samples */
  float R[P+1];	/* order+1 autocorrelation values of Sn[] */
  int i;

  hanning_window(Sn,Wn,Nsam);

  autocorrelate(Wn,R,Nsam,order);
  R[0] += LPC_FLOOR;
  assert(order == 10); /* lag window only defined for order == 10 */
  for(i=0; i<=order; i++)
      R[i] *= lag_window[i];
  levinson_durbin(R,a,order);

  *E = 0.0;
  for(i=0; i<=order; i++)
    *E += a[i]*R[i];
  if (*E < 0.0)
    *E = 1E-12;
}

/*---------------------------------------------------------------------------*\
                                                                            
                                MAIN 
                                   
\*---------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
  FILE *fin;            /* input speech files */
  short buf[N];         /* buffer of 16 bit speech samples */
  float Sn[P+N];        /* input speech samples */
  float E;
  float ak[P+1];        /* LP coeffs */
  float ak_[P+1];       /* quantised LP coeffs */
  float lsp[P];
  float lsp_[P];        /* quantised LSPs */
  int   roots;          /* number of LSP roots found */
  int frames;           /* frames processed so far */
  int i;                /* loop variables */

  SpeexBits bits;

  float  sd;            /* SD for this frame                */
  float  totsd;         /* accumulated SD so far            */
  int    gt2,gt4;       /* number of frames > 2 and 4 dB SD */
  int    unstables;     /* number of unstable LSP frames    */

  if (argc < 2) {
    printf("usage: %s InputFile\n", argv[0]);
    exit(0);
  }

  /* Open files */

  if ((fin = fopen(argv[1],"rb")) == NULL) {
    printf("Error opening input file: %s\n",argv[1]);
    exit(0);
  }

  /* Initialise */

  frames = 0;
  for(i=0; i<P; i++) {
    Sn[i] = 0.0;
  }
  ak_[0] = 1.0;

  speex_bits_init(&bits);

  totsd = 0.0;
  unstables = 0;
  gt2 = 0; gt4 = 0;

  /* Main loop */

  while( (fread(buf,sizeof(short),N,fin)) == N) {
    frames++;
    for(i=0; i<N; i++)
      Sn[P+i] = (float)buf[i];

    /* convert to LSP domain and back */

    find_aks(&Sn[P], ak, N, P, &E);
    roots = lpc_to_lsp(&ak[1], P , lsp, 10, LSP_DELTA1, NULL);
    if (roots == P) {

        speex_bits_reset(&bits);
	lsp_quant_lbr(lsp, lsp_, P, &bits);	
	lsp_to_lpc(lsp_, &ak_[1], P, NULL);
	
	/* measure spectral distortion */
	sd = spectral_dist(ak, ak_, P, NDFT);
	if (sd > 2.0) gt2++;
	if (sd > 4.0) gt4++;
	totsd += sd;
    }
    else
	unstables++;
  }

  fclose(fin);

  printf("frames = %d Av sd = %3.2f dB", frames, totsd/frames);
  printf("  >2 dB %3.2f%%  >4 dB %3.2f%%  unstables: %d\n",gt2*100.0/frames,
         gt4*100.0/frames, unstables);

  return 0;
}

