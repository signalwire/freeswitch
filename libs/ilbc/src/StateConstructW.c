/*
 * iLBC - a library for the iLBC codec
 *
 * StateConstruct.c - The iLBC low bit rate speech codec.
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
 * $Id: StateConstructW.c,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "ilbc.h"
#include "constants.h"
#include "filter.h"
#include "StateConstructW.h"

/*----------------------------------------------------------------*
 *  decoding of the start state
 *---------------------------------------------------------------*/

void StateConstructW(int idxForMax,     /* (i) 6-bit index for the quantization of
                                               max amplitude */
                     int *idxVec,       /* (i) vector of quantization indexes */
                     float *syntDenum,  /* (i) synthesis filter denumerator */
                     float *out,        /* (o) the decoded state vector */
                     int len)           /* (i) length of a state vector */
{
    float maxVal;
    float tmpbuf[ILBC_LPC_FILTERORDER + 2*STATE_LEN];
    float *tmp;
    float numerator[ILBC_LPC_FILTERORDER + 1];
    float foutbuf[ILBC_LPC_FILTERORDER + 2*STATE_LEN];
    float *fout;
    int k;
    int tmpi;

    /* decoding of the maximum value */
    maxVal = state_frgqTbl[idxForMax];
    maxVal = powf(10.0f, maxVal)/4.5f;

    /* initialization of buffers and coefficients */

    memset(tmpbuf, 0, ILBC_LPC_FILTERORDER*sizeof(float));
    memset(foutbuf, 0, ILBC_LPC_FILTERORDER*sizeof(float));
    for (k = 0;  k < ILBC_LPC_FILTERORDER;  k++)
        numerator[k] = syntDenum[ILBC_LPC_FILTERORDER - k];
    numerator[ILBC_LPC_FILTERORDER] = syntDenum[0];
    tmp = &tmpbuf[ILBC_LPC_FILTERORDER];
    fout = &foutbuf[ILBC_LPC_FILTERORDER];

    /* decoding of the sample values */
    for (k = 0;  k < len;  k++)
    {
        tmpi = len - 1 - k;
        /* maxVal = 1/scal */
        tmp[k] = maxVal*state_sq3Tbl[idxVec[tmpi]];
    }

    /* circular convolution with all-pass filter */
    memset(tmp + len, 0, len*sizeof(float));
    ZeroPoleFilter(tmp, numerator, syntDenum, 2*len, ILBC_LPC_FILTERORDER, fout);
    for (k = 0;  k < len;  k++)
        out[k] = fout[len - 1 - k] + fout[2*len - 1 - k];
}
