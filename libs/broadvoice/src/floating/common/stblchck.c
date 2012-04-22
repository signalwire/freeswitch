/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * stblchck.c -
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
 * $Id: stblchck.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bvcommon.h"

int stblchck(Float *x, int vdim)
{
    int k;
    int stbl;

    if (x[0] < 0.0)
    {
        stbl = 0;
    }
    else
    {
        stbl = 1;
        for (k = 1;  k < vdim;  k++)
        {
            if (x[k] - x[k-1] < 0.0)
                stbl = 0;
        }
    }
    return stbl;
}
