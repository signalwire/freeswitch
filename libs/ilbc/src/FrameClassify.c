/*
 * iLBC - a library for the iLBC codec
 *
 * FrameClassify.c - The iLBC low bit rate speech codec.
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
 * $Id: FrameClassify.c,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <string.h>

#include "ilbc.h"
#include "FrameClassify.h"

/*---------------------------------------------------------------*
 *  Classification of subframes to localize start state
 *--------------------------------------------------------------*/

int FrameClassify(                                      /* index to the max-energy sub-frame */
                  ilbc_encode_state_t *iLBCenc_inst,    /* (i/o) the encoder state structure */
                  float *residual)                      /* (i) lpc residual signal */
{
    float max_ssqEn;
    float fssqEn[ILBC_NUM_SUB_MAX];
    float bssqEn[ILBC_NUM_SUB_MAX];
    float *pp;
    int n;
    int l;
    int max_ssqEn_n;
    static const float ssqEn_win[ILBC_NUM_SUB_MAX - 1] =
    {
        0.8f, 0.9f,
        1.0f, 0.9f, 0.8f
    };
    static const float sampEn_win[5]=
    {
        1.0f/6.0f,
        2.0f/6.0f, 3.0f/6.0f,
        4.0f/6.0f, 5.0f/6.0f
    };

    /* init the front and back energies to zero */
    memset(fssqEn, 0, ILBC_NUM_SUB_MAX*sizeof(float));
    memset(bssqEn, 0, ILBC_NUM_SUB_MAX*sizeof(float));

    /* Calculate front of first sequence */
    n = 0;
    pp = residual;
    for (l = 0;  l < 5;  l++)
    {
        fssqEn[n] += sampEn_win[l]*(*pp)*(*pp);
        pp++;
    }
    for (l = 5;  l < SUBL;  l++)
    {
        fssqEn[n] += (*pp)*(*pp);
        pp++;
    }

    /* Calculate front and back of all middle sequences */
    for (n = 1;  n < iLBCenc_inst->nsub - 1;  n++)
    {
        pp = residual + n*SUBL;
        for (l = 0;  l < 5;  l++)
        {
            fssqEn[n] += sampEn_win[l]*(*pp)*(*pp);
            bssqEn[n] += (*pp)*(*pp);
            pp++;
        }
        for (l = 5;  l < SUBL - 5;  l++)
        {
            fssqEn[n] += (*pp)*(*pp);
            bssqEn[n] += (*pp)*(*pp);
            pp++;
        }
        for (l = SUBL - 5; l < SUBL;  l++)
        {
            fssqEn[n] += (*pp)*(*pp);
            bssqEn[n] += sampEn_win[SUBL - l - 1]*(*pp)*(*pp);
            pp++;
        }
    }

    /* Calculate back of last sequence */
    n = iLBCenc_inst->nsub - 1;
    pp = residual + n*SUBL;
    for (l = 0;  l < SUBL - 5;  l++)
    {
        bssqEn[n] += (*pp)*(*pp);
        pp++;
    }
    for (l = SUBL - 5;  l < SUBL;  l++)
    {
        bssqEn[n] += sampEn_win[SUBL - l - 1]*(*pp)*(*pp);
        pp++;
    }

    /* find the index to the weighted 80 sample with
       most energy */
    l = (iLBCenc_inst->mode == 20)  ?  1  :  0;
    max_ssqEn = (fssqEn[0] + bssqEn[1])*ssqEn_win[l];
    max_ssqEn_n = 1;
    for (n = 2;  n < iLBCenc_inst->nsub;  n++)
    {
        l++;
        if ((fssqEn[n - 1] + bssqEn[n])*ssqEn_win[l] > max_ssqEn)
        {
            max_ssqEn = (fssqEn[n - 1] + bssqEn[n])*ssqEn_win[l];
            max_ssqEn_n = n;
        }
    }

    return max_ssqEn_n;
}
