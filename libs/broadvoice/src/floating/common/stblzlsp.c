/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * stblzlsp.c - Find stability flag (LSPs)
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
 * $Id: stblzlsp.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
//#include "bv16cnst.h"
#include "bvcommon.h"

void stblz_lsp(Float *lsp, int order)
{

    /* This function orders the lsp to prevent      */
    /* unstable synthesis filters and imposes basic */
    /* lsp properties in order to avoid marginal    */
    /* stability of the synthesis filter.           */

    int k, i;
    Float mintmp, maxtmp, a0;


    /* order lsps as minimum stability requirement */
    do
    {
        k = 0;              /* use k as a flag for order reversal */
        for (i = 0; i < order - 1; i++)
        {
            if (lsp[i] > lsp[i+1])   /* if there is an order reversal */
            {
                a0 = lsp[i+1];
                lsp[i+1] = lsp[i];    /* swap the two LSP elements */
                lsp[i] = a0;
                k = 1;      /* set the flag for order reversal */
            }
        }
    }
    while (k > 0); /* repeat order checking if there was order reversal */


    /* impose basic lsp properties */
    maxtmp=LSPMAX-(order-1)*DLSPMIN;

    if (lsp[0] < LSPMIN)
        lsp[0] = LSPMIN;
    else if (lsp[0] > maxtmp)
        lsp[0] = maxtmp;

    for (i=0; i<order-1; i++)
    {
        /* space lsp(i+1) */

        /* calculate lower and upper bound for lsp(i+1) */
        mintmp=lsp[i]+DLSPMIN;
        maxtmp += DLSPMIN;

        /* guarantee minimum spacing to lsp(i) */
        if (lsp[i+1] < mintmp)
            lsp[i+1] = mintmp;

        /* make sure the remaining lsps fit within the remaining space */
        else if (lsp[i+1] > maxtmp)
            lsp[i+1] = maxtmp;

    }

    return;
}
