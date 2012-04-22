/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv32lspdec.c - Decode LSPs.
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
 * $Id: bv32lspdec.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include "typedef.h"
#include "bv32externs.h"
#include "bvcommon.h"

static void vqdec(Float *, int16_t, const Float *, int);

void bv32_lspdec(Float *lspq,
                 int16_t *lspidx,
                 Float *lsppm,
                 Float *lspq_last)
{
    Float elsp[LPCO];
    Float lspe[LPCO];
    Float lspeq1[LPCO];
    Float lspeq2[LPCO];
    Float a0;
    const Float *fp1;
    Float *fp2;
    Float *fp3;
    int i;
    int k;
    int lsfdordr;

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

    /* PERFORM FIRST-STAGE VQ CODEBOOK DECODE */
    vqdec(lspeq1, lspidx[0], bv32_lspecb1, LPCO);

    /* PERFORM SECOND-STAGE VQ CODEBOOK DECODE */
    vqdec(lspeq2, lspidx[1], bv32_lspecb21, SVD1);
    vqdec(lspeq2 + SVD1, lspidx[2], bv32_lspecb22, SVD2);

    /* GET OVERALL QUANTIZER OUTPUT VECTOR OF THE TWO-STAGE VQ */
    /* AND CALCULATE QUANTIZED LSP */
    for (i = 0;  i < LPCO;  i++)
    {
        lspe[i] = lspeq1[i] + lspeq2[i];
        lspq[i] = lspe[i] + elsp[i] + bv32_lspmean[i];
    }

    /* detect bit-errors based on ordering property */
    if (lspq[0] < 0.0)
        lsfdordr = 1;
    else
        lsfdordr = 0;
    for (i = 1;  i < SVD1;  i++)
    {
        if (lspq[i] - lspq[i-1] < 0.0)
            lsfdordr = 1;
    }

    /* substitute LSP and MA predictor update if bit-error detected */
    if (lsfdordr)
    {
        for (i = 0;  i < LPCO;  i++)
        {
            lspq[i] = lspq_last[i];
            lspe[i] = lspq[i] - elsp[i] - bv32_lspmean[i];
        }
    }

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

    /* ENSURE CORRECT ORDERING & MINIMUM SPACING TO GUARANTEE STABILITY */
    stblz_lsp(lspq, LPCO);
}

static void vqdec(Float *xq,       /* VQ output vector (quantized version of input vector) */
                  int16_t idx,     /* VQ codebook index for the nearest neighbor */
                  const Float *cb, /* VQ codebook */
                  int vdim)        /* vector dimension */
{
    int j;
    int k;

    j = idx * vdim;
    for (k = 0;  k < vdim;  k++)
        xq[k] = cb[j + k];
}


void bv32_lspplc(Float *lspq,
                 Float *lsppm)
{
    Float elsp[LPCO];
    Float a0;
    const Float *fp1;
    Float *fp2;
    Float *fp3;
    int i, k;

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

    /* UPDATE LSP MA PREDICTOR MEMORY */
    i = LPCO * LSPPORDER - 1;
    fp3 = &lsppm[i];
    fp2 = &lsppm[i - 1];
    for (i = LPCO - 1;  i >= 0;  i--)
    {
        for (k = LSPPORDER;  k > 1;  k--)
            *fp3-- = *fp2--;
        *fp3-- = lspq[i] - bv32_lspmean[i] - elsp[i];
        fp2--;
    }
}
