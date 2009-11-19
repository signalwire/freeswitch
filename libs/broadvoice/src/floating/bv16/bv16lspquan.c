/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv16lspquan.c - LSP quantization based on inter-frame moving-average
 *                 prediction and two-stage VQ.
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
 * $Id: bv16lspquan.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include "typedef.h"
#include "bv16externs.h"
#include "bvcommon.h"

void vqmse(
    Float *xq,
    int16_t *idx,
    Float *x,
    const Float *cb,
    int vdim,
    int cbsz);

void svqwmse(
    Float *xq,
    int16_t *idx,
    Float *x,
    Float *xa,
    Float *w,
    const Float *cb,
    int vdim,
    int cbsz);

void lspquan(
    Float   *lspq,
    int16_t   *lspidx,
    Float   *lsp,
    Float   *lsppm
)
{
    Float d[LPCO];
    Float w[LPCO];
    Float elsp[LPCO];
    Float lspe[LPCO];
    Float lspa[LPCO];
    Float lspeq1[LPCO];
    Float lspeq2[LPCO];
    Float a0;
    Float *fp1;
    const Float *fp2;
    const Float *fp3;
    int i;
    int k;

    /* calculate the weights for weighted mean-square error distortion */
    for (i = 0;  i < LPCO - 1;  i++)
        d[i] = lsp[i + 1] - lsp[i];       /* LSP difference vector */
    w[0] = 1.0F/d[0];
    for (i = 1;  i < LPCO - 1;  i++)
    {
        if (d[i] < d[i-1])
            w[i] = 1.0F/d[i];
        else
            w[i] = 1.0F/d[i - 1];
    }
    w[LPCO - 1] = 1.0F/d[LPCO - 2];

    /* Calculate estimated (ma-predicted) lsp vector */
    fp3 = bv16_lspp;
    fp2 = lsppm;
    for (i = 0;  i < LPCO;  i++)
    {
        a0 = 0.0F;
        for (k = 0;  k < LSPPORDER;  k++)
            a0 += *fp3++ * *fp2++;
        elsp[i] = a0;
    }

    /* Subtract lsp mean value & estimated lsp to get prediction error */
    for (i = 0;  i < LPCO;  i++)
        lspe[i] = lsp[i] - bv16_lspmean[i] - elsp[i];

    /* Perform first-stage mse vq codebook search */
    vqmse(lspeq1, &lspidx[0], lspe, bv16_lspecb1, LPCO, LSPECBSZ1);

    /* Calculate quantization error vector of first-stage vq */
    for (i = 0;  i < LPCO;  i++)
        d[i] = lspe[i] - lspeq1[i];

    /* Perform second-stage vq codebook search, signed codebook with wmse */
    for (i = 0;  i < LPCO;  i++)
        lspa[i] = bv16_lspmean[i] + elsp[i] + lspeq1[i];
    svqwmse(lspeq2, &lspidx[1], d, lspa, w, bv16_lspecb2, LPCO, LSPECBSZ2);

    /* Get overall quantizer output vector of the two-stage vq */
    for (i = 0;  i < LPCO;  i++)
        lspe[i] = lspeq1[i] + lspeq2[i];

    /* update lsp ma predictor memory */
    i = LPCO * LSPPORDER - 1;
    fp1 = &lsppm[i];
    fp2 = &lsppm[i - 1];
    for (i = LPCO - 1;  i >= 0;  i--)
    {
        for (k = LSPPORDER;  k > 1;  k--)
            *fp1-- = *fp2--;
        *fp1-- = lspe[i];
        fp2--;
    }

    /* calculate quantized lsp */
    for (i = 0;  i < LPCO;  i++)
        lspq[i] = lspa[i] + lspeq2[i];

    /* ensure correct ordering of lsp to guarantee lpc filter stability */
    stblz_lsp(lspq, LPCO);
}

void vqmse(Float *xq,    /* VQ output vector (quantized version of input vector) */
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
            d += e*e;
        }
        if (d < dmin)
        {
            dmin = d;
            *idx = j;
        }
    }

    j = *idx * vdim;
    for (k = 0;  k < vdim;  k++)
        xq[k] = cb[j + k];
}

/* Signed WMSE VQ */
void svqwmse(
    Float   *xq,    /* VQ output vector (quantized version of input vector) */
    int16_t   *idx,   /* VQ codebook index for the nearest neighbor */
    Float   *x,     /* input vector */
    Float   *xa,    /* approximation prior to current stage */
    Float   *w,     /* weights for weighted Mean-Square Error */
    const Float   *cb,    /* VQ codebook */
    int     vdim,   /* vector dimension */
    int     cbsz    /* codebook size (number of codevectors) */
)
{
    const Float *fp1;
    const Float *fp2;
    Float dmin;
    Float d;
    Float xqc[STBLDIM];
    int j, k, stbl, sign=1;
    Float e;

    fp1  = cb;
    dmin = 1e30;
    *idx = -1;

    for (j = 0;  j < cbsz;  j++)
    {
        /* Try negative sign */
        d = 0.0;
        fp2 = fp1;

        for (k = 0;  k < vdim;  k++)
        {
            e = x[k] + *fp1++;
            d += w[k]*e*e;
        }

        /* check candidate - negative sign */
        if (d < dmin)
        {
            for (k = 0;  k < STBLDIM;  k++)
                xqc[k]  = xa[k] - *fp2++;
            /* check stability - negative sign */
            stbl = stblchck(xqc, STBLDIM);
            if (stbl > 0)
            {
                dmin = d;
                *idx = j;
                sign = -1;
            }
        }

        /* Try positive sign */
        fp1 -= vdim;
        d = 0.0;
        fp2 = fp1;

        for (k = 0;  k < vdim;  k++)
        {
            e = x[k] - *fp1++;
            d += w[k]*e*e;
        }

        /* check candidate - positive sign */
        if (d < dmin)
        {
            for (k = 0;  k < STBLDIM;  k++)
                xqc[k]  = xa[k] + *fp2++;

            /* check stability - positive sign */
            stbl = stblchck(xqc, STBLDIM);
            if (stbl > 0)
            {
                dmin = d;
                *idx = j;
                sign = +1;
            }
        }
    }

    if (*idx == -1)
    {
        printf("\nWARNING: Encoder-decoder synchronization lost for clean channel!!!\n");
        *idx = 0;
        sign = 1;
    }

    fp1 = cb + (*idx)*vdim;
    for (k = 0;  k < vdim;  k++)
        xq[k] = (double) sign*(*fp1++);
    if (sign < 0)
        *idx = (2*cbsz - 1) - (*idx);
}
