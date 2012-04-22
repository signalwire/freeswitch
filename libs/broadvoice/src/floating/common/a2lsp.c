/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * a2lsp.c -
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
 * $Id: a2lsp.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <math.h>
#include "typedef.h"
#include "bvcommon.h"

#define	PI	3.14159265358979

#define NAB	((LPCO >> 1) + 1)

static Float FNevChebP(Float x, Float *c, int nd2);

#define NBIS            4               /* number of bisections */

/*----------------------------------------------------------------------------
* a2lsp -  Convert predictor coefficients to line spectral pairs
*
* Description:
*  The transfer function of the prediction error filter is formed from the
*  predictor coefficients.  This polynomial is transformed into two reciprocal
*  polynomials having roots on the unit circle. The roots of these polynomials
*  interlace.  It is these roots that determine the line spectral pairs.
*  The two reciprocal polynomials are expressed as series expansions in
*  Chebyshev polynomials with roots in the range -1 to +1.  The inverse cosine
*  of the roots of the Chebyshev polynomial expansion gives the line spectral
*  pairs.  If np line spectral pairs are not found, this routine
*  stops with an error message.  This error occurs if the input coefficients
*  do not give a prediction error filter with minimum phase.
*
*  Line spectral pairs and predictor coefficients are usually expressed
*  algebraically as vectors.
*    lsp[0]     first (lowest frequency) line spectral pair
*    lsp[i]     1 <= i < np
*    pc[0]=1.0  predictor coefficient corresponding to lag 0
*    pc[i]      1 <= 1 <= np
*
* Parameters:
*  ->  Float pc[]
*      Vector of predictor coefficients (Np+1 values).  These are the
*      coefficients of the predictor filter, with pc[0] being the predictor
*      coefficient corresponding to lag 0, and pc[Np] corresponding to lag Np.
*      The predictor coeffs. must correspond to a minimum phase prediction
*      error filter.
*  <-  Float lsp[]
*      Array of Np line spectral pairss (in ascending order).  Each line
*      spectral pair lies in the range 0 to pi.
*  ->  int Np
*      Number of coefficients (at most LPCO = 50)
*----------------------------------------------------------------------------
*/

