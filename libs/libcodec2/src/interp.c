/*---------------------------------------------------------------------------*\

  FILE........: interp.c
  AUTHOR......: David Rowe
  DATE CREATED: 9/10/09

  Interpolation of 20ms frames to 10ms frames.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2009 David Rowe

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

#include <assert.h>
#include <math.h>
#include <string.h>

#include "defines.h"
#include "interp.h"

float sample_log_amp(MODEL *model, float w);

/*---------------------------------------------------------------------------*\

  FUNCTION....: interp()	     
  AUTHOR......: David Rowe			      
  DATE CREATED: 22/8/10 
        
  Given two frames decribed by model parameters 20ms apart, determines
  the model parameters of the 10ms frame between them.  Assumes
  voicing is available for middle (interpolated) frame.  Outputs are
  amplitudes and Wo for the interpolated frame.

  This version can interpolate the amplitudes between two frames of
  different Wo and L.
  
\*---------------------------------------------------------------------------*/

void interpolate(
  MODEL *interp,    /* interpolated model params                     */
  MODEL *prev,      /* previous frames model params                  */
  MODEL *next       /* next frames model params                      */
)
{
    int   l;
    float w,log_amp;

    /* Wo depends on voicing of this and adjacent frames */

    if (interp->voiced) {
	if (prev->voiced && next->voiced)
	    interp->Wo = (prev->Wo + next->Wo)/2.0;
	if (!prev->voiced && next->voiced)
	    interp->Wo = next->Wo;
	if (prev->voiced && !next->voiced)
	    interp->Wo = prev->Wo;
    }
    else {
	interp->Wo = TWO_PI/P_MAX;
    }
    interp->L = PI/interp->Wo;

    /* Interpolate amplitudes using linear interpolation in log domain */

    for(l=1; l<=interp->L; l++) {
	w = l*interp->Wo;
	log_amp = (sample_log_amp(prev, w) + sample_log_amp(next, w))/2.0;
	interp->A[l] = pow(10.0, log_amp);
    }
}

/*---------------------------------------------------------------------------*\

  FUNCTION....: sample_log_amp()
  AUTHOR......: David Rowe			      
  DATE CREATED: 22/8/10 
        
  Samples the amplitude envelope at an arbitrary frequency w.  Uses
  linear interpolation in the log domain to sample between harmonic
  amplitudes.
  
\*---------------------------------------------------------------------------*/

float sample_log_amp(MODEL *model, float w)
{
    int   m;
    float f, log_amp;

    assert(w > 0.0); assert (w <= PI);

    m = floor(w/model->Wo + 0.5);
    f = (w - m*model->Wo)/w;
    assert(f <= 1.0);

    if (m < 1) {
	log_amp = f*log10(model->A[1]);
    }
    else if ((m+1) > model->L) {
	log_amp = (1.0-f)*log10(model->A[model->L]);
    }
    else {
	log_amp = (1.0-f)*log10(model->A[m]) + f*log10(model->A[m+1]);
    }

    return log_amp;
}

