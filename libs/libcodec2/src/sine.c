/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: sine.c
  AUTHOR......: David Rowe                                           
  DATE CREATED: 19/8/2010
                                                                             
  Sinusoidal analysis and synthesis functions.
                                                                             
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 1990-2010 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*---------------------------------------------------------------------------*\
                                                                             
				INCLUDES                                      
                                                                             
\*---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "defines.h"
#include "sine.h"
#include "four1.h"

/*---------------------------------------------------------------------------*\
                                                                             
				HEADERS                                     
                                                                             
\*---------------------------------------------------------------------------*/

void hs_pitch_refinement(MODEL *model, COMP Sw[], float pmin, float pmax, 
			 float pstep);

/*---------------------------------------------------------------------------*\
                                                                             
				FUNCTIONS                                     
                                                                             
\*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*\
                                                       
  FUNCTION....: make_analysis_window	     
  AUTHOR......: David Rowe			      
  DATE CREATED: 11/5/94 

  Init function that generates the time domain analysis window and it's DFT.

\*---------------------------------------------------------------------------*/

void make_analysis_window(float w[],COMP W[])
{
  float m;
  COMP  temp;
  int   i,j;

  /* 
     Generate Hamming window centered on M-sample pitch analysis window
  
  0            M/2           M-1
  |-------------|-------------|
        |-------|-------|
            NW samples

     All our analysis/synthsis is centred on the M/2 sample.               
  */

  m = 0.0;
  for(i=0; i<M/2-NW/2; i++)
    w[i] = 0.0;
  for(i=M/2-NW/2,j=0; i<M/2+NW/2; i++,j++) {
    w[i] = 0.5 - 0.5*cos(TWO_PI*j/(NW-1));
    m += w[i]*w[i];
  }
  for(i=M/2+NW/2; i<M; i++)
    w[i] = 0.0;
 
  /* Normalise - makes freq domain amplitude estimation straight
     forward */

  m = 1.0/sqrt(m*FFT_ENC);
  for(i=0; i<M; i++) {
    w[i] *= m;
  }

  /* 
     Generate DFT of analysis window, used for later processing.  Note
     we modulo FFT_ENC shift the time domain window w[], this makes the
     imaginary part of the DFT W[] equal to zero as the shifted w[] is
     even about the n=0 time axis if NW is odd.  Having the imag part
     of the DFT W[] makes computation easier.

     0                      FFT_ENC-1
     |-------------------------|

      ----\               /----
           \             / 
            \           /          <- shifted version of window w[n]
             \         /
              \       /
               -------

     |---------|     |---------|      
       NW/2              NW/2
  */

  for(i=0; i<FFT_ENC; i++) {
    W[i].real = 0.0;
    W[i].imag = 0.0;
  }
  for(i=0; i<NW/2; i++)
    W[i].real = w[i+M/2];
  for(i=FFT_ENC-NW/2,j=M/2-NW/2; i<FFT_ENC; i++,j++)
    W[i].real = w[j];

  four1(&W[-1].imag,FFT_ENC,-1);         /* "Numerical Recipes in C" FFT */

  /* 
      Re-arrange W[] to be symmetrical about FFT_ENC/2.  Makes later 
      analysis convenient.

   Before:


     0                 FFT_ENC-1
     |----------|---------|
     __                   _       
       \                 /          
        \_______________/      

   After:

     0                 FFT_ENC-1
     |----------|---------|
               ___                        
              /   \                
     ________/     \_______     

  */
       
      
  for(i=0; i<FFT_ENC/2; i++) {
    temp.real = W[i].real;
    temp.imag = W[i].imag;
    W[i].real = W[i+FFT_ENC/2].real;
    W[i].imag = W[i+FFT_ENC/2].imag;
    W[i+FFT_ENC/2].real = temp.real;
    W[i+FFT_ENC/2].imag = temp.imag;
  }

}

/*---------------------------------------------------------------------------*\
                                                       
  FUNCTION....: dft_speech	     
  AUTHOR......: David Rowe			      
  DATE CREATED: 27/5/94 

  Finds the DFT of the current speech input speech frame.

\*---------------------------------------------------------------------------*/

