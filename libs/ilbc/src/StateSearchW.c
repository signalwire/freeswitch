/*
 * iLBC - a library for the iLBC codec
 *
 * StateSearchW.c - The iLBC low bit rate speech codec.
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
 * $Id: StateSearchW.c,v 1.2 2008/03/06 12:27:37 steveu Exp $
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
#include "helpfun.h"
#include "StateSearchW.h"

/*----------------------------------------------------------------*
 *  predictive noise shaping encoding of scaled start state
 *  (subrutine for StateSearchW)
 *---------------------------------------------------------------*/

void AbsQuantW(ilbc_encode_state_t *iLBCenc_inst,   /* (i) Encoder instance */
               float *in,                           /* (i) vector to encode */
               float *syntDenum,                    /* (i) denominator of synthesis filter */
               float *weightDenum,                  /* (i) denominator of weighting filter */
               int *out,                            /* (o) vector of quantizer indexes */
               int len,                             /* (i) length of vector to encode and
                                                           vector of quantizer indexes */
               int state_first)                     /* (i) position of start state in the 80 vec */
{
    float *syntOut;
    float syntOutBuf[ILBC_LPC_FILTERORDER + STATE_SHORT_LEN_30MS];
    float toQ;
    float xq;
    int n;
    int index;

    /* initialization of buffer for filtering */
    memset(syntOutBuf, 0, ILBC_LPC_FILTERORDER*sizeof(float));

    /* initialization of pointer for filtering */
    syntOut = &syntOutBuf[ILBC_LPC_FILTERORDER];

    /* synthesis and weighting filters on input */
    if (state_first)
    {
        AllPoleFilter(in, weightDenum, SUBL, ILBC_LPC_FILTERORDER);
    }
    else
    {
        AllPoleFilter(in, weightDenum,
                      iLBCenc_inst->state_short_len - SUBL,
                      ILBC_LPC_FILTERORDER);
    }

    /* encoding loop */
    for (n = 0;  n < len;  n++)
    {
        /* time update of filter coefficients */
        if ((state_first)  &&  (n == SUBL))
        {
            syntDenum += (ILBC_LPC_FILTERORDER + 1);
            weightDenum += (ILBC_LPC_FILTERORDER + 1);

            /* synthesis and weighting filters on input */
            AllPoleFilter(&in[n], weightDenum, len - n, ILBC_LPC_FILTERORDER);
        }
        else if ((state_first == 0)
                 &&
                 (n == (iLBCenc_inst->state_short_len - SUBL)))
        {
            syntDenum += (ILBC_LPC_FILTERORDER + 1);
            weightDenum += (ILBC_LPC_FILTERORDER + 1);

            /* synthesis and weighting filters on input */
            AllPoleFilter(&in[n], weightDenum, len - n, ILBC_LPC_FILTERORDER);
        }
        /* prediction of synthesized and weighted input */
        syntOut[n] = 0.0f;
        AllPoleFilter(&syntOut[n], weightDenum, 1, ILBC_LPC_FILTERORDER);

        /* quantization */
        toQ = in[n] - syntOut[n];
        sort_sq(&xq, &index, toQ, state_sq3Tbl, 8);
        out[n] = index;
        syntOut[n] = state_sq3Tbl[out[n]];

        /* update of the prediction filter */
        AllPoleFilter(&syntOut[n], weightDenum, 1, ILBC_LPC_FILTERORDER);
    }
}

/*----------------------------------------------------------------*
 *  encoding of start state
 *---------------------------------------------------------------*/

void StateSearchW(ilbc_encode_state_t *iLBCenc_inst,    /* (i) Encoder instance */
                  float *residual,                      /* (i) target residual vector */
                  float *syntDenum,                     /* (i) lpc synthesis filter */
                  float *weightDenum,                   /* (i) weighting filter denuminator */
                  int *idxForMax,                       /* (o) quantizer index for maximum
                                                               amplitude */
                  int *idxVec,                          /* (o) vector of quantization indexes */
                  int len,                              /* (i) length of all vectors */
                  int state_first)                      /* (i) position of start state in the 80 vec */
{
    float dtmp;
    float maxVal;
    float tmpbuf[ILBC_LPC_FILTERORDER + 2*STATE_SHORT_LEN_30MS];
    float *tmp;
    float numerator[ILBC_LPC_FILTERORDER + 1];
    float foutbuf[ILBC_LPC_FILTERORDER + 2*STATE_SHORT_LEN_30MS];
    float *fout;
    int k;
    float qmax;
    float scal;

    /* initialization of buffers and filter coefficients */

    memset(tmpbuf, 0, ILBC_LPC_FILTERORDER*sizeof(float));
    memset(foutbuf, 0, ILBC_LPC_FILTERORDER*sizeof(float));
    for (k = 0;  k < ILBC_LPC_FILTERORDER;  k++)
        numerator[k] = syntDenum[ILBC_LPC_FILTERORDER - k];
    numerator[ILBC_LPC_FILTERORDER] = syntDenum[0];
    tmp = &tmpbuf[ILBC_LPC_FILTERORDER];
    fout = &foutbuf[ILBC_LPC_FILTERORDER];

    /* circular convolution with the all-pass filter */
    memcpy(tmp, residual, len*sizeof(float));
    memset(tmp + len, 0, len*sizeof(float));
    ZeroPoleFilter(tmp, numerator, syntDenum, 2*len, ILBC_LPC_FILTERORDER, fout);
    for (k = 0;  k < len;  k++)
        fout[k] += fout[k+len];

    /* identification of the maximum amplitude value */
    maxVal = fout[0];
    for (k = 1;  k < len;  k++)
    {
        if (fout[k]*fout[k] > maxVal*maxVal)
            maxVal = fout[k];
    }
    maxVal = fabsf(maxVal);

    /* encoding of the maximum amplitude value */
    if (maxVal < 10.0f)
        maxVal = 10.0f;
    maxVal = log10f(maxVal);
    sort_sq(&dtmp, idxForMax, maxVal, state_frgqTbl, 64);

    /* decoding of the maximum amplitude representation value,
       and corresponding scaling of start state */
    maxVal = state_frgqTbl[*idxForMax];
    qmax = powf(10.0f, maxVal);
    scal = 4.5f/qmax;
    for (k = 0;  k < len;  k++)
        fout[k] *= scal;

    /* predictive noise shaping encoding of scaled start state */
    AbsQuantW(iLBCenc_inst, fout,syntDenum, weightDenum,idxVec, len, state_first);
}
