/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * allpole.c -
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
 * $Id: allpole.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bvcommon.h"

#define MAXDIM	160     /* maximum vector dimension */
#define MAXORDER LPCO     /* maximum filter order */

void apfilter(const Float *a,   /* (i) a[m+1] prediction coefficients   (m=10)  */
              int m,            /* (i) LPC order                                */
              Float *x,         /* (i) input signal                             */
              Float *y,         /* (o) output signal                            */
              int lg,           /* (i) size of filtering                        */
              Float *mem,       /* (i/o) input memory                           */
              int16_t update)   /* (i) flag for memory update                   */
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
        a0 = x[n];
        for (i = m;  i > 0;  i--)
            a0 -= *fp1++ * a[i];

        /* update the output & temporary buffer for filter memory */
        y[n] = a0;
        *fp1 = a0;
    }

    /* get the filter memory after filtering the current vector */
    if (update)
    {
        for (i = 0;  i < m;  i++)
            mem[i] = *fp1--;
    }
}
