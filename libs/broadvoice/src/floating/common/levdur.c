/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * levdur.c - Levinson Durbin
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
 * $Id: levdur.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bvcommon.h"

/*  Levinson-Durbin recursion */
void Levinson(Float *r,         /* (i): autocorrelation coefficients */
              Float *a,	        /* (o): LPC coefficients */
              Float *old_a,	    /* (i/o): LPC coefficients of previous frame */
              int m)	        /* (i): LPC order */
{
    Float alpha;
    Float a0, a1;
    Float rc, *aip, *aib, *alp;
    int mh, minc, ip;

    *a = 1.;
    if (*r <= 0.0)
        goto illcond;

    /* start durbin's recursion */
    rc = - *(r + 1) / *r;
    *(a + 1) = rc;
    alpha = *r + *(r+1) * rc;
    if (alpha <= 0.0)
        goto illcond;
    for (minc = 2;  minc <= m;  minc++)
    {
        a0 = 0.0;
        aip = a;
        aib = r + minc;
        for (ip = 0;  ip <= minc - 1;  ip++)
            a0 = a0 + *aib-- * *aip++;
        rc = -a0 / alpha;
        mh = minc / 2;
        aip = a + 1;
        aib = a + minc - 1;
        for (ip = 1;  ip <= mh;  ip++)
        {
            a1 = *aip + rc * *aib;
            *aib = *aib + rc * *aip;
            aib--;
            *aip++ = a1;
        }
        *(a+minc) = rc;
        alpha = alpha + rc * a0;
        if (alpha <= 0.0)
            goto illcond;
    }

    aip = a;
    alp = old_a;
    for (ip = 0;  ip <= m;  ip++)
        *alp++ = *aip++;

    return;
illcond:
    aip = a;
    alp = old_a;
    for (ip = 0;  ip <= m;  ip++)
        *aip++ = *alp++;
}
