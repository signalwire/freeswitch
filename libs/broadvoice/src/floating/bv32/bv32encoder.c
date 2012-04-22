/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * encoder.c -
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
 * $Id: bv32encoder.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
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

BV_DECLARE(bv32_encode_state_t *) bv32_encode_init(bv32_encode_state_t *s)
{
    int k;

    if (s == NULL)
    {
        if ((s = (bv32_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    Fzero(s->lgpm, LGPORDER);
    s->allast[0] = 1.0;
    Fzero(s->allast + 1, LPCO);
    for (k = 0;  k < LPCO;  k++)
        s->lsplast[k] = (Float) (k + 1)/(Float)(LPCO + 1);
    Fzero(s->lsppm, LPCO*LSPPORDER);
    Fzero(s->x, XOFF);
    Fzero(s->xwd, XDOFF);
    Fzero(s->dq, XOFF);
    Fzero(s->stpem, LPCO);
    Fzero(s->stwpm, LPCO);
    Fzero(s->dfm, DFO);
    Fzero(s->stnfm, LPCO);
    Fzero(s->stsym, LPCO);
    Fzero(s->ltsym, MAXPP1 + FRSZ);
    Fzero(s->ltnfm, MAXPP1 + FRSZ);
    s->cpplast = 12*cpp_scale;
    Fzero(s->hpfzm,HPO);
    Fzero(s->hpfpm,HPO);
    s->prevlg[0] = MinE;
    s->prevlg[1] = MinE;
    s->lmax = -100.0;
    s->lmin = 100.0;
    s->lmean = 8.0;
    s->x1 = 13.5;
    s->level = 13.5;
    return s;
}

BV_DECLARE(int) bv32_encode(bv32_encode_state_t *cs,
                            uint8_t *out,
                            const int16_t amp[],
                            int len)
{
    Float x[LX];
    Float dq[LX];
    Float xw[FRSZ];
    Float r[LPCO + 1];
    Float a[LPCO + 1];
    Float aw[LPCO + 1];
    Float lsp[LPCO];
    Float lspq[LPCO];
    Float cbs[VDIM*CBSZ];
    Float qv[SFRSZ];
    Float bq[3];
    Float beta;
    Float gainq[2];
    Float lg;
    Float e;
    Float ee;
    Float ppt;
    Float lth;
    int	pp;
    int cpp;
    int	i;
    int issf;
    Float *fp0;
    Float *fp1;
    struct BV32_Bit_Stream bs;
    int ii;
    int outlen;

    outlen = 0;
    for (ii = 0;  ii < len;  ii += FRSZ)
    {
        /* Copy state memory to local memory buffers */
        Fcopy(x, cs->x, XOFF);
        for (i = 0;  i < FRSZ;  i++)
            x[XOFF + i] = (Float) amp[ii + i];

        /* High pass filtering & pre-emphasis filtering */
        azfilter(bv32_hpfb, HPO, x + XOFF, x + XOFF, FRSZ, cs->hpfzm, 1);
        apfilter(bv32_hpfa, HPO, x + XOFF, x + XOFF, FRSZ, cs->hpfpm, 1);

        /* Copy to coder state */
        Fcopy(cs->x, x + FRSZ, XOFF);

        /* Perform lpc analysis with asymmetrical window */
        Autocor(r, x + LX - WINSZ, bv32_winl, WINSZ, LPCO);	/* get autocorrelation lags */

        for (i = 0;  i <= LPCO;  i++)
            r[i] *= bv32_sstwin[i];	/* apply spectral smoothing */
        Levinson(r, a, cs->allast, LPCO); 			/* Levinson-Durbin recursion */
        for (i = 0;  i <= LPCO;  i++)
            a[i] *= bwel[i];

        a2lsp(a, lsp, cs->lsplast);

        bv32_lspquan(lspq, bs.lspidx, lsp, cs->lsppm);

        lsp2a(lspq, a);

        /* Calculate LPC prediction residual */
        Fcopy(dq, cs->dq, XOFF); 			/* copy dq() state to buffer */
        azfilter(a, LPCO, x + XOFF, dq + XOFF, FRSZ, cs->stpem, 1);

        /* Use weighted version of LPC filter as noise feedback filter */
        for (i = 0;  i <= LPCO;  i++)
            aw[i] = STWAL[i]*a[i];

        /* Get perceptually weighted version of speech */
        apfilter(aw, LPCO, dq + XOFF, xw, FRSZ, cs->stwpm, 1);

        /* Get the coarse version of pitch period using 8:1 decimation */
        cpp = bv32_coarsepitch(xw, cs->xwd, cs->dfm, cs->cpplast);
        cs->cpplast = cpp;

        /* Refine the pitch period in the neighborhood of coarse pitch period
           also calculate the pitch predictor tap for single-tap predictor */
        pp = bv32_refinepitch(dq, cpp, &ppt);
        bs.ppidx = pp - MINPP;

        /* vq 3 pitch predictor taps with minimum residual energy */
        bs.bqidx = bv32_pitchtapquan(dq, pp, bq);

        /* get coefficients for long-term noise feedback filter */
        if (ppt > 1.0)
            beta = LTWFL;
        else if (ppt < 0.0)
            beta = 0.0;
        else
            beta = LTWFL*ppt;

        /* Loop over excitation sub-frames */
        for (issf = 0;  issf < NSF;  issf++)
        {
            /* Calculate pitch prediction residual */
            fp0 = dq + XOFF + issf*SFRSZ;
            fp1 = dq + XOFF + issf*SFRSZ - (pp - 2) - 1;
            ee = 0.0;
            for (i = 0;  i < SFRSZ;  i++)
            {
                e = *fp0++ - bq[0]*fp1[0] - bq[1] * fp1[-1] - bq[2] * fp1[-2];
                fp1++;
                ee += e*e;
            }

            /* Log-gain quantization within each sub-frame */
            lg = (ee < TMinE)  ?  MinE  :  log(ee/SFRSZ)/log(2.0);
            bs.gidx[issf] = bv32_gainquan(gainq + issf, lg, cs->lgpm, cs->prevlg, cs->level);

            /* Level Estimation */
            lth = bv32_estlevel(cs->prevlg[0], &cs->level, &cs->lmax, &cs->lmin, &cs->lmean, &cs->x1);

            /* Scale the excitation codebook */
            for (i = 0;  i < (VDIM*CBSZ);  i++)
                cbs[i] = gainq[issf]*bv32_cccb[i];

            /* Perform noise feedback coding of the excitation signal */
            bv32_excquan(qv, bs.qvidx + issf*NVPSSF, dq + XOFF + issf*SFRSZ, aw, bq, beta, cs->ltsym, cs->ltnfm, cs->stnfm, cbs, pp);

            /* Update quantized short-term prediction residual buffer */
            Fcopy(dq + XOFF + issf*SFRSZ, qv, SFRSZ);
        }

        /* update state memory */
        Fcopy(cs->dq, dq + FRSZ, XOFF);
        Fcopy(cs->lsplast, lspq, LPCO);
        i = bv32_bitpack(out, &bs);
        out += i;
        outlen += i;
    }
    return outlen;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv32_encode_release(bv32_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv32_encode_free(bv32_encode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