void dft_speech(COMP Sw[], float Sn[], float w[])
{
  int i;
  
  for(i=0; i<FFT_ENC; i++) {
    Sw[i].real = 0.0;
    Sw[i].imag = 0.0;
  }

  /* Centre analysis window on time axis, we need to arrange input
     to FFT this way to make FFT phases correct */
  
  /* move 2nd half to start of FFT input vector */

  for(i=0; i<NW/2; i++)
    Sw[i].real = Sn[i+M/2]*w[i+M/2];

  /* move 1st half to end of FFT input vector */

  for(i=0; i<NW/2; i++)
    Sw[FFT_ENC-NW/2+i].real = Sn[i+M/2-NW/2]*w[i+M/2-NW/2];

  four1(&Sw[-1].imag,FFT_ENC,-1);
}

/*---------------------------------------------------------------------------*\
                                                                     
  FUNCTION....: two_stage_pitch_refinement			
  AUTHOR......: David Rowe
  DATE CREATED: 27/5/94				

  Refines the current pitch estimate using the harmonic sum pitch
  estimation technique.

\*---------------------------------------------------------------------------*/

void two_stage_pitch_refinement(MODEL *model, COMP Sw[])
{
  float pmin,pmax,pstep;	/* pitch refinment minimum, maximum and step */ 

  /* Coarse refinement */

  pmax = TWO_PI/model->Wo + 5;
  pmin = TWO_PI/model->Wo - 5;
  pstep = 1.0;
  hs_pitch_refinement(model,Sw,pmin,pmax,pstep);
  
  /* Fine refinement */
  
  pmax = TWO_PI/model->Wo + 1;
  pmin = TWO_PI/model->Wo - 1;
  pstep = 0.25;
  hs_pitch_refinement(model,Sw,pmin,pmax,pstep);
  
  /* Limit range */
  
  if (model->Wo < TWO_PI/P_MAX)
    model->Wo = TWO_PI/P_MAX;
  if (model->Wo > TWO_PI/P_MIN)
    model->Wo = TWO_PI/P_MIN;

  model->L = floor(PI/model->Wo);
}

/*---------------------------------------------------------------------------*\
                                                                
 FUNCTION....: hs_pitch_refinement				
 AUTHOR......: David Rowe			
 DATE CREATED: 27/5/94							     
									  
 Harmonic sum pitch refinement function.			   
									    
 pmin   pitch search range minimum	    
 pmax	pitch search range maximum	    
 step   pitch search step size		    
 model	current pitch estimate in model.Wo  
									    
 model 	refined pitch estimate in model.Wo  
									     
\*---------------------------------------------------------------------------*/

void hs_pitch_refinement(MODEL *model, COMP Sw[], float pmin, float pmax, float pstep)
{
  int m;		/* loop variable */
  int b;		/* bin for current harmonic centre */
  float E;		/* energy for current pitch*/
  float Wo;		/* current "test" fundamental freq. */
  float Wom;		/* Wo that maximises E */
  float Em;		/* mamimum energy */
  float r;		/* number of rads/bin */
  float p;		/* current pitch */
  
  /* Initialisation */
  
  model->L = PI/model->Wo;	/* use initial pitch est. for L */
  Wom = model->Wo;
  Em = 0.0;
  r = TWO_PI/FFT_ENC;
  
  /* Determine harmonic sum for a range of Wo values */

  for(p=pmin; p<=pmax; p+=pstep) {
    E = 0.0;
    Wo = TWO_PI/p;

    /* Sum harmonic magnitudes */

    for(m=1; m<=model->L; m++) {
      b = floor(m*Wo/r + 0.5);
      E += Sw[b].real*Sw[b].real + Sw[b].imag*Sw[b].imag;
    }  

    /* Compare to see if this is a maximum */
    
    if (E > Em) {
      Em = E;
      Wom = Wo;
    }
  }

  model->Wo = Wom;
}

