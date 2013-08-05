/*
 * SpanDSP - a series of DSP components for telephony
 *
 * awgn.c - An additive Gaussian white noise generator
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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

/* This code is based on some demonstration code in a research
   paper somewhere. I can't track down where I got the original from,
   so that due recognition can be given. The original had no explicit
   copyright notice, and I hope nobody objects to its use here.

   Having a reasonable Gaussian noise generator is pretty important for
   telephony testing (in fact, pretty much any DSP testing), and this
   one seems to have served me OK. Since the generation of Gaussian
   noise is only for test purposes, and not a core system component,
   I don't intend to worry excessively about copyright issues, unless
   someone worries me.

   The non-core nature of this code also explains why it is unlikely
   to ever be optimised. */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/fast_convert.h"
#include "spandsp/saturated.h"
#include "spandsp/awgn.h"

#include "spandsp/private/awgn.h"

/* Gaussian noise generator constants */
#define M1 259200
#define IA1 7141
#define IC1 54773
#define RM1 (1.0/M1)
#define M2 134456
#define IA2 8121
#define IC2 28411
#define RM2 (1.0/M2)
#define M3 243000
#define IA3 4561
#define IC3 51349

static double ran1(awgn_state_t *s)
{
    double temp;
    int j;

    s->ix1 = (IA1*s->ix1 + IC1)%M1;
    s->ix2 = (IA2*s->ix2 + IC2)%M2;
    s->ix3 = (IA3*s->ix3 + IC3)%M3;
    j = 1 + ((97*s->ix3)/M3);
    if (j > 97  ||  j < 1)
    {
        /* Error */
        return -1;
    }
    temp = s->r[j];
    s->r[j] = (s->ix1 + s->ix2*RM2)*RM1;
    return temp;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(awgn_state_t *) awgn_init_dbov(awgn_state_t *s, int idum, float level)
{
    int j;

    if (s == NULL)
    {
        if ((s = (awgn_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    if (idum < 0)
        idum = -idum;

    s->rms = pow(10.0, level/20.0)*32768.0;

    s->ix1 = (IC1 + idum)%M1;
    s->ix1 = (IA1*s->ix1 + IC1)%M1;
    s->ix2 = s->ix1%M2;
    s->ix1 = (IA1*s->ix1 + IC1)%M1;
    s->ix3 = s->ix1%M3;
    s->r[0] = 0.0;
    for (j = 1;  j <= 97;  j++)
    {
        s->ix1 = (IA1*s->ix1 + IC1)%M1;
        s->ix2 = (IA2*s->ix2 + IC2)%M2;
        s->r[j] = (s->ix1 + s->ix2*RM2)*RM1;
    }
    s->gset = 0.0;
    s->iset = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(awgn_state_t *) awgn_init_dbm0(awgn_state_t *s, int idum, float level)
{
    return awgn_init_dbov(s, idum, level - DBM0_MAX_POWER);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) awgn_release(awgn_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) awgn_free(awgn_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) awgn(awgn_state_t *s)
{
    double fac;
    double r;
    double v1;
    double v2;
    double amp;

    if (s->iset == 0)
    {
        do
        {
            v1 = 2.0*ran1(s) - 1.0;
            v2 = 2.0*ran1(s) - 1.0;
            r = v1*v1 + v2*v2;
        }
        while (r >= 1.0);
        fac = sqrt(-2.0*log(r)/r);
        s->gset = v1*fac;
        s->iset = 1;
        amp = v2*fac*s->rms;
    }
    else
    {
        s->iset = 0;
        amp = s->gset*s->rms;
    }
    return fsaturate(amp);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
