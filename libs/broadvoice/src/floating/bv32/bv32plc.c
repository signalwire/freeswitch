/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * plc.c - Packet loss concealment.
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
 * $Id: bv32plc.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <math.h>

#include "typedef.h"
#include "bv32cnst.h"
#include "utility.h"
#include "bvcommon.h"
#include "bv32externs.h"
#include "bv32strct.h"
#include "broadvoice/broadvoice.h"

BV_DECLARE(int) bv32_fillin(bv32_decode_state_t *ds, int16_t amp[], int len)
{
    int n;
    int i_sf;
    Float r[SFRSZ];        /* random excitation */
    Float E;
    Float gain;
    Float tmp;
    Float xq[SFRSZ];
    Float d[LTMOFF + FRSZ];	/* long-term synthesis filter memory */

    /************************************************************/
    /*                 Copy decoder state memory                */
    /************************************************************/
    Fcopy(d, ds->ltsym, LTMOFF); /* excitation */

    /************************************************************/
    /*        Update counter of consecutive list frames         */
    /************************************************************/
    if (ds->cfecount < HoldPLCG + AttnPLCG - 1)
        ds->cfecount++;

    /* loop over subframes */
    for (i_sf = 0;  i_sf < FECNSF;  i_sf++)
    {
        /* Generate Unscaled Excitation */
        E = 0.0;
        for (n = 0;  n < SFRSZ;  n++)
        {
            ds->idum = 1664525L*ds->idum + 1013904223L;
            r[n] = (Float)(ds->idum >> 16) - 32767.0;
            E += r[n] * r[n];
        }

        /* Calculate Scaling */
        ds->scplcg = ScPLCG_a + ScPLCG_b * ds->per;
        if (ds->scplcg > ScPLCGmax)
            ds->scplcg = ScPLCGmax;
        else if (ds->scplcg < ScPLCGmin)
            ds->scplcg = ScPLCGmin;
        gain = ds->scplcg * sqrt(ds->E/E);

        /* Long-term synthesis filter */
        for (n = 0;  n < SFRSZ;  n++)
        {
            d[LTMOFF+i_sf*SFRSZ+n] = gain * r[n];
            d[LTMOFF+i_sf*SFRSZ+n] += ds->bq_last[0] * d[LTMOFF+i_sf*SFRSZ+n-ds->pp_last+1];
            d[LTMOFF+i_sf*SFRSZ+n] += ds->bq_last[1] * d[LTMOFF+i_sf*SFRSZ+n-ds->pp_last];
            d[LTMOFF+i_sf*SFRSZ+n] += ds->bq_last[2] * d[LTMOFF+i_sf*SFRSZ+n-ds->pp_last-1];
        }

        /************************************************************/
        /*                Short-term synthesis filter               */
        /************************************************************/
        apfilter(ds->atplc, LPCO, d+i_sf*SFRSZ+LTMOFF, xq, SFRSZ, ds->stsym, 1);

        /**********************************************************/
        /*                    De-emphasis filter                  */
        /**********************************************************/
        for (n = 0;  n < SFRSZ;  n++)
        {
            tmp = xq[n] + PEAPFC*ds->dezfm[0] - PEAZFC*ds->depfm[0];
            ds->dezfm[0] = xq[n];
            ds->depfm[0] = tmp;
            if (tmp >= 0)
                tmp += 0.5;
            else tmp -= 0.5;

            if (tmp > 32767.0)
                tmp = 32767.0;
            else if (tmp < -32768.0)
                tmp = -32768.0;
            amp[i_sf*SFRSZ + n] = (int16_t) tmp;
        }

        /************************************************************/
        /*        Update memory of predictive gain quantizer        */
        /************************************************************/
        bv32_gainplc(ds->E, ds->lgpm, ds->prevlg);

        /* Estimate the signal level */
        bv32_estlevel(ds->prevlg[0], &ds->level, &ds->lmax, &ds->lmin, &ds->lmean, &ds->x1);
    }

    /************************************************************/
    /*                 Save decoder state memory                */
    /************************************************************/
    Fcopy(ds->ltsym, d + FRSZ, LTMOFF);

    /************************************************************/
    /*        Update memory of predictive LSP quantizer         */
    /************************************************************/
    bv32_lspplc(ds->lsplast,ds->lsppm);

    /************************************************************/
    /*          Attenuation during long packet losses           */
    /************************************************************/
    if (ds->cfecount >= HoldPLCG)
    {
        gain = 1.0 - AttnFacPLCG*(Float) (ds->cfecount - (HoldPLCG - 1));
        ds->bq_last[0] = gain*ds->bq_last[0];
        ds->bq_last[1] = gain*ds->bq_last[1];
        ds->bq_last[2] = gain*ds->bq_last[2];
        ds->E = gain*gain*ds->E;
    }
    return FRSZ;
}
