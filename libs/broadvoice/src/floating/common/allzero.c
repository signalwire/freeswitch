/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * allzero.c -
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
 * $Id: allzero.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bvcommon.h"

#define MAXDIM	160     /* maximum vector dimension */
#define MAXORDER LPCO     /* maximum filter order */

void azfilter(
    const Float *a, /* (i) prediction coefficients                  */
    int     m,      /* (i) LPC order                                */
    Float   *x,     /* (i) input signal vector                      */
    Float   *y,     /* (o) output signal vector                     */
    int     lg,     /* (i) size of filtering                        */
    Float   *mem,   /* (i/o) filter memory before filtering         */
    int16_t update) /* (i) flag for memory update                   */
{
    Float buf[MAXORDER + MAXDIM]; /* buffer for filter memory & signal */
    Float a0;
    Float *fp1;
    int i;
    int n;

    /* copy filter memory to beginning part of temporary buffer */
    fp1 = &mem[m - 1];
    for (i = 0;  i < m;  i++)
        buf[i] = *fp1--;    /* this buffer is used to avoid memory shifts */

    /* loop through every element of the current vector */
    for (n = 0;  n < lg;  n++)
    {
        /* perform multiply-adds along the delay line of filter */
        fp1 = &buf[n];
        a0 = 0.0F;
        for (i = m;  i > 0;  i--)
            a0 += *fp1++ * a[i];

        /* update the temporary buffer for filter memory */
        *fp1 = x[n];

        /* do the last multiply-add separately and get the output */
        y[n] = a0 + x[n] * a[0];
    }

    /* get the filter memory after filtering the current vector */
    if (update)
    {
        for (i = 0;  i < m;  i++)
            mem[i] = *fp1--;
    }
}
