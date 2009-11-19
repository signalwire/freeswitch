/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * utility.c -
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
 * $Id: utility.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "utility.h"

void Fcopy(Float *y, Float *x, int size)
{
    while ((size--) > 0)
        *y++ = *x++;
}

void Fzero(Float *x, int size)
{
    while ((size--) > 0)
        *x++ = 0.0;
}

void F2s(int16_t *s, Float *f, int size)
{
    Float t;
    int16_t v;
    int i;

    for (i = 0;  i < size;  i++)
    {
        t = *f++;

        /* Rounding */
        if (t >= 0)
            t += 0.5;
        else
            t -= 0.5;

        if (t > 32767.0)
            v = 32767;
        else if (t < -32768.0)
            v = -32768;
        else
            v = (int16_t) t;
        *s++ = v;
    }
}
