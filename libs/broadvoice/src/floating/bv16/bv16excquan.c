/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv16excquan.c : Vector Quantizer for 2-Stage Noise Feedback Coding
 *                 with long-term predictive noise feedback coding embedded
 *                 inside the int16_t-term predictive noise feedback coding loop.
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
 * $Id: bv16excquan.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include "typedef.h"
#include "bv16cnst.h"
#include "bvcommon.h"
#include "bv16externs.h"

void excquan(int16_t *idx, /* quantizer codebook index for uq[] vector */
             Float *s,     /* input speech signal vector */
             Float *aq,    /* int16_t-term predictor coefficient array */
             Float *fsz,   /* int16_t-term noise feedback filter - numerator */
             Float *fsp,   /* int16_t-term noise feedback filter - denominator */
             Float *b,     /* coefficient of 3-tap pitch predictor */
             Float beta,   /* coefficient of 1-tap LT noise feedback filter */
             Float *stsym, /* int16_t-term synthesis filter memory */
             Float *ltsym, /* long-term synthesis filter memory */
             Float *ltnfm, /* long-term noise feedback filter memory */
             Float *stnfz,
             Float *stnfp,
             Float *cb,    /* scalar quantizer codebook */
             int pp)       /* pitch period */
{
    Float qzir[VDIM];               /* Zero-input response */
    Float qzsr[VDIM*CBSZ];          /* Negated zero-state response of codebook */
    Float uq[VDIM];                 /* Selected codebook vector (incl. sign) */
    Float buf1[LPCO + FRSZ];        /* Buffer for filter memory & signal */
    Float buf2[NSTORDER + FRSZ];    /* Buffer for filter memory */
    Float buf3[NSTORDER + FRSZ];    /* Buffer for filter memory */
    Float buf4[VDIM];               /* Buffer for filter memory */
    Float a0;
    Float a1;
    Float a2;
    Float *fp1;
    Float *fp2;
    Float *fp3;
    Float *fp4;
    Float sign;
    Float *fpa;
    Float *fpb;
    Float ltfv[VDIM];
    Float ppv[VDIM];
    int i;
    int j;
    int m;
    int n;
    int jmin;
    int iv;
    Float buf5[VDIM];           /* Buffer for filter memory */
    Float buf6[VDIM];           /* Buffer for filter memory */
    Float e;
    Float E;
    Float Emin;
    Float *p_ppv;
    Float *p_ltfv;
    Float *p_uq;
    Float v;

    /* copy filter memory to beginning part of temporary buffer */
    fp1 = &stsym[LPCO - 1];
    for (i = 0;  i < LPCO;  i++)
        buf1[i] = *fp1--;    /* this buffer is used to avoid memory shifts */

    /* copy noise feedback filter memory */
    fp1 = &stnfz[NSTORDER - 1];
    fp2 = &stnfp[NSTORDER - 1];
    for (i = 0;  i < NSTORDER;  i++)
    {
        buf2[i] = *fp1--;
        buf3[i] = *fp2--;
    }

    /************************************************************************************/
    /*                       Z e r o - S t a t e   R e s p o n s e                      */
    /************************************************************************************/
    /* Calculate negated Zero State Response */
    fp2 = cb;   /* fp2 points to start of first codevector */
    fp3 = qzsr; /* fp3 points to start of first zero-state response vector */

    /* For each codevector */
    for (j = 0;  j < CBSZ;  j++)
    {
        /* Calculate the elements of the negated ZSR */
        for (i = 0;  i < VDIM;  i++)
        {
            /* int16_t-term prediction */
            a0 = 0.0;
            fp1 = buf4;
            for (n = i;  n > 0;  n--)
                a0 -= *fp1++ * aq[n];

            /* Update memory of int16_t-term prediction filter */
            *fp1++ = a0 + *fp2;

            /* noise feedback filter */
            a1 = 0.0;
            fpa = buf5;
            fpb = buf6;
            for (n = i;  n > 0;  n--)
                a1 += ((*fpa++ * fsz[n]) - (*fpb++ * fsp[n]));

            /* Update memory of pole section of noise feedback filter */
            *fpb++ = a1;

            /* ZSR */
            *fp3 = *fp2++ + a0 + a1;

            /* Update memory of zero section of noise feedback filter */
            *fpa++ = -(*fp3++);
        }
    }

    /* loop through every vector of the current subframe */
    iv = 0;     /* iv = index of the current vector */
    for (m = 0;  m < FRSZ;  m += VDIM)
    {
        /********************************************************************************/
        /*                     Z e r o - I n p u t   R e s p o n s e                    */
        /********************************************************************************/
        /* compute pitch-predicted vector, which should be independent of the
        residual vq codevectors being tried if vdim < min. pitch period */
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

        /* compute zero-input response */
        fp2 = ppv;
        fp4 = ltfv;
        fp3 = qzir;
        for (n = m;  n < m + VDIM;  n++)
        {
            /* perform multiply-adds along the delay line of the predictor */
            fp1 = &buf1[n];
            a0 = 0.;
            for (i = LPCO;  i > 0;  i--)
                a0 -= *fp1++ * aq[i];

            /* perform multiply-adds along the noise feedback filter */
            fpa = &buf2[n];
            fpb = &buf3[n];
            a1 = 0.;
            for (i = NSTORDER;  i > 0;  i--)
                a1 += ((*fpa++ * fsz[i]) - (*fpb++ * fsp[i]));
            *fpb = a1;		/* update output of the noise feedback filter */

            a2 = s[n] - (a0 + a1);		/* v[n] */

            /* a2 now contains v[n]; subtract the sum of the two long-term
            filters to get the zero-input response */
            *fp3++ = a2 - *fp4++;	/* q[n] = u[n] during ZIR computation */

            /* update int16_t-term noise feedback filter memory */
            a0 += *fp2;		 /* a0 now conatins the qs[n] */
            *fp1 = a0;
            a2 -= *fp2++;    /* a2 now contains qszi[n] */
            *fpa = a2; /* update int16_t-term noise feedback filter memory */
        }

        /********************************************************************************/
        /*                         S e a r c h   C o d e b o o k                        */
        /********************************************************************************/
        /* loop through every codevector of the residual vq codebook */
        /* and find the one that minimizes the energy of q[n] */
        Emin = 1e30;
        fp4 = qzsr;
        sign = 0.0F;
        jmin = 0;
        for (j = 0;  j < CBSZ;  j++)
        {
            /* Try positive sign */
            fp2 = qzir;
            E   = 0.0;
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
            E   = 0.0;
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

        /* The best codevector has been found; assign vq codebook index */
        if (sign == 1.0F)
            idx[iv++] = jmin;
        else
            idx[iv++] = jmin + CBSZ; /* MSB of index is sign bit */

        fp3 = &cb[jmin*VDIM]; /* fp3 points to start of best codevector */
        for (n = 0;  n < VDIM;  n++)
            uq[n] = sign * *fp3++;
        /********************************************************************************/


        /********************************************************************************/
        /*                    U p d a t e   F i l t e r   M e m o r y                   */
        /********************************************************************************/
        fp3 = ltsym + MAXPP1 + m;
        fp4 = ltnfm + MAXPP1 + m;
        p_ltfv = ltfv;
        p_ppv = ppv;
        p_uq = uq;
        for (n = m;  n < m + VDIM;  n++)
        {
            /* Update memory of long-term synthesis filter */
            *fp3 = *p_ppv++ + *p_uq;

            /* int16_t-term prediction */
            a0 = 0.0;
            fp1 = &buf1[n];
            for (i = LPCO;  i > 0;  i--)
                a0 -= *fp1++ * aq[i];

            /* Update memory of int16_t-term synthesis filter */
            *fp1++ = a0 + *fp3;

            /* int16_t-term pole-zero noise feedback filter */
            fpa = &buf2[n];
            fpb = &buf3[n];
            a1 = 0.0;
            for (i = NSTORDER;  i > 0;  i--)
                a1 += ((*fpa++ * fsz[i]) - (*fpb++ * fsp[i]));

            /* Update memory of pole section of noise feedback filter */
            *fpb++ = a1;

            v = s[n] - a0 - a1;

            /* Update memory of zero section of noise feedback filter */
            *fpa++ = v - *fp3++;

            /* Update memory of long-term noise feedback filter */
            *fp4++ = v - *p_ltfv++ - *p_uq++;
        }
    }

    /* Update short-term predictor and noise feedback filter memories after subframe */
    for (i = 0;  i < LPCO;  i++)
        stsym[i] = *--fp1;

    for (i = 0;  i < NSTORDER;  i++)
    {
        stnfz[i] = *--fpa;
        stnfp[i] = *--fpb;
    }

    /* update long-term predictor and noise feedback filter memories after subframe */
    fp2 = &ltnfm[FRSZ];
    fp3 = &ltsym[FRSZ];
    for (i = 0;  i < MAXPP1;  i++)
    {
        ltnfm[i] = fp2[i];
        ltsym[i] = fp3[i];
    }
}
