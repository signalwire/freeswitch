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
 * $Id: bv16decoder.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "typedef.h"
#include "bv16cnst.h"
#include "bv16strct.h"
#include "bv16externs.h"
#include "bvcommon.h"
#include "utility.h"
#include "bv16postfilter.h"
#include "bitpack16.h"
#include "broadvoice/broadvoice.h"

BV_DECLARE(bv16_decode_state_t *) bv16_decode_init(bv16_decode_state_t *s)
{
    int i;

    if (s == NULL)
    {
        if ((s = (bv16_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    for (i = 0;  i < LPCO;  i++)
        s->lsplast[i] = (Float)(i + 1)/(Float)(LPCO + 1);
    Fzero(s->stsym, LPCO);
    Fzero(s->ltsym, LTMOFF);
    Fzero(s->xq, XQOFF);
    Fzero(s->lgpm, LGPORDER);
    Fzero(s->lsppm, LPCO*LSPPORDER);
    Fzero(s->prevlg, 2);
    s->pp_last = 50;
    s->cfecount = 0;
    s->idum = 0;
    s->per = 0;
    s->E = 0.0;
    for (i = 0;  i < LPCO;  i++)
        s->atplc[i + 1] = 0.0;
    s->ngfae = LGPORDER + 1;
    s->lmax = -100.0;
    s->lmin = 100.0;
    s->lmean = 12.5;
    s->x1 = 17.0;
    s->level = 17.0;
    s->nggalgc = Nfdm + 1;
    s->estl_alpha_min = estl_alpha;
    s->ma_a = 0.0;
    s->b_prv[0] = 1.0;
    s->b_prv[1] = 0.0;
    s->pp_prv = 100;
    return s;
}

BV_DECLARE(int) bv16_decode(bv16_decode_state_t *ds,
                            int16_t amp[],
                            const uint8_t *in,
                            int len)
{
    Float xq[LXQ];		/* quantized 8 kHz low-band signal */
    Float ltsym[LTMOFF + FRSZ];
    Float a[LPCO + 1];
    Float lspq[LPCO];
    Float bq[3];
    Float gainq;
    Float lgq;
    Float lg_el;
    Float xpf[FRSZ];
    int16_t pp;
    Float bss;
    Float E;
    struct BV16_Bit_Stream bs;
    int ii;
    int outlen;

    outlen = 0;
    for (ii = 0;  ii < len;  ii += 10)
    {
        bv16_bitunpack(&in[ii], &bs);
    
        /* Set frame erasure flags */
        if (ds->cfecount != 0)
        {
            ds->ngfae = 1;
        }
        else
        {
            ds->ngfae++;
            if (ds->ngfae > LGPORDER)
                ds->ngfae = LGPORDER + 1;
        }

        /* Reset frame erasure counter */
        ds->cfecount = 0;

        /* Decode pitch period */
        pp = bs.ppidx + MINPP;

        /* Decode spectral information */
        lspdec(lspq, bs.lspidx, ds->lsppm, ds->lsplast);
        lsp2a(lspq, a);
        Fcopy(ds->lsplast, lspq, LPCO);

        /* Decode pitch taps */
        bv16_pp3dec(bs.bqidx, bq);

        /* Decode gain */
        gainq = gaindec(&lgq, bs.gidx, ds->lgpm, ds->prevlg, ds->level, &ds->nggalgc, &lg_el);

        /* Copy state memory to buffer */
        Fcopy(ltsym, ds->ltsym, LTMOFF);
        Fcopy(xq, ds->xq, XQOFF);

        /* Decode the excitation signal including long-term synthesis and codevector scaling */
        excdec_w_LT_synth(ltsym, bs.qvidx, gainq, bq, pp, bv16_cccb, &E);

        ds->E = E;

        /* LPC synthesis filtering of short-term excitation */
        apfilter(a, LPCO, ltsym + LTMOFF, xq + XQOFF, FRSZ, ds->stsym, 1);

        /* Update the remaining state memory */
        ds->pp_last = pp;
        Fcopy(ds->xq, xq + FRSZ, XQOFF);
        Fcopy(ds->ltsym, ltsym + FRSZ, LTMOFF);
        Fcopy(ds->bq_last, bq, 3);

        /* Level estimation */
        estlevel(lg_el, &ds->level, &ds->lmax, &ds->lmin, &ds->lmean, &ds->x1, ds->ngfae, ds->nggalgc, &ds->estl_alpha_min);

        /* Adaptive postfiltering */
        postfilter(xq, pp, &(ds->ma_a), ds->b_prv, &(ds->pp_prv), xpf);
        F2s(&amp[outlen], xpf, FRSZ);

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

BV_DECLARE(int) bv16_decode_release(bv16_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv16_decode_free(bv16_decode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
