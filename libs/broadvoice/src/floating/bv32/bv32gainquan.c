/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * gainquan.c - Quantize gains.
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
 * $Id: bv32gainquan.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include "typedef.h"
#include "bv32externs.h"

int bv32_gainquan(Float *gainq,
                  Float lg,
                  Float *lgpm,
                  Float *prevlg,        /* previous log gains (last two frames) */
                  Float level)          /* input level estimate */
{
    Float elg;
    Float lgpe;
    Float limit;
    Float dmin;
    Float d;
    int i;
    int n;
    int gidx = 0;
    const int *p_gidx;

    /* CALCULATE ESTIMATED LOG-GAIN */
    elg = bv32_lgmean;
    for (i = 0;  i < LGPORDER;  i++)
        elg += bv32_lgp[i]*lgpm[i];

    /* SUBTRACT LOG-GAIN MEAN & ESTIMATED LOG-GAIN TO GET PREDICTION ERROR */
    lgpe = lg - elg;

    /* SCALAR QUANTIZATION OF LOG-GAIN PREDICTION ERROR */
    dmin = 1e30;
    p_gidx = bv32_idxord;
    for (i = 0;  i < LGPECBSZ;  i++)
    {
        d = lgpe - bv32_lgpecb[*p_gidx++];
        if (d < 0.0F)
        {
            d = -d;
        }
        if (d < dmin)
        {
            dmin = d;
            /* index into ordered codebook */
            gidx=i;
        }
    }

    /* CALCULATE QUANTIZED LOG-GAIN */
    *gainq = bv32_lgpecb[bv32_idxord[gidx]] + elg;

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
    i = i*NGCB + n;

    /* CHECK WHETHER QUANTIZED LOG-GAIN CAUSE A GAIN CHANGE > LGCLIMIT */
    limit = prevlg[0] + bv32_lgclimit[i];/* limit that log-gain shouldn't exceed */
    while (*gainq > limit && gidx > 0)   /* if quantized gain exceeds limit */
    {
        gidx -= 1;     /* decrement gain quantizer index by 1 */
        *gainq = bv32_lgpecb[bv32_idxord[gidx]] + elg; /* use next quantizer output*/
    }
    /* get true codebook index */
    gidx = bv32_idxord[gidx];

    /* UPDATE LOG-GAIN PREDICTOR MEMORY */
    prevlg[1] = prevlg[0];
    prevlg[0] = *gainq;
    for (i = LGPORDER - 1; i > 0; i--)
        lgpm[i] = lgpm[i - 1];
    lgpm[0] = bv32_lgpecb[gidx];

    /* CONVERT QUANTIZED LOG-GAIN TO LINEAR DOMAIN */
    *gainq = pow(2.0F, 0.5F * *gainq);

    return gidx;
}
