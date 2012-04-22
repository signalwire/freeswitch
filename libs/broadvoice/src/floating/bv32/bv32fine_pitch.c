/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * fineptch.c -
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
 * $Id: bv32fine_pitch.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bv32cnst.h"
#include "bv32externs.h"

#define FS  (XOFF + 1)      /* Frame Starting index */
#define FE  (XOFF + FRSZ)   /* Frame Ending index */
#define DEV	6

int bv32_refinepitch(Float *x, int cpp, Float *ppt)
{
    Float cor;
    Float cor2;
    Float energy;
    Float cormax;
    Float cor2max;
    Float energymax;
    Float *fp0;
    Float *fp1;
    Float *fp2;
    Float *fp3;
    int	lb;
    int ub;
    int pp;
    int i;
    int j;

    if (cpp >= MAXPP)
        cpp = MAXPP - 1;
    if (cpp < MINPP)
        cpp = MINPP;
    lb = cpp - DEV;
    if (lb < MINPP)
        lb = MINPP; /* lower bound of pitch period search range */
    ub = cpp + DEV;
    /* to avoid selecting MAXPP as the refined pitch period */
    if (ub >= MAXPP)
        ub = MAXPP - 1; /* lower bound of pitch period search range */

    i = lb;				/* start the search from lower bound	    */

    fp0 = x + FS - 1;
    fp1 = x + FS - 1 - i;
    cor = energy = 0.0;
    for (j = 0;  j < (FE - FS + 1);  j++)
    {
        energy += (*fp1) * (*fp1);
        cor += (*fp0++) * (*fp1++);
    }

    pp = i;
    cormax = cor;
    cor2max = cor*cor;
    energymax = energy;

    fp0 = x + FE - lb - 1;
    fp1 = x + FS - lb - 2;
    for (i = lb + 1;  i <= ub;  i++)
    {
        fp2 = x + FS - 1;
        fp3 = x + FS - i - 1;
        cor = 0.;
        for (j = 0;  j < (FE - FS + 1);  j++)
            cor += (*fp2++)*(*fp3++);
        cor2 = cor*cor;
        energy += ((*fp1)*(*fp1) - (*fp0)*(*fp0));
        fp0--;
        fp1--;
        if ((cor2*energymax) > (cor2max*energy))
        {
            pp = i;
            cormax = cor;
            cor2max = cor2;
            energymax = energy;
        }
    }

    *ppt = (energymax != 0)  ?  (cormax/energymax)  :  0.0;

    return pp;
}
