/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * decoder.c -
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
 * $Id: bv32decoder.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "typedef.h"
#include "bv32cnst.h"
#include "utility.h"
#include "bvcommon.h"
#include "bv32externs.h"
#include "bv32strct.h"
#include "bitpack32.h"
#include "broadvoice/broadvoice.h"

BV_DECLARE(bv32_decode_state_t *) bv32_decode_init(bv32_decode_state_t *s)
{
    int i;

    if (s == NULL)
    {
        if ((s = (bv32_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    for (i = 0;  i < LPCO;  i++)
        s->lsplast[i] = (Float)(i + 1)/(Float)(LPCO + 1);
    Fzero(s->stsym, LPCO);
    Fzero(s->ltsym, LTMOFF);
    Fzero(s->lgpm, LGPORDER);
    Fzero(s->lsppm, LPCO*LSPPORDER);
    Fzero(s->dezfm, PFO);
    Fzero(s->depfm, PFO);
    s->cfecount = 0;
    s->idum = 0;
    s->scplcg = 1.0;
    s->per = 0;
    s->E = 0.0;
    for (i = 0;  i < LPCO;  i++)
        s->atplc[i + 1] = 0.0;
    s->pp_last = 100;
    s->prevlg[0] = MinE;
    s->prevlg[1] = MinE;
    s->lgq_last = MinE;
    s->lmax = -100.0;
    s->lmin = 100.0;
    s->lmean = 8.0;
    s->x1 = 13.5;
    s->level = 13.5;
    s->nclglim = 0;
    s->lctimer = 0;
    return s;
}

BV_DECLARE(int) bv32_decode(bv32_decode_state_t *ds,
                            int16_t amp[],
                            const uint8_t *in,
                            int len)
{
    Float xq[FRSZ];
    Float ltsym[LTMOFF + FRSZ];
    Float a[LPCO + 1];
    Float lspq[LPCO];
    Float bq[3];
    Float gainq[NSF];
    Float lgq[NSF];
    Float E;
    int16_t pp;
    int16_t i;
    Float bss;
    struct BV32_Bit_Stream bs;
    int ii;
    int outlen;

    outlen = 0;
    for (ii = 0;  ii < len;  ii += 20)
    {
        bv32_bitunpack(&in[ii], &bs);

        /* Reset frame erasure counter */
        ds->cfecount = 0;

        /* Decode spectral information */
        bv32_lspdec(lspq, bs.lspidx, ds->lsppm, ds->lsplast);
        lsp2a(lspq,	a);

        /* Decode pitch period & 3 pitch predictor taps */
        pp = (bs.ppidx + MINPP);
        bv32_pp3dec(bs.bqidx, bq);

        /* Decode excitation gain */
        for (i = 0;  i < NSF;  i++)
        {
            gainq[i] = bv32_gaindec(lgq + i,
                                    bs.gidx[i],
                                    ds->lgpm,
                                    ds->prevlg,
                                    ds->level,
                                    &ds->nclglim,
                                    ds->lctimer);

            if (ds->lctimer > 0)
                ds->lctimer = ds->lctimer - 1;
            if (ds->nclglim == NCLGLIM_TRAPPED)
                ds->lctimer = LEVEL_CONVERGENCE_TIME;

            /* Level estimation */
            bv32_estlevel(ds->prevlg[0], &ds->level, &ds->lmax, &ds->lmin, &ds->lmean, &ds->x1);
        }

        /* Copy state memory ltsym[] to local buffer */
        Fcopy(ltsym, ds->ltsym, LTMOFF);

        /* Decode the excitation signal */
        bv32_excdec_w_LT_synth(ltsym, bs.qvidx, gainq, bq, pp, &E);

        ds->E = E;

        /* LPC synthesis filtering of excitation */
        apfilter(a, LPCO, ltsym + LTMOFF, xq, FRSZ, ds->stsym, 1);

        /* Update pitch period of last frame */
        ds->pp_last = pp;

        /* Update signal memory */
        Fcopy(ds->ltsym, ltsym + FRSZ, LTMOFF);
        Fcopy(ds->bq_last, bq, 3);

        /* Update average quantized log-gain */
        ds->lgq_last = 0.5*(lgq[0] + lgq[1]);

        /* De-emphasis filtering */
        azfilter(bv32_a_pre, PFO, xq, xq, FRSZ, ds->dezfm, 1);
        apfilter(bv32_b_pre, PFO, xq, xq, FRSZ, ds->depfm, 1);

        F2s(&amp[outlen], xq, FRSZ);
        Fcopy(ds->lsplast, lspq, LPCO);

        Fcopy(ds->atplc, a, LPCO + 1);

        bss = bq[0] + bq[1] + bq[2];
        if (bss > 1.0)
            bss = 1.0;
        else if (bss < 0.0)
            bss = 0.0;
        ds->per = 0.5*ds->per + 0.5*bss;
        outlen += FRSZ;
    }
    return outlen;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv32_decode_release(bv32_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv32_decode_free(bv32_decode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
