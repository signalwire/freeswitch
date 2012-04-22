/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * lsp2a.c -
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
 * $Id: lsp2a.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include "typedef.h"
#include "bvcommon.h"

#define OR (LPCO+1)	/* Maximum LPC order */
#define PI  3.14159265358979

void lsp2a(
    Float *lsp,	/* (i) LSP vector       */
    Float *a) 	/* (o) LPC coefficients */
{
    Float c1, c2, p[OR], q[OR];
    int orderd2, n, i, nor;

    orderd2=LPCO/2;
    for (i = 1; i <= LPCO ; i++)
        p[i] = q[i]= 0.;
    /* Get Q & P polyn. less the (1 +- z-1) ( or (1 +- z-2) ) factor */
    p[0] = q[0] = 1.;
    for (n = 1; n <= orderd2; n++)
    {
        nor= 2 * n;
        c1 = 2. * cos((double)PI*lsp[nor-1]);
        c2 = 2. * cos((double)PI*lsp[nor-2]);
        for (i = nor; i >= 2; i--)
        {
            q[i] += q[i-2] - c1*q[i-1];
            p[i] += p[i-2] - c2*p[i-1];
        }
        q[1] -= c1;
        p[1] -= c2;
    }
    /* Get the the predictor coeff. */
    a[0] = 1.;
    a[1] = 0.5 * (p[1] + q[1]);
    for (i=1, n=2; i < LPCO ; i++, n++)
        a[n] = 0.5 * (p[i] + p[n] + q[n] - q[i]);
}
