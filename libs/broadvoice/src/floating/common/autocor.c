/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * autocor.c -
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
 * $Id: autocor.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>

#include "typedef.h"
#include "bvcommon.h"

#define WINSZ 320    /* maximum analysis window size */

void Autocor(Float *r,              /* (o) : Autocorrelations */
             Float *x,              /* (i) : Input signal */
             const Float *window,   /* (i) : LPC Analysis window */
             int l_window,          /* (i) : window length */
             int m)                 /* (i) : LPC order */
{
    Float buf[WINSZ];
    Float a0;
    int i;
    int n;

    /* Apply analysis window */
    for (n = 0;  n < l_window;  n++)
        buf[n] = x[n]*window[n];

    /* Compute autocorrealtion coefficients up to lag order */
    for (i = 0;  i <= m;  i++)
    {
        a0 = 0.0F;
        for (n = i;  n < l_window;  n++)
            a0 += buf[n]*buf[n - i];
        r[i] = a0;
    }
}
