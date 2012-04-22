/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * cmtables.c -
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
 * $Id: cmtables.c,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "typedef.h"

Float	bwel[] =
{
    1.000000, 0.968526251091, 0.938025365420, 0.908506308303, 0.879914932377,
    0.852235186110, 0.825387345342, 0.799409908158, 0.774255089985
};

Float STWAL[]=
{
    1.000000, 0.750000, 0.562500, 0.421875, 0.316406, 0.237305, 0.177979, 0.133484, 0.100098
};

Float grid[]=
{
    0.9999390,  0.9935608,  0.9848633,  0.9725342,  0.9577942,  0.9409180,  0.9215393,  0.8995972,
    0.8753662,  0.8487854,  0.8198242,  0.7887573,  0.7558899,  0.7213440,  0.6853943,  0.6481323,
    0.6101379,  0.5709839,  0.5300903,  0.4882507,  0.4447632,  0.3993530,  0.3531189,  0.3058167,
    0.2585754,  0.2109680,  0.1630859,  0.1148682,  0.0657349,  0.0161438, -0.0335693, -0.0830994,
    -0.1319580, -0.1804199, -0.2279663, -0.2751465, -0.3224487, -0.3693237, -0.4155884, -0.4604187,
    -0.5034180, -0.5446472, -0.5848999, -0.6235962, -0.6612244, -0.6979980, -0.7336731, -0.7675781,
    -0.7998962, -0.8302002, -0.8584290, -0.8842468, -0.9077148, -0.9288635, -0.9472046, -0.9635010,
    -0.9772034, -0.9883118, -0.9955139, -0.9999390
};
