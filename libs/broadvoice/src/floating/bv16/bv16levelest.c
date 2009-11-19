/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv16levelest.c - Signal level estimation
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
 * $Id: bv16levelest.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bv16externs.h"

Float	estlevel(
    Float	lg,
    Float	*level,
    Float	*lmax,
    Float	*lmin,
    Float	*lmean,
    Float	*x1,
    int16_t ngfae,
    int16_t nggalgc,
    Float   *estl_alpha_min)
{
    Float	lth;

    /* Reset forgetting factor for Lmin to fast decay.  This is to avoid Lmin staying at an
    incorrect low level compensating for the possibility it has caused incorrect bit-error
    declaration by making the estimated level too low. */
    if (nggalgc == 0)
    {
        *estl_alpha_min = estl_alpha1;
    }
    /* Reset forgetting factor for Lmin to regular decay if fast decay has taken place for
    the past Nfdm frames. */
    else if (nggalgc == Nfdm+1)
    {
        *estl_alpha_min = estl_alpha;
    }

    /* update the new maximum, minimum, & mean of log-gain */
    if (lg > *lmax)
    {
        *lmax=lg;	/* use new log-gain as max if it is > max */
    }
    else
    {
        *lmax=*lmean+estl_alpha*(*lmax-*lmean); /* o.w. attenuate toward lmean */
    }

    if (lg < *lmin && ngfae == LGPORDER+1 && nggalgc > LGPORDER
       )
    {
        *lmin=lg;	/* use new log-gain as min if it is < min */
        /* Reset forgetting factor for Lmin to regular decay in case it has been on
        fast decay since it has now found a new minimum level. */
        *estl_alpha_min = estl_alpha;
    }
    else
    {
        *lmin=*lmean+(*estl_alpha_min)*(*lmin-*lmean); /* o.w. attenuate toward lmean */
    }

    *lmean=estl_beta*(*lmean)+estl_beta1*(0.5*(*lmax+*lmin));

    /* update estimated input level, by calculating a running average
    (using an exponential window) of log-gains exceeding lmean */
    lth=*lmean+estl_TH*(*lmax-*lmean);
    if (lg > lth)
    {
        *x1=estl_a*(*x1)+estl_a1*lg;
        *level=estl_a*(*level)+estl_a1*(*x1);
    }

    return	lth;

}
