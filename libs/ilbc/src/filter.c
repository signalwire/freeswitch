/*
 * iLBC - a library for the iLBC codec
 *
 * filter.c - The iLBC low bit rate speech codec.
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * iLBC code supplied in RFC3951.
 *
 * Original code Copyright (C) The Internet Society (2004).
 * All changes to produce this version Copyright (C) 2008 by Steve Underwood
 * All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: filter.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>

#include "iLBC_define.h"
#include "filter.h"

/*----------------------------------------------------------------*
 *  all-pole filter
 *---------------------------------------------------------------*/

void AllPoleFilter(float *InOut,   /* (i/o) on entrance InOut[-orderCoef] to
                                            InOut[-1] contain the state of the
                                            filter (delayed samples). InOut[0] to
                                            InOut[lengthInOut-1] contain the filter
                                            input, on en exit InOut[-orderCoef] to
                                            InOut[-1] is unchanged and InOut[0] to
                                            InOut[lengthInOut-1] contain filtered
                                            samples */
                   const float *Coef,   /* (i) filter coefficients, Coef[0] is assumed to be 1.0 */
                   int lengthInOut,     /* (i) number of input/output samples */
                   int orderCoef)       /* (i) number of filter coefficients */
{
    int n;
    int k;

    for (n = 0;  n < lengthInOut;  n++)
    {
        for (k = 1;  k <= orderCoef;  k++)
            *InOut -= Coef[k]*InOut[-k];
        InOut++;
    }
}

/*----------------------------------------------------------------*
 *  all-zero filter
 *---------------------------------------------------------------*/

void AllZeroFilter(float *In,           /* (i) In[0] to In[lengthInOut-1] contain
                                               filter input samples */
                   const float *Coef,   /* (i) filter coefficients (Coef[0] is assumed to be 1.0) */
                   int lengthInOut,     /* (i) number of input/output samples */
                   int orderCoef,       /* (i) number of filter coefficients */
                   float *Out)          /* (i/o) on entrance Out[-orderCoef] to Out[-1]
                                                 contain the filter state, on exit Out[0]
                                                 to Out[lengthInOut-1] contain filtered
                                                 samples */
{
    int n;
    int k;

    for (n = 0;  n < lengthInOut;  n++)
    {
        *Out = Coef[0]*In[0];
        for (k = 1;  k <= orderCoef;  k++)
            *Out += Coef[k]*In[-k];
        Out++;
        In++;
    }
}

/*----------------------------------------------------------------*
 *  pole-zero filter
 *---------------------------------------------------------------*/

void ZeroPoleFilter(float *In,              /* (i) In[0] to In[lengthInOut-1] contain
                                                   filter input samples In[-orderCoef] to
                                                   In[-1] contain state of all-zero
                                                   section */
                    const float *ZeroCoef,  /* (i) filter coefficients for all-zero
                                                   section (ZeroCoef[0] is assumed to
                                                   be 1.0) */
                    const float *PoleCoef,  /* (i) filter coefficients for all-pole section
                                                   (ZeroCoef[0] is assumed to be 1.0) */
                    int lengthInOut,        /* (i) number of input/output samples */
                    int orderCoef,          /* (i) number of filter coefficients */
                    float *Out)             /* (i/o) on entrance Out[-orderCoef] to Out[-1]
                                                     contain state of all-pole section. On
                                                     exit Out[0] to Out[lengthInOut-1]
                                                     contain filtered samples */
{
    AllZeroFilter(In, ZeroCoef, lengthInOut, orderCoef, Out);
    AllPoleFilter(Out, PoleCoef, lengthInOut, orderCoef);
}

/*----------------------------------------------------------------*
 * downsample (LP filter and decimation)
 *---------------------------------------------------------------*/

void DownSample(const float *In,        /* (i) input samples */
                const float *Coef,      /* (i) filter coefficients */
                int lengthIn,           /* (i) number of input samples */
                float *state,           /* (i) filter state */
                float *Out)             /* (o) downsampled output */
{
    float o;
    float *Out_ptr = Out;
    const float *Coef_ptr;
    const float *In_ptr;
    float *state_ptr;
    int i;
    int j;
    int stop;

    /* LP filter and decimate at the same time */
    for (i = DELAY_DS;  i < lengthIn;  i += FACTOR_DS)
    {
        Coef_ptr = &Coef[0];
        In_ptr = &In[i];
        state_ptr = &state[FILTERORDER_DS - 2];

        o = 0.0f;

        stop = (i < FILTERORDER_DS)  ?  (i + 1)  :  FILTERORDER_DS;

        for (j = 0;  j < stop;  j++)
            o += *Coef_ptr++ * (*In_ptr--);
        for (j = i + 1;  j < FILTERORDER_DS;  j++)
            o += *Coef_ptr++ * (*state_ptr--);
        *Out_ptr++ = o;
    }

    /* Get the last part (use zeros as input for the future) */
    for (i = (lengthIn + FACTOR_DS);  i < (lengthIn + DELAY_DS);  i += FACTOR_DS)
    {
        o = 0.0f;

        if (i<lengthIn)
        {
            Coef_ptr = &Coef[0];
            for (j = 0;  j < FILTERORDER_DS;  j++)
                o += *Coef_ptr++ * (*Out_ptr--);
        }
        else
        {
            Coef_ptr = &Coef[i - lengthIn];
            In_ptr = &In[lengthIn - 1];
            for (j = 0;  j < FILTERORDER_DS - (i - lengthIn);  j++)
                o += *Coef_ptr++ * (*In_ptr--);
        }
        *Out_ptr++ = o;
    }
}
