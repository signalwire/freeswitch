/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv16lspdec.c - LSP decoding
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
 * $Id: bv16lspdec.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include "typedef.h"
#include "bv16externs.h"
#include "bvcommon.h"

void vqdec(Float *, int16_t, const Float *, int, int);

void lspdec(Float *lspq,
            int16_t *lspidx,
            Float *lsppm,
            Float *lspq_last)
{
    Float elsp[LPCO];
    Float lspe[LPCO];
    Float lspeq1[LPCO];
    Float lspeq2[LPCO];
    Float a0;
    Float *fp1;
    Float *fp2;
    const Float *fp3;
    int i;
    int k;
    int sign;
    int stbl;

    /* calculate estimated (ma-predicted) lsp vector */
    fp3 = bv16_lspp;
    fp2 = lsppm;
    for (i = 0;  i < LPCO;  i++)
    {
        a0 = 0.0F;
        for (k = 0;  k < LSPPORDER;  k++)
            a0 += *fp3++ * *fp2++;
        elsp[i] = a0;
    }

    /* perform first-stage vq codebook decode */
    vqdec(lspeq1, lspidx[0], bv16_lspecb1, LPCO, LSPECBSZ1);

    /* perform second-stage vq codebook decode */
    if (lspidx[1] >= LSPECBSZ2)
    {
        sign = -1;
        lspidx[1] = (2*LSPECBSZ2 - 1) - lspidx[1];
    }
    else
    {
        sign = 1;
    }
    vqdec(lspeq2, lspidx[1], bv16_lspecb2, LPCO, LSPECBSZ2);

    /* get overall quantizer output vector of the two-stage vq */
    for (i = 0;  i < LPCO;  i++)
        lspe[i] = lspeq1[i] + sign*lspeq2[i];

    /* calculate quantized lsp for stability check */
    for (i = 0;  i < STBLDIM;  i++)
        lspq[i] = lspe[i] + elsp[i] + bv16_lspmean[i];

    /* detect bit-errors based on ordering property of LSP */
    stbl = stblchck(lspq, STBLDIM);

    /* replace LSP if bit-errors are detected */
    if (!stbl)
    {
        for (i = 0;  i < LPCO;  i++)
        {
            lspq[i] = lspq_last[i];
            lspe[i] = lspq[i] - elsp[i] - bv16_lspmean[i];
        }
    }
    else
    {
        /* calculate remaining quantized LSP for error free case */
        for (i = STBLDIM;  i < LPCO;  i++)
            lspq[i] = lspe[i] + elsp[i] + bv16_lspmean[i];
    }

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

    /* ensure correct ordering of lsp to guarantee lpc filter stability */
    stblz_lsp(lspq, LPCO);
}


void vqdec(Float *xq,       /* VQ output vector (quantized version of input vector) */
           int16_t idx,     /* VQ codebook index for the nearest neighbor */
           const Float *cb, /* VQ codebook */
           int vdim,        /* vector dimension */
           int cbsz)        /* codebook size (number of codevectors) */
{
    int j;
    int k;

    j = idx * vdim;
    for (k = 0;  k < vdim;  k++)
        xq[k] = cb[j + k];
}


void lspplc(Float *lspq, Float *lsppm)
{
    Float elsp[LPCO];
    Float a0;
    Float *fp1;
    const Float *fp2;
    const Float *fp3;
    int i;
    int k;

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

    /* Update lsp ma predictor memory */
    i = LPCO*LSPPORDER - 1;
    fp1 = &lsppm[i];
    fp2 = &lsppm[i - 1];
    for (i = LPCO - 1; i >= 0; i--)
    {
        for (k = LSPPORDER;  k > 1;  k--)
            *fp1-- = *fp2--;
        *fp1-- = lspq[i] - bv16_lspmean[i] - elsp[i];
        fp2--;
    }
}
