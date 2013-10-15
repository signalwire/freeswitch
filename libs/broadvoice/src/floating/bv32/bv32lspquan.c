/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * lspquan.c - Quantize LSPs.
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
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include "typedef.h"
#include "bv32externs.h"
#include "bvcommon.h"

static void vqmse(Float *xq, int16_t *idx, Float *x, const Float *cb, int vdim, int cbsz);
static void vqwmse_stbl(Float *xq, int16_t *idx, Float *x, Float *w, Float *xa,
                        const Float *cb, int vdim, int cbsz);
static void vqwmse(Float *xq, int16_t *idx, Float *x, Float *w, const Float *cb, int vdim,
                   int cbsz);

void bv32_lspquan(Float *lspq,
                  int16_t *lspidx,
                  Float *lsp,
                  Float *lsppm)
{
    Float d[LPCO];
    Float w[LPCO];
    Float elsp[LPCO];
    Float lspe[LPCO];
    Float lspeq1[LPCO];
    Float lspeq2[LPCO];
    Float lspa[LPCO];
    Float a0;
    const Float *fp1;
    Float *fp2;
    Float *fp3;
    int i;
    int k;

    /* CALCULATE THE WEIGHTS FOR WEIGHTED MEAN-SQUARE ERROR DISTORTION */
    for (i = 0;  i < LPCO - 1;  i++)
        d[i] = lsp[i + 1] - lsp[i];       /* LSP difference vector */
    w[0] = 1.0F / d[0];
    for (i = 1;  i < LPCO - 1;  i++)
    {
        if (d[i] < d[i - 1])
            w[i] = 1.0F/d[i];
        else
            w[i] = 1.0F/d[i - 1];
    }
    w[LPCO - 1] = 1.0F/d[LPCO - 2];

    /* CALCULATE ESTIMATED (MA-PREDICTED) LSP VECTOR */
    fp1 = bv32_lspp;
    fp2 = lsppm;
    for (i = 0;  i < LPCO;  i++)
    {
        a0 = 0.0F;
        for (k = 0;  k < LSPPORDER;  k++)
            a0 += *fp1++ * *fp2++;
        elsp[i] = a0;
    }

    /* SUBTRACT LSP MEAN VALUE & ESTIMATED LSP TO GET PREDICTION ERROR */
    for (i = 0;  i < LPCO;  i++)
        lspe[i] = lsp[i] - bv32_lspmean[i] - elsp[i];

    /* PERFORM FIRST-STAGE VQ CODEBOOK SEARCH, MSE VQ */
    vqmse(lspeq1, &lspidx[0], lspe, bv32_lspecb1, LPCO, LSPECBSZ1);

    /* CALCULATE QUANTIZATION ERROR VECTOR OF FIRST-STAGE VQ */
    for (i = 0;  i < LPCO;  i++)
        d[i] = lspe[i] - lspeq1[i];

    /* PERFORM SECOND-STAGE VQ CODEBOOK SEARCH */
    for (i = 0;  i < SVD1;  i++)
        lspa[i] = bv32_lspmean[i] + elsp[i] + lspeq1[i];
    vqwmse_stbl(lspeq2, &lspidx[1], d, w, lspa, bv32_lspecb21, SVD1, LSPECBSZ21);
    vqwmse(&lspeq2[SVD1], &lspidx[2], &d[SVD1], &w[SVD1], bv32_lspecb22, SVD2, LSPECBSZ22);

    /* GET OVERALL QUANTIZER OUTPUT VECTOR OF THE TWO-STAGE VQ */
    for (i = 0;  i < LPCO;  i++)
        lspe[i] = lspeq1[i] + lspeq2[i];

    /* UPDATE LSP MA PREDICTOR MEMORY */
    i = LPCO * LSPPORDER - 1;
    fp3 = &lsppm[i];
    fp2 = &lsppm[i - 1];
    for (i = LPCO - 1;  i >= 0;  i--)
    {
        for (k = LSPPORDER;  k > 1;  k--)
            *fp3-- = *fp2--;
        *fp3-- = lspe[i];
        fp2--;
    }

    /* CALCULATE QUANTIZED LSP */
    for (i = 0;  i < LPCO;  i++)
        lspq[i] = lspe[i] + elsp[i] + bv32_lspmean[i];

    /* ENSURE CORRECT ORDERING & MINIMUM SPACING TO GUARANTEE STABILITY */
    stblz_lsp(lspq, LPCO);
}

