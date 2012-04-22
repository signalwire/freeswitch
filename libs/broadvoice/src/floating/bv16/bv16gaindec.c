/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv16gaindec.c - Gain decoding
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
 * $Id: bv16gaindec.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include "typedef.h"
#include "bv16strct.h"
#include "bv16externs.h"

Float gaindec(Float *lgq,
              int16_t gidx,
              Float *lgpm,
              Float *prevlg,		/* previous log gains (last two frames) */
              Float level,
              int16_t *nggalgc,
              Float *lg_el)
{
    Float gainq;
    Float elg;
    Float lgc;
    Float lgq_nh;
    int i;
    int n;
    int k;

    /* calculate estimated log-gain */
    elg = 0;
    for (i = 0;  i < LGPORDER;  i++)
        elg += bv16_lgp[i]*lgpm[i];

    elg += bv16_lgmean;

    /* Calculate decoded log-gain */
    *lgq = bv16_lgpecb[gidx] + elg;

    /* next higher gain */
    if (gidx < LGPECBSZ - 1)
    {
        lgq_nh = bv16_lgpecb_nh[gidx] + elg;
        if (*lgq < 0.0  &&  fabs(lgq_nh) < fabs(*lgq))
        {
            /* To avoid thresholding when the enc Q makes it below the threshold */
            *lgq = 0.0;
        }
    }

    /* look up from lgclimit() table the maximum log gain change allowed */
    i = (int) ((prevlg[0] - level - LGLB) * 0.5F); /* get column index */
    if (i >= NGB)
        i = NGB - 1;
    else if (i < 0)
        i = 0;
    n = (int) ((prevlg[0] - prevlg[1] - LGCLB) * 0.5F);  /* get row index */
    if (n >= NGCB)
        n = NGCB - 1;
    else if (n < 0)
        n = 0;

    i = i*NGCB + n;

    /* update log-gain predictor memory,
    check whether decoded log-gain exceeds lgclimit */
    for (k = LGPORDER - 1;  k > 0;  k--)
        lgpm[k] = lgpm[k - 1];

    lgc = *lgq - prevlg[0];
    if ((lgc > bv16_lgclimit[i])  &&  (gidx > 0))   /* if decoded log-gain exceeds limit */
    {
        *lgq = prevlg[0];	 /* use the log-gain of previous frame */
        lgpm[0] = *lgq - elg;
        *nggalgc = 0;
        *lg_el = bv16_lgclimit[i] + prevlg[0];
    }
    else
    {
        lgpm[0] = bv16_lgpecb[gidx];
        *nggalgc = *nggalgc + 1;
        if (*nggalgc > Nfdm)
            *nggalgc = Nfdm + 1;
        *lg_el = *lgq;
    }

    /* update log-gain predictor memory */
    prevlg[1] = prevlg[0];
    prevlg[0] = *lgq;

    /* convert quantized log-gain to linear domain */
    gainq = pow(2.0F, 0.5F * *lgq);

    return gainq;
}

Float gaindec_fe(Float lgq_last,
                 Float *lgpm)
{
    Float elg;
    int i;

    /* calculate estimated log-gain */
    elg = 0.0F;
    for (i = 0;  i < LGPORDER;  i++)
        elg += bv16_lgp[i]*lgpm[i];

    /* update log-gain predictor memory */
    for (i = LGPORDER - 1;  i > 0;  i--)
        lgpm[i] = lgpm[ i- 1];
    lgpm[0] = lgq_last - bv16_lgmean - elg;

    return lgq_last;
}

void gainplc(Float E,
             Float *lgeqm,
             Float *lgqm)
{
    int k;
    Float pe;
    Float lg;
    Float mrlg;
    Float elg;
    Float lge;

    pe = INVFRSZ*E;

    if (pe - TMinlg > 0.0)
        lg = log(pe)/log(2.0);
    else
        lg = Minlg;

    mrlg = lg - bv16_lgmean;

    elg = 0.0;
    for (k = 0;  k < GPO;  k++)
        elg += bv16_lgp[k]*lgeqm[k];

    /* Predicted log-gain error */
    lge = mrlg - elg;

    /* Update quantizer memory */
    for (k = GPO - 1;  k > 0;  k--)
        lgeqm[k] = lgeqm[k - 1];
    lgeqm[0] = lge;

    /* Update quantized log-gain memory */
    lgqm[1] = lgqm[0];
    lgqm[0] = lg;
}
