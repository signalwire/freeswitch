/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * excquan.c -
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from code which is
 * Copyright 2000-2009 Broadcom Corporation
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: bv32excquan.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/*****************************************************************************
  Vector Quantizer for 2-Stage Noise Feedback Coding
  with long-term predictive noise feedback coding embedded
  inside the short-term predictive noise feedback coding loop.

  Note that the Noise Feedback Coding of the excitation signal is implemented
  using the Zero-State Responsse and Zero-input Response decomposition as
  described in: J.-H. Chen, "Novel Codec Structures for Noise Feedback
  Coding of Speech," Proc. ICASSP, 2006.
******************************************************************************/

#include "typedef.h"
#include "bv32cnst.h"
#include "bvcommon.h"
#include "bv32externs.h"

void bv32_excquan(Float *qv,     /* output quantized excitation signal vector */
                  int16_t *idx,  /* quantizer codebook index for uq[] vector */
                  Float *d,      /* input prediction residual signal vector */
                  Float *h,      /* noise feedback filter coefficient array */
                  Float *b,      /* coefficient of 3-tap pitch predictor */
                  Float beta,    /* coefficient of 1-tap LT noise feedback filter */
                  Float *ltsym,  /* long-term synthesis filter memory */
                  Float *ltnfm,  /* long-term noise feedback filter memory */
                  Float *stnfm,  /* short-term noise feedback filter memory */
                  Float *cb,     /* scalar quantizer codebook */
                  int pp)        /* pitch period (# of 8 kHz samples) */
{
    Float qzir[VDIM];
    Float zbuf[VDIM];
    Float buf[LPCO + SFRSZ]; /* buffer for filter memory & signal */
    Float a0;
    Float a1;
    Float *fp1;
    Float *fp2;
    Float *fp3;
    Float *fp4;
    Float sign = 1.0;
    Float ltfv[VDIM];
    Float ppv[VDIM];
    Float qzsr[VDIM*CBSZ];
    int i;
    int j;
    int m;
    int n;
    int jmin;
    int iv;
    Float E;
    Float Emin;
    Float e;

    /* COPY FILTER MEMORY TO BEGINNING PART OF TEMPORARY BUFFER */
    fp1 = &stnfm[LPCO - 1];
    for (i = 0;  i < LPCO;  i++)
        buf[i] = *fp1--;    /* this buffer is used to avoid memory shifts */

    /* COMPUTE CODEBOOK ZERO-STATE RESPONSE */
    fp2 = cb;
    fp3 = qzsr;
    for (j = 0;  j < CBSZ;  j++)
    {
        *fp3 = *fp2++;	/* no multiply-add needed for 1st ZSR vector element*/
        for (n = 1;  n < VDIM;  n++)   /* loop from 2nd to last vector element */
        {
            /* PERFORM MULTIPLY-ADDS ALONG THE DELAY LINE OF FILTER */
            fp1 = &h[n];
            fp4 = fp3;  /* fp4 --> first element of current ZSR vector */
            a0 = *fp2++;    /* initialize a0 to codebook element */
            for (i = 0;  i < n;  i++)
                a0 -= *fp4++ * *fp1--;
            *fp4 = a0; /* update short-term noise feedback filter memory */
        }
        fp3 += VDIM;    /* fp3 --> 1st element of next ZSR vector */
    }

    /* LOOP THROUGH EVERY VECTOR OF THE CURRENT SUBFRAME */
    iv = 0;     /* iv = index of the current vector */
    for (m = 0;  m < SFRSZ;  m += VDIM)
    {
        /* COMPUTE PITCH-PREDICTED VECTOR, WHICH SHOULD BE INDEPENDENT OF THE
           RESIDUAL VQ CODEVECTORS BEING TRIED IF VDIM < MIN. PITCH PERIOD */
        fp2 = ltfv;
        fp3 = ppv;
        for (n = m;  n < m + VDIM;  n++)
        {
            fp1 = &ltsym[MAXPP1 + n - pp + 1];
            a1  = b[0] * *fp1--;
            a1 += b[1] * *fp1--;
            a1 += b[2] * *fp1--;/* a1=pitch predicted vector of LT syn filt */
            *fp3++ = a1;            /* write result to ppv[] vector */

            *fp2++ = a1 + beta*ltnfm[MAXPP1 + n - pp];
        }

        /* COMPUTE ZERO-INPUT RESPONSE */
        fp2 = ppv;
        fp4 = ltfv;
        fp3 = qzir;
        for (n = m;  n < m + VDIM;  n++)
        {
            /* PERFORM MULTIPLY-ADDS ALONG THE DELAY LINE OF FILTER */
            fp1 = &buf[n];
            a0 = d[n];
            for (i = LPCO;  i > 0;  i--)
                a0 -= *fp1++ * h[i];

            /* a0 NOW CONTAINS v[n]; SUBTRACT THE SUM OF THE TWO LONG_TERM
               FILTERS TO GET THE ZERO-INPUT RESPONSE */
            *fp3++ = a0 - *fp4++;   /* q[n] = u[n] during ZIR computation */

            /* UPDATE SHORT-TERM NOISE FEEDBACK FILTER MEMORY */
            a0 -= *fp2++;    /* a0 now contains qs[n] */
            *fp1 = a0; /* update short-term noise feedback filter memory */
        }

        /* LOOP THROUGH EVERY CODEVECTOR OF THE RESIDUAL VQ CODEBOOK */
        /* AND FIND THE ONE THAT MINIMIZES THE ENERGY OF q[n] */

        Emin = 1.0e30;
        fp4 = qzsr;
        jmin = 0;
        for (j = 0;  j < CBSZ;  j++)
        {
            /* Try positive sign */
            fp2 = qzir;
            E = 0.0;
            for (n = 0;  n < VDIM;  n++)
            {
                e = *fp2++ - *fp4++; // sign impacted by negated ZSR
                E += e*e;
            }
            if (E < Emin)
            {
                jmin = j;
                Emin = E;
                sign = +1.0F;
            }
            /* Try negative sign */
            fp4 -= VDIM;
            fp2 = qzir;
            E = 0.0;
            for (n = 0;  n < VDIM;  n++)
            {
                e = *fp2++ + *fp4++; // sign impacted by negated ZSR
                E += e*e;
            }
            if (E < Emin)
            {
                jmin = j;
                Emin = E;
                sign = -1.0F;
            }
        }

        /* THE BEST CODEVECTOR HAS BEEN FOUND; ASSIGN VQ CODEBOOK INDEX */
        if (sign == 1.0F)
            idx[iv++] = jmin;
        else
            idx[iv++] = jmin + CBSZ; /* MSB of index is sign bit */

        /* BORROW zbuf[] TO STORE FINAL VQ OUTPUT VECTOR WITH CORRECT SIGN */
        fp3 = &cb[jmin*VDIM]; /* fp3 points to start of best codevector */
        for (n = 0;  n < VDIM;  n++)
            zbuf[n] = sign * *fp3++;

        /* UPDATE FILTER MEMORY */
        fp2 = ppv;      /* fp2 points to start of pitch-predicted vector */
        fp3 = zbuf;     /* fp3 points to start of final VQ output vector */
        fp4 = ltfv;     /* fp4 points to long-term filtered vector */

        /* LOOP THROUGH EVERY ELEMENT OF THE CURRENT VECTOR */
        for (n = m;  n < m + VDIM;  n++)
        {
            /* PERFORM MULTIPLY-ADDS ALONG THE DELAY LINE OF FILTER */
            fp1 = &buf[n];
            a0 = d[n];
            for (i = LPCO;  i > 0;  i--)
                a0 -= *fp1++ * h[i];

            /* COMPUTE VQ INPUT SIGNAL u[n] */
            a1 = a0 - *fp4++;   /* a1 now contains u[n] */

            /* COMPUTE VQ ERROR q[n] */
            a1 -= *fp3; /* a1 now contains VQ quantization error q[n] */

            /* UPDATE LONG-TERM NOISE FEEDBACK FILTER MEMORY */
            ltnfm[MAXPP1 + n] = a1;

            /* CALCULATE QUANTIZED LPC EXCITATION VECTOR qv[n] */
            qv[n] = (*fp3++ + *fp2++);

            /* UPDATE LONG-TERM PREDICTOR MEMORY */
            ltsym[MAXPP1 + n] = qv[n];

            /* COMPUTE ERROR BETWEEN v[n] AND qv[n] */
            a0 -= qv[n];    /* a0 now contains u[n] - qv[n] = qs[n] */

            /* UPDATE SHORT-TERM NOISE FEEDBACK FILTER MEMORY */
            *fp1 = a0;
        }
    }

    /* UPDATE NOISE FEEDBACK FILTER MEMORY AFTER FILTERING CURRENT SUBFRAME */
    for (i = 0;  i < LPCO;  i++)
        stnfm[i] = *fp1--;

    /* UPDATE LONG-TERM PREDICTOR MEMORY AFTER PROCESSING CURRENT SUBFRAME */
    fp2 = &ltnfm[SFRSZ];
    fp3 = &ltsym[SFRSZ];
    for (i = 0;  i < MAXPP1;  i++)
    {
        ltnfm[i] = fp2[i];
        ltsym[i] = fp3[i];
    }
}