/* MSE VQ */
static void vqmse(Float *xq,    /* VQ output vector (quantized version of input vector) */
                  int16_t *idx,   /* VQ codebook index for the nearest neighbor */
                  Float *x,     /* input vector */
                  const Float *cb,    /* VQ codebook */
                  int vdim,   /* vector dimension */
                  int cbsz)   /* codebook size (number of codevectors) */
{
    const Float *fp1;
    Float dmin;
    Float d;
    int j;
    int k;
    Float e;

    fp1 = cb;
    dmin = 1.0e30;
    for (j = 0;  j < cbsz;  j++)
    {
        d = 0.0F;
        for (k = 0;  k < vdim;  k++)
        {
            e = x[k] - (*fp1++);
            d += e * e;
        }
        if (d < dmin)
        {
            dmin = d;
            *idx = (int16_t) j;
        }
    }

    j = *idx * vdim;
    for (k = 0;  k < vdim;  k++)
        xq[k] = cb[j + k];
}

/* WMSE VQ with enforcement of ordering property */
static void vqwmse_stbl(Float *xq,    /* VQ output vector (quantized version of input vector) */
                        int16_t *idx,   /* VQ codebook index for the nearest neighbor */
                        Float *x,     /* input vector */
                        Float *w,     /* weights for weighted Mean-Square Error */
                        Float *xa,    /* lsp approximation */
                        const Float *cb,    /* VQ codebook */
                        int vdim,   /* vector dimension */
                        int cbsz)   /* codebook size (number of codevectors) */
{
    Float a0;
    const Float *fp1;
    const Float *fp2;
    Float dmin;
    Float d;
    Float xqc[LPCO];
    int j;
    int k;
    int stbl;

    fp1 = cb;
    dmin = 1.0e30;
    *idx = -1;
    for (j = 0;  j < cbsz;  j++)
    {
        /* Check stability */
        fp2 = fp1;
        xqc[0] = xa[0] + *fp2++;
        stbl = (xqc[0] < 0.0)  ?  0  :  1;
        for (k = 1;  k < vdim;  k++)
        {
            xqc[k]  = xa[k] + *fp2++;
            if (xqc[k] - xqc[k-1] < 0.0)
                stbl = 0;
        }

        /* Calculate distortion */
        d = 0.0F;
        for (k = 0; k < vdim; k++)
        {
            a0 = x[k] - *fp1++;
            d += w[k] * a0 * a0;
        }

        if (stbl > 0)
        {
            if (d < dmin)
            {
                dmin = d;
                *idx = (int16_t) j;
            }
        }
    }

    if (*idx == -1)
    {
        //printf("\nWARNING: Encoder-decoder synchronization lost for clean channel!!!\n");
        *idx = 1;
    }

    fp1 = cb + (*idx)*vdim;
    for (k = 0;  k < vdim;  k++)
        xq[k] = *fp1++;
}

/* MSE VQ */
static void vqwmse(Float *xq,      /* VQ output vector (quantized version of input vector) */
                   int16_t *idx,   /* VQ codebook index for the nearest neighbor */
                   Float *x,       /* input vector */
                   Float *w,       /* weights for weighted Mean-Square Error */
                   const Float *cb,    /* VQ codebook */
                   int vdim,       /* vector dimension */
                   int cbsz)       /* codebook size (number of codevectors) */
{
    Float a0;
    const Float *fp1;
    Float dmin;
    Float d;
    int j;
    int k;

    fp1 = cb;
    dmin = 1.0e30;
    for (j = 0;  j < cbsz;  j++)
    {
        d = 0.0F;
        for (k = 0;  k < vdim;  k++)
        {
            a0 = x[k] - *fp1++;
            d += w[k]*a0*a0;
        }
        if (d < dmin)
        {
            dmin = d;
            *idx = (int16_t) j;
        }
    }

    j = *idx * vdim;
    for (k = 0;  k < vdim;  k++)
        xq[k] = cb[j + k];
}