/*---------------------------------------------------------------------------*\
                                                                             
  FUNCTION....: estimate_amplitudes 			      
  AUTHOR......: David Rowe		
  DATE CREATED: 27/5/94			       
									      
  Estimates the complex amplitudes of the harmonics.    
									      
\*---------------------------------------------------------------------------*/

void estimate_amplitudes(MODEL *model, COMP Sw[], COMP W[])
{
  int   i,m;		/* loop variables */
  int   am,bm;		/* bounds of current harmonic */
  int   b;		/* DFT bin of centre of current harmonic */
  float den;		/* denominator of amplitude expression */
  float r;		/* number of rads/bin */
  int   offset;
  COMP  Am;

  r = TWO_PI/FFT_ENC;

  for(m=1; m<=model->L; m++) {
    den = 0.0;
    am = floor((m - 0.5)*model->Wo/r + 0.5);
    bm = floor((m + 0.5)*model->Wo/r + 0.5);
    b = floor(m*model->Wo/r + 0.5);

    /* Estimate ampltude of harmonic */

    den = 0.0;
    Am.real = Am.imag = 0.0;
    for(i=am; i<bm; i++) {
      den += Sw[i].real*Sw[i].real + Sw[i].imag*Sw[i].imag;
      offset = i + FFT_ENC/2 - floor(m*model->Wo/r + 0.5);
      Am.real += Sw[i].real*W[offset].real;
      Am.imag += Sw[i].imag*W[offset].real;
    }

    model->A[m] = sqrt(den);

    /* Estimate phase of harmonic */

    model->phi[m] = atan2(Sw[b].imag,Sw[b].real);
  }
}

/*---------------------------------------------------------------------------*\
                                                                             
  est_voicing_mbe()          
                                                                             
  Returns the error of the MBE cost function for a fiven F0.

  Note: I think a lot of the operations below can be simplified as
  W[].imag = 0 and has been normalised such that den always equals 1.
                                                                             
\*---------------------------------------------------------------------------*/

float est_voicing_mbe(
    MODEL *model,
    COMP   Sw[],
    COMP   W[],
    float  f0,
    COMP   Sw_[]          /* DFT of all voiced synthesised signal for f0 */
                          /* useful for debugging/dump file              */
)
{
    int   i,l,al,bl,m;    /* loop variables */
    COMP  Am;             /* amplitude sample for this band */
    int   offset;         /* centers Hw[] about current harmonic */
    float den;            /* denominator of Am expression */
    float error;          /* accumulated error between originl and synthesised */
    float Wo;             /* current "test" fundamental freq. */
    int   L;
    float sig, snr;

    sig = 0.0;
    for(l=1; l<=model->L/4; l++) {
	sig += model->A[l]*model->A[l];
    }

    for(i=0; i<FFT_ENC; i++) {
	Sw_[i].real = 0.0;
	Sw_[i].imag = 0.0;
    }

    L = floor((FS/2.0)/f0);
    Wo = f0*(TWO_PI/FS);

    error = 0.0;

    /* Just test across the harmonics in the first 1000 Hz (L/4) */

    for(l=1; l<=L/4; l++) {
	Am.real = 0.0;
	Am.imag = 0.0;
	den = 0.0;
	al = ceil((l - 0.5)*Wo*FFT_ENC/TWO_PI);
	bl = ceil((l + 0.5)*Wo*FFT_ENC/TWO_PI);

	/* Estimate amplitude of harmonic assuming harmonic is totally voiced */

	for(m=al; m<bl; m++) {
	    offset = FFT_ENC/2 + m - l*Wo*FFT_ENC/TWO_PI + 0.5;
	    Am.real += Sw[m].real*W[offset].real + Sw[m].imag*W[offset].imag;
	    Am.imag += Sw[m].imag*W[offset].real - Sw[m].real*W[offset].imag;
	    den += W[offset].real*W[offset].real + W[offset].imag*W[offset].imag;
        }

        Am.real = Am.real/den;
        Am.imag = Am.imag/den;

        /* Determine error between estimated harmonic and original */

        for(m=al; m<bl; m++) {
	    offset = FFT_ENC/2 + m - l*Wo*FFT_ENC/TWO_PI + 0.5;
	    Sw_[m].real = Am.real*W[offset].real - Am.imag*W[offset].imag;
	    Sw_[m].imag = Am.real*W[offset].imag + Am.imag*W[offset].real;
	    error += (Sw[m].real - Sw_[m].real)*(Sw[m].real - Sw_[m].real);
	    error += (Sw[m].imag - Sw_[m].imag)*(Sw[m].imag - Sw_[m].imag);
	}
    }

    snr = 10.0*log10(sig/error);
    if (snr > V_THRESH)
	model->voiced = 1;
    else
	model->voiced = 0;

    return snr;
}

