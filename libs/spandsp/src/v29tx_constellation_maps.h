/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29tx_constellation_maps.h - ITU V.29 modem transmit part.
 *                              Constellation mapping.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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
 * $Id: v29tx_constellation_maps.h,v 1.2 2008/09/04 14:40:05 steveu Exp $
 */

#if defined(SPANDSP_USE_FIXED_POINT)
static const complexi16_t v29_abab_constellation[6] =
#else
static const complexf_t v29_abab_constellation[6] =
#endif
{
    { 3, -3},           /* 315deg high 9600 */
    {-3,  0},           /* 180deg low       */
    { 1, -1},           /* 315deg low 7200  */
    {-3,  0},           /* 180deg low       */
    { 0, -3},           /* 270deg low 4800  */
    {-3,  0}            /* 180deg low       */
};

#if defined(SPANDSP_USE_FIXED_POINT)
static const complexi16_t v29_cdcd_constellation[6] =
#else
static const complexf_t v29_cdcd_constellation[6] =
#endif
{
    { 3,  0},           /*   0deg low 9600  */
    {-3,  3},           /* 135deg high      */
    { 3,  0},           /*   0deg low 7200  */
    {-1,  1},           /* 135deg low       */
    { 3,  0},           /*   0deg low 4800  */
    { 0,  3}            /*  90deg low       */
};

#if defined(SPANDSP_USE_FIXED_POINT)
static const complexi16_t v29_9600_constellation[16] =
#else
static const complexf_t v29_9600_constellation[16] =
#endif
{
    { 3,  0},           /*   0deg low  */
    { 1,  1},           /*  45deg low  */
    { 0,  3},           /*  90deg low  */
    {-1,  1},           /* 135deg low  */
    {-3,  0},           /* 180deg low  */
    {-1, -1},           /* 225deg low  */
    { 0, -3},           /* 270deg low  */
    { 1, -1},           /* 315deg low  */
    { 5,  0},           /*   0deg high */
    { 3,  3},           /*  45deg high */
    { 0,  5},           /*  90deg high */
    {-3,  3},           /* 135deg high */
    {-5,  0},           /* 180deg high */
    {-3, -3},           /* 225deg high */
    { 0, -5},           /* 270deg high */
    { 3, -3}            /* 315deg high */
};

/*- End of file ------------------------------------------------------------*/
