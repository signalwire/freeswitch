/*
 * iLBC - a library for the iLBC codec
 *
 * filter.h - The iLBC low bit rate speech codec.
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
 * $Id: filter.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_FILTER_H
#define __iLBC_FILTER_H

void AllPoleFilter(float *InOut,            /* (i/o) on entrance InOut[-orderCoef] to
                                                     InOut[-1] contain the state of the
                                                     filter (delayed samples). InOut[0] to
                                                     InOut[lengthInOut-1] contain the filter
                                                     input, on en exit InOut[-orderCoef] to
                                                     InOut[-1] is unchanged and InOut[0] to
                                                     InOut[lengthInOut-1] contain filtered
                                                     samples */
                   const float *Coef,       /* (i) filter coefficients, Coef[0] is assumed to be 1.0 */
                   int lengthInOut,         /* (i) number of input/output samples */
                   int orderCoef);          /* (i) number of filter coefficients */

void AllZeroFilter(float *In,               /* (i) In[0] to In[lengthInOut-1] contain
                                                   filter input samples */
                   const float *Coef,       /* (i) filter coefficients (Coef[0] is assumed to be 1.0) */
                   int lengthInOut,         /* (i) number of input/output samples */
                   int orderCoef,           /* (i) number of filter coefficients */
                   float *Out);             /* (i/o) on entrance Out[-orderCoef] to Out[-1]
                                                     contain the filter state, on exit Out[0]
                                                     to Out[lengthInOut-1] contain filtered
                                                     samples */

void ZeroPoleFilter(float *In,              /* (i) In[0] to In[lengthInOut-1] contain filter
                                                   input samples In[-orderCoef] to In[-1]
                                                   contain state of all-zero section */
                    const float *ZeroCoef,  /* (i) filter coefficients for all-zero
                                                   section (ZeroCoef[0] is assumed to
                                                   be 1.0) */
                    const float *PoleCoef,  /* (i) filter coefficients for all-pole section
                                                   (ZeroCoef[0] is assumed to be 1.0) */
                    int lengthInOut,        /* (i) number of input/output samples */
                    int orderCoef,          /* (i) number of filter coefficients */
                    float *Out);            /* (i/o) on entrance Out[-orderCoef] to Out[-1]
                                                     contain state of all-pole section. On
                                                     exit Out[0] to Out[lengthInOut-1]
                                                     contain filtered samples */

void DownSample(const float *In,            /* (i) input samples */
                const float *Coef,          /* (i) filter coefficients */
                int lengthIn,               /* (i) number of input samples */
                float *state,               /* (i) filter state */
                float *Out);                /* (o) downsampled output */

#endif
