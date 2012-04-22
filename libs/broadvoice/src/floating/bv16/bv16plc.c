/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv16plc.c - Packet loss concealment
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
 * $Id: bv16plc.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include "typedef.h"
#include "bv16cnst.h"
#include "bv16strct.h"
#include "bv16externs.h"
#include "bvcommon.h"

#include "utility.h"
#include "bv16postfilter.h"
#include "broadvoice/broadvoice.h"

BV_DECLARE(int) bv16_fillin(bv16_decode_state_t *ds, int16_t amp[], int len)
{
    int n;
    Float r[FRSZ];        /* random excitation */
    Float E;
    Float gain;
    Float scplcg;
    Float xq[LXQ];
    Float s[FRSZ];        /* enhanced short-term excitation */
    Float d[LTMOFF + FRSZ];	/* long-term synthesis filter memory */
    Float *sq;

    /************************************************************/
    /*                 Copy decoder state memory                */
    /************************************************************/
    Fcopy(d, ds->ltsym, LTMOFF); /* excitation */
    Fcopy(xq, ds->xq, XQOFF);

    sq = xq + XQOFF;
    /************************************************************/
    /*        Update counter of consecutive list frames         */
    /************************************************************/
    if (ds->cfecount < HoldPLCG + AttnPLCG - 1)
        ds->cfecount++;
    ds->ngfae = 0;

    /************************************************************/
    /*                Generate Unscaled Excitation              */
    /************************************************************/
    E = 0.0;
    for (n = 0;  n < FRSZ;  n++)
    {
        ds->idum = 1664525L*ds->idum + 1013904223L;
        r[n] = (Float)(ds->idum >> 16) - 32767.0;
        E += r[n]*r[n];
    }

    /************************************************************/
    /*                      Calculate Scaling                   */
    /************************************************************/
    scplcg = ScPLCG_a + ScPLCG_b*ds->per;
    if (scplcg > ScPLCGmax)
        scplcg = ScPLCGmax;
    else if (scplcg < ScPLCGmin)
        scplcg = ScPLCGmin;
    gain = scplcg * sqrt(ds->E/E);

    /************************************************************/
    /*                  Long-term synthesis filter              */
    /************************************************************/
    for (n = 0;  n < FRSZ;  n++)
    {
        d[LTMOFF+n] = gain*r[n];
        d[LTMOFF+n] += ds->bq_last[0]*d[LTMOFF + n-ds->pp_last + 1];
        d[LTMOFF+n] += ds->bq_last[1]*d[LTMOFF + n-ds->pp_last];
        d[LTMOFF+n] += ds->bq_last[2]*d[LTMOFF + n-ds->pp_last - 1];
    }

    /************************************************************/
    /*                Short-term synthesis filter               */
    /************************************************************/
    apfilter(ds->atplc, LPCO, d+LTMOFF, sq, FRSZ, ds->stsym, 1);

    /************************************************************/
    /*                 Save decoder state memory                */
    /************************************************************/
    Fcopy(ds->ltsym, d+FRSZ, LTMOFF); /* excitation */

    /************************************************************/
    /*        Update memory of predictive LSP quantizer         */
    /************************************************************/
    lspplc(ds->lsplast, ds->lsppm);

    /************************************************************/
    /*        Update memory of predictive gain quantizer        */
    /************************************************************/
    gainplc(ds->E, ds->lgpm, ds->prevlg);

    /************************************************************/
    /*                  Signal level estimation                 */
    /************************************************************/
    estlevel(ds->prevlg[0], &ds->level, &ds->lmax, &ds->lmin, &ds->lmean,
             &ds->x1, ds->ngfae, ds->nggalgc, &ds->estl_alpha_min);

    /************************************************************/
    /*          Attenuation during long packet losses           */
    /************************************************************/
    if (ds->cfecount >= HoldPLCG)
    {
        gain = 1.0 - AttnFacPLCG*(Float)(ds->cfecount - (HoldPLCG - 1));
        ds->bq_last[0] = gain*ds->bq_last[0];
        ds->bq_last[1] = gain*ds->bq_last[1];
        ds->bq_last[2] = gain*ds->bq_last[2];
        ds->E = (gain*gain)*ds->E;
    }

    /************************************************************/
    /*                   Adaptive Postfiltering                 */
    /************************************************************/
    postfilter(xq, ds->pp_last, &(ds->ma_a), ds->b_prv, &(ds->pp_prv), s);
    F2s(amp, s, FRSZ);
    Fcopy(ds->xq, xq + FRSZ, XQOFF);

    return FRSZ;
}
