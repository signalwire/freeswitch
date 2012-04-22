/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * excdec.c - Excitation signal decoding including long-term synthesis.
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
 * $Id: bv32excdec.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bv32externs.h"

void bv32_excdec_w_LT_synth(Float *ltsymd,   /* long-term synthesis filter memory at decoder*/
                            int16_t *idx,    /* excitation codebook index array for current subframe */
                            Float *gainq,    /* quantized linear gains for sub-subframes */
                            Float *b,        /* coefficient of 3-tap pitch predictor */
                            int16_t pp,      /* pitch period (# of 8 kHz samples) */
                            Float *EE)
{
    Float a0;
    Float *fp1;
    Float *fp2;
    const Float *fp3;
    Float gain;
    int i;
    int m;
    int n;
    int id;
    int16_t	*ip;
    Float E;
    Float t;

    ip = idx;
    fp1 = &ltsymd[LTMOFF]; /* fp1 points to 1st sample of current subframe */
    fp2 = &ltsymd[LTMOFF - pp + 1];
    for (i = 0;  i < NSF;  i++)   /* loop through sub-subframes */
    {
        E = 0.0;
        for (m = 0;  m < SFRSZ;  m += VDIM)   /* loop thru vectors in sub-subframe */
        {
            id = *ip++;   /* get codebook index of current vector */
            if (id < CBSZ)
            {
                gain = gainq[i];
            }
            else
            {
                gain = -gainq[i];
                id -= CBSZ;
            }
            fp3 = &bv32_cccb[id*VDIM];
            for (n = 0;  n < VDIM;  n++)
            {
                a0  = b[0] * *fp2--;
                a0 += b[1] * *fp2--;
                a0 += b[2] * *fp2;/* a0=pitch predicted value of LT syn filt */
                t = *fp3++ * gain;
                E += t*t;
                *fp1++ = a0 + t; /* add scale codevector to a0 */
                fp2 = &fp2[3];    /* prepare fp2 for filtering next sample */
            }
        }
    }
    *EE = E;
}
