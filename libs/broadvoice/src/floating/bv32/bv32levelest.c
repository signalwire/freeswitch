/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * levelest.c - 
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
 * $Id: bv32levelest.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bv32cnst.h"
#include "bv32externs.h"

Float bv32_estlevel(Float lg,
                    Float *level,
                    Float *lmax,
                    Float *lmin,
                    Float *lmean,
                    Float *x1)
{
    Float lth;
    
    /* UPDATE THE NEW MAXIMUM, MINIMUM, & MEAN OF LOG-GAIN */
    if (lg > *lmax)
        *lmax = lg;	/* use new log-gain as max if it is > max */
    else
        *lmax = *lmean + estl_alpha*(*lmax - *lmean); /* o.w. attenuate toward lmean */
    if (lg < *lmin)
        *lmin=lg;	/* use new log-gain as min if it is < min */
    else
        *lmin = *lmean + estl_alpha*(*lmin - *lmean); /* o.w. attenuate toward lmean */
    *lmean = estl_beta*(*lmean) + estl_beta1*(0.5*(*lmax + *lmin));

    /* UPDATE ESTIMATED INPUT LEVEL, BY CALCULATING A RUNNING AVERAGE
    (USING AN EXPONENTIAL WINDOW) OF LOG-GAINS EXCEEDING lmean */
    lth = *lmean + estl_TH*(*lmax - *lmean);
    if (lg > lth)
    {
        *x1 = estl_a*(*x1) + estl_a1*lg;
        *level = estl_a*(*level) + estl_a1*(*x1);
    }

    return lth;
}