/*---------------------------------------------------------------------------*\
                                                       
  FUNCTION....: make_synthesis_window	     
  AUTHOR......: David Rowe			      
  DATE CREATED: 11/5/94 

  Init function that generates the trapezoidal (Parzen) sythesis window.

\*---------------------------------------------------------------------------*/

void make_synthesis_window(float Pn[])
{
  int   i;
  float win;

  /* Generate Parzen window in time domain */

  win = 0.0;
  for(i=0; i<N/2-TW; i++)
    Pn[i] = 0.0;
  win = 0.0;
  for(i=N/2-TW; i<N/2+TW; win+=1.0/(2*TW), i++ )
    Pn[i] = win;
  for(i=N/2+TW; i<3*N/2-TW; i++)
    Pn[i] = 1.0;
  win = 1.0;
  for(i=3*N/2-TW; i<3*N/2+TW; win-=1.0/(2*TW), i++)
    Pn[i] = win;
  for(i=3*N/2+TW; i<2*N; i++)
    Pn[i] = 0.0;
}

/*---------------------------------------------------------------------------*\
                                                                             
  FUNCTION....: synthesise 			      
  AUTHOR......: David Rowe		
  DATE CREATED: 20/2/95		       
									      
  Synthesise a speech signal in the frequency domain from the
  sinusodal model parameters.  Uses overlap-add a triangular window to
  smoothly interpolate betwen frames.
									      
\*---------------------------------------------------------------------------*/

void synthesise(
  float  Sn_[],		/* time domain synthesised signal         */
  MODEL *model,		/* ptr to model parameters for this frame */
  float  Pn[],		/* time domain Parzen window              */
  int    shift          /* used to handle transition frames       */
)
{
    int   i,l,j,b;	/* loop variables */
    COMP  Sw_[FFT_DEC];	/* DFT of synthesised signal */

    if (shift) {
	/* Update memories */

	for(i=0; i<N-1; i++) {
	    Sn_[i] = Sn_[i+N];
	}
	Sn_[N-1] = 0.0;
    }

    for(i=0; i<FFT_DEC; i++) {
	Sw_[i].real = 0.0;
	Sw_[i].imag = 0.0;
    }

    /* Now set up frequency domain synthesised speech */

    for(l=1; l<=model->L; l++) {
	b = floor(l*model->Wo*FFT_DEC/TWO_PI + 0.5);
	Sw_[b].real = model->A[l]*cos(model->phi[l]);
	Sw_[b].imag = model->A[l]*sin(model->phi[l]);
	Sw_[FFT_DEC-b].real = Sw_[b].real;
	Sw_[FFT_DEC-b].imag = -Sw_[b].imag;
    }

    /* Perform inverse DFT */

    four1(&Sw_[-1].imag,FFT_DEC,1);

    /* Overlap add to previous samples */

    for(i=0; i<N-1; i++) {
	Sn_[i] += Sw_[FFT_DEC-N+1+i].real*Pn[i];
    }

    if (shift)
	for(i=N-1,j=0; i<2*N; i++,j++)
	    Sn_[i] = Sw_[j].real*Pn[i];
    else
	for(i=N-1,j=0; i<2*N; i++,j++)
	    Sn_[i] += Sw_[j].real*Pn[i];
}

