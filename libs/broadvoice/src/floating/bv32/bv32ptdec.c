/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bv32ptdec.c -
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
 * $Id: bv32ptdec.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"
#include "bv32externs.h"

void bv32_pp3dec(int16_t idx, Float *b)
{
    const Float *fp;
    int	i;

    fp = bv32_pp9cb + idx*9;
    for (i = 0;  i < 3;  i++)
        b[i] = fp[i]*0.5;
}
