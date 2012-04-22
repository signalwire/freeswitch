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
 *
 * $Id: bv32gaindec.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>

#include "typedef.h"
#include "bv32externs.h"

Float bv32_gaindec(Float *lgq,
                   int16_t gidx,
                   Float *lgpm,
                   Float *prevlg,        /* previous log gains (last two frames) */
                   Float level,           /* input level estimate */
                   int16_t *nclglim,
                   int16_t lctimer)
{
    Float gainq;
    Float elg;
    Float lgc;
    Float lgq_nh;
    int i;
    int n;
    int k;

    /* CALCULATE ESTIMATED LOG-GAIN (WITH MEAN VALUE OF LOG GAIN RESTORED) */
    elg = bv32_lgmean;
    for (i = 0;  i < LGPORDER;  i++)
        elg += bv32_lgp[i]*lgpm[i];

    /* CALCULATE DECODED LOG-GAIN */
    *lgq = bv32_lgpecb[gidx] + elg;

    /* next higher gain */
    if (gidx < LGPECBSZ - 1)
    {
        lgq_nh = bv32_lgpecb_nh[gidx] + elg;
        if (*lgq < MinE  &&  fabs(lgq_nh-MinE) < fabs(*lgq-MinE))
        {
            /* To avoid thresholding when the enc Q makes it below the threshold */
            *lgq = MinE;
        }
    }

    /* LOOK UP FROM lgclimit() TABLE THE MAXIMUM LOG GAIN CHANGE ALLOWED */
    i = (int) ((prevlg[0] - level - LGLB) * 0.5F); /* get column index */
    if (i >= NGB)
        i = NGB - 1;
    else if (i < 0)
        i = 0;
    n = (int) ((prevlg[0] - prevlg[1] - GCLB) * 0.5F);  /* get row index */
    if (n >= NGCB)
        n = NGCB - 1;
    else if (n < 0)
        n = 0;
    i = i * NGCB + n;

    /* UPDATE LOG-GAIN PREDICTOR MEMORY, CHECK WHETHER DECODED LOG-GAIN EXCEEDS LGCLIMIT */
    for (k = LGPORDER - 1;  k > 0;  k--)
        lgpm[k] = lgpm[k-1];
    lgc = *lgq - prevlg[0];
    if (lgc > bv32_lgclimit[i]  &&  gidx > 0  &&  lctimer == 0)   /* if decoded log-gain exceeds limit */
    {
        *lgq = prevlg[0];   /* use the log-gain of previous frame */
        lgpm[0] = *lgq - elg;
        *nclglim = *nclglim + 1;
        if (*nclglim > NCLGLIM_TRAPPED)
            *nclglim = NCLGLIM_TRAPPED;
    }
    else
    {
        lgpm[0] = bv32_lgpecb[gidx];
        *nclglim = 0;
    }

    /* UPDATE PREVIOUS LOG-GAINS */
    prevlg[1] = prevlg[0];
    prevlg[0] = *lgq;

    /* CONVERT QUANTIZED LOG-GAIN TO LINEAR DOMAIN */
    gainq = pow(2.0F, 0.5F * *lgq);

    return gainq;
}

void bv32_gainplc(Float E,
                  Float *lgeqm,
                  Float *lgqm)
{
    int k;
    Float pe, lg, mrlg, elg, lge;

    pe = INVSFRSZ * E;

    if (pe - TMinlg > 0.0)
        lg = log(pe)/log(2.0);
    else
        lg = Minlg;

    mrlg = lg - bv32_lgmean;

    elg = 0.0;
    for (k = 0;  k < GPO;  k++)
        elg += bv32_lgp[k]*lgeqm[k];

    /* predicted log-gain error */
    lge = mrlg - elg;

    /* update quantizer memory */
    for (k = GPO - 1;  k > 0;  k--)
        lgeqm[k] = lgeqm[k-1];
    lgeqm[0] = lge;

    /* update quantized log-gain memory */
    lgqm[1] = lgqm[0];
    lgqm[0] = lg;
}