void a2lsp(Float pc[],       /* (i) input the np+1 predictor coeff.          */
           Float lsp[],      /* (o) line spectral pairs                      */
           Float old_lsp[])  /* (i/o) old lsp[] (in case not found 10 roots) */
{
    Float fa[NAB], fb[NAB];
    Float ta[NAB], tb[NAB];
    Float *t;
    Float xlow, xmid, xhigh;
    Float ylow, ymid, yhigh;
    Float xroot;
    Float dx;
    int i, j, nf, nd2, nab = ((LPCO>>1) + 1), ngrd;

    fb[0] = fa[0] = 1.0;
    for (i = 1, j = LPCO;  i <= (LPCO/2);  i++, j--)
    {
        fa[i] = pc[i] + pc[j] - fa[i-1];
        fb[i] = pc[i] - pc[j] + fb[i-1];
    }

    nd2 = LPCO/2;

    /*
    *   To look for roots on the unit circle, Ga(D) and Gb(D) are evaluated for
    *   D=exp(jw).  Since Gz(D) and Gb(D) are symmetric, they can be expressed in
    *   terms of a series in cos(nw) for D on the unit circle.  Since M is odd and
    *   D=exp(jw)
    *
    *           M-1        n
    *   Ga(D) = SUM fa(n) D             (symmetric, fa(n) = fa(M-1-n))
    *           n=0
    *                                    Mh-1
    *         = exp(j Mh w) [ f1(Mh) + 2 SUM fa(n) cos((Mh-n)w) ]
    *                                    n=0
    *                       Mh
    *         = exp(j Mh w) SUM ta(n) cos(nw),
    *                       n=0
    *
    *   where Mh=(M-1)/2=Nc-1.  The Nc=Mh+1 coefficients ta(n) are defined as
    *
    *   ta(n) =   fa(Nc-1),     n=0,
    *         = 2 fa(Nc-1-n),   n=1,...,Nc-1.
    *   The next step is to identify cos(nw) with the Chebyshev polynomial T(n,x).
    *   The Chebyshev polynomials satisfy the relationship T(n,cos(w)) = cos(nw).
    *   Omitting the exponential delay term, the series expansion in terms of
    *   Chebyshev polynomials is
    *
    *           Nc-1
    *   Ta(x) = SUM ta(n) T(n,x)
    *           n=0
    *
    *   The domain of Ta(x) is -1 < x < +1.  For a given root of Ta(x), say x0,
    *   the corresponding position of the root of Fa(D) on the unit circle is
    *   exp(j arccos(x0)).
    */
    ta[0] = fa[nab-1];
    tb[0] = fb[nab-1];
    for (i = 1, j = nab - 2; i < nab; ++i, --j)
    {
        ta[i] = 2.0 * fa[j];
        tb[i] = 2.0 * fb[j];
    }

    /*
    *   To find the roots, we sample the polynomials Ta(x) and Tb(x) looking for
    *   sign changes.  An interval containing a root is successively bisected to
    *   narrow the interval and then linear interpolation is used to estimate the
    *   root.  For a given root at x0, the line spectral pair is w0=acos(x0).
    *
    *   Since the roots of the two polynomials interlace, the search for roots
    *   alternates between the polynomials Ta(x) and Tb(x).  The sampling interval
    *   must be small enough to avoid having two cancelling sign changes in the
    *   same interval.  The sampling (grid) points were trained from a large amount
    *   of LSP vectors derived with high accuracy and stored in a table.
    */

    nf = 0;
    t = ta;
    xroot = 2.0;
    ngrd = 0;
    xlow = grid[0];
    ylow = FNevChebP(xlow, t, nd2);


    /* Root search loop */
    while (ngrd<(Ngrd-1) && nf < LPCO)
    {

        /* New trial point */
        ngrd++;
        xhigh = xlow;
        yhigh = ylow;
        xlow = grid[ngrd];
        ylow = FNevChebP(xlow, t, nd2);

        if (ylow * yhigh <= 0.0)
        {

            /* Bisections of the interval containing a sign change */
            dx = xhigh - xlow;
            for (i = 1; i <= NBIS; ++i)
            {
                dx = 0.5 * dx;
                xmid = xlow + dx;
                ymid = FNevChebP(xmid, t, nd2);
                if (ylow * ymid <= 0.0)
                {
                    yhigh = ymid;
                    xhigh = xmid;
                }
                else
                {
                    ylow = ymid;
                    xlow = xmid;
                }
            }

            /*
            * Linear interpolation in the subinterval with a sign change
            * (take care if yhigh=ylow=0)
            */
            if (yhigh != ylow)
                xmid = xlow + dx * ylow / (ylow - yhigh);
            else
                xmid = xlow + dx;

            /* New root position */
            lsp[nf] = acos(xmid)/PI;
            ++nf;

            /* Start the search for the roots of the next polynomial at the estimated
            * location of the root just found.  We have to catch the case that the
            * two polynomials have roots at the same place to avoid getting stuck at
            * that root.
            */
            if (xmid >= xroot)
            {
                xmid = xlow - dx;
            }
            xroot = xmid;
            if (t == ta)
                t = tb;
            else
                t = ta;
            xlow = xmid;
            ylow = FNevChebP(xlow, t, nd2);
        }
    }

    if (nf != LPCO)
    {
        /* LPCO roots have not been found */
        printf("\nWARNING: a2lsp failed to find all lsp nf=%d LPCO=%d\n", nf, LPCO);
        for (i = 0;  i < LPCO;  i++)
            lsp[i] = old_lsp[i];
    }
    else
    {
        /* Update LSP of previous frame with the new LSP */
        for (i = 0;  i < LPCO;  i++)
            old_lsp[i] = lsp[i];
    }
}

static Float FNevChebP(Float x,             /* (i) value 	*/
                       Float *c,            /* (i) coefficient array */
                       int nd2)
{
    Float t;
    Float b[NAB];
    int	i;

    t = x*2;
    b[0] = c[nd2];
    b[1] = c[nd2 - 1] + t*b[0];
    for (i = 2;  i < nd2;  i++)
        b[i] = c[nd2 - i] - b[i - 2] + t * b[i - 1];
    return (c[0] - b[nd2 - 2] + x * b[nd2 - 1]);
}
