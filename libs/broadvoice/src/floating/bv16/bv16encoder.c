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
#include "utility.h"
#include "bv16externs.h"
#include "bv16strct.h"
#include "bvcommon.h"
#include "bitpack16.h"
#include "broadvoice/broadvoice.h"

BV_DECLARE(bv16_encode_state_t *) bv16_encode_init(bv16_encode_state_t *s)
{
    int k;

    if (s == NULL)
    {
        if ((s = (bv16_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    Fzero(s->lgpm, LGPORDER);
    s->old_A[0] = 1.0;
    Fzero(s->old_A + 1, LPCO);
    for (k = 0;  k < LPCO;  k++)
        s->lsplast[k] = (Float) (k + 1)/(Float)(LPCO + 1);
    Fzero(s->lsppm, LPCO*LSPPORDER);
    Fzero(s->x, XOFF);
    Fzero(s->xwd, XDOFF);
    Fzero(s->dq, XOFF);
    Fzero(s->stpem, LPCO);
    Fzero(s->stwpm, LPCO);
    Fzero(s->dfm, DFO);
    Fzero(s->stsym, LPCO);
    Fzero(s->stnfz, NSTORDER);
    Fzero(s->stnfp, NSTORDER);
    Fzero(s->ltsym, MAXPP1 + FRSZ);
    Fzero(s->ltnfm, MAXPP1 + FRSZ);
    Fzero(s->hpfzm, HPO);
    Fzero(s->hpfpm, HPO);
    Fzero(s->prevlg, 2);
    s->cpplast = 12*cpp_scale;
    s->lmax = -100.0;
    s->lmin = 100.0;
    s->lmean = 12.5;
    s->x1 = 17.0;
    s->level = 17.0;
    return s;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv16_encode(bv16_encode_state_t *cs,
                            uint8_t *out,
                            const int16_t amp[],
                            int len)
{
    Float x[LX];			/* Signal buffer */
    Float dq[LX];		    /* Quantized int16_t term pred error, low-band */
    Float xw[FRSZ];		    /* Perceptually weighted low-band signal */
    Float r[NSTORDER + 1];
    Float a[LPCO + 1];
    Float aw[LPCO + 1];
    Float fsz[1 + NSTORDER];
    Float fsp[1 + NSTORDER];
    Float lsp[LPCO];
    Float lspq[LPCO];
    Float cbs[VDIM*CBSZ];
    Float bq[3];
    Float beta;
    Float gainq;
    Float lg;
    Float ppt;
    Float dummy;
    int	pp;
    int cpp;
    int	i;
    struct BV16_Bit_Stream bs;
    int ii;
    int outlen;

    outlen = 0;
    for (ii = 0;  ii < len;  ii += FRSZ)
    {
        /* Copy state memory to local memory buffers */
        Fcopy(x, cs->x, XOFF);
        for (i = 0;  i < FRSZ;  i++)
            x[XOFF + i] = (Float) amp[ii + i];

        /* 150Hz high pass filtering */
        azfilter(bv16_hpfb, HPO, x + XOFF, x + XOFF, FRSZ, cs->hpfzm, 1);
        apfilter(bv16_hpfa, HPO, x + XOFF, x + XOFF, FRSZ, cs->hpfpm, 1);

        /* Update highpass filtered signal buffer */
        Fcopy(cs->x, x + FRSZ, XOFF);

        /* Perform lpc analysis with asymmetrical window */
        Autocor(r, x + LX - WINSZ, bv16_winl, WINSZ, NSTORDER); /* get autocorrelation lags */
        for (i = 0;  i <= NSTORDER;  i++)
            r[i] *= bv16_sstwin[i]; /* apply spectral smoothing */
        Levinson(r, a, cs->old_A, LPCO); 			/* Levinson-Durbin recursion */

        /* Pole-zero noise feedback filter */
        for (i = 0;  i <= NSTORDER;  i++)
        {
            fsz[i] = a[i]*bv16_gfsz[i];
            fsp[i] = a[i]*bv16_gfsp[i];
        }

        /* Bandwidth expansion */
        for (i = 0;  i <= LPCO;  i++)
            a[i] *= bwel[i];

        /* LPC -> LSP Conversion */
        a2lsp(a, lsp, cs->lsplast);

        /* Spectral Quantization */
        lspquan(lspq, bs.lspidx, lsp, cs->lsppm);

        lsp2a(lspq, a);

        /* Calculate lpc prediction residual */
        Fcopy(dq, cs->dq, XOFF); 			/* copy dq() state to buffer */
        azfilter(a, LPCO, x + XOFF, dq + XOFF, FRSZ, cs->stpem, 1);

        /* Weighted version of lpc filter to generate weighted speech */
        for (i = 0;  i <= LPCO;  i++)
            aw[i] = STWAL[i]*a[i];

        /* Get perceptually weighted speech signal */
        apfilter(aw, LPCO, dq + XOFF, xw, FRSZ, cs->stwpm, 1);

        /* Get the coarse version of pitch period using 4:1 decimation */
        cpp = coarsepitch(xw, cs->xwd, cs->dfm, cs->cpplast);
        cs->cpplast = cpp;

        /* Refine the pitch period in the neighborhood of coarse pitch period
           also calculate the pitch predictor tap for single-tap predictor */
        pp = refinepitch(dq, cpp, &ppt);
        bs.ppidx = (int16_t) (pp - MINPP);

        /* Vector quantize 3 pitch predictor taps with minimum residual energy */
        bs.bqidx = (int16_t) pitchtapquan(dq, pp, bq, &lg);

        /* Get coefficients of long-term noise feedback filter */
        if (ppt > 1.0)
            beta = LTWFL;
        else if (ppt < 0.0)
            beta = 0.0;
        else
            beta = LTWFL*ppt;

        /* Gain quantization */
        lg = (lg < FRSZ)  ?  0  :  log(lg/FRSZ)/log(2.0);
        bs.gidx = (int16_t) gainquan(&gainq, lg, cs->lgpm, cs->prevlg, cs->level);

        /* Level estimation */
        dummy = estl_alpha;
        estlevel(cs->prevlg[0],
                 &cs->level,
                 &cs->lmax,
                 &cs->lmin,
                 &cs->lmean,
                 &cs->x1,
                 LGPORDER + 1,
                 Nfdm + 1,
                 &dummy);

        /* Scale the scalar quantizer codebook */
        for (i = 0;  i < (VDIM*CBSZ);  i++)
            cbs[i] = gainq*bv16_cccb[i];

        /* Perform noise feedback coding of the excitation signal */
        excquan(bs.qvidx, x + XOFF, a, fsz, fsp, bq, beta, cs->stsym,
                cs->ltsym, cs->ltnfm, cs->stnfz, cs->stnfp, cbs, pp);

        /* Update state memory */
        Fcopy(dq + XOFF, cs->ltsym + MAXPP1, FRSZ);
        Fcopy(cs->dq, dq + FRSZ, XOFF);
        i = bv16_bitpack(out, &bs);
        out += i;
        outlen += i;
    }
    return outlen;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv16_encode_release(bv16_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

BV_DECLARE(int) bv16_encode_free(bv16_encode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
