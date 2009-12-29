/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v17_v32bis_tx_constellation_maps.h - ITU V.17 and V.32bis modems
 *                                      transmit part.
 *                                      Constellation mapping.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: v17_v32bis_tx_constellation_maps.h,v 1.1.4.1 2009/12/24 16:52:30 steveu Exp $
 */

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_14400_constellation[128] =
#else
static const complexf_t v17_v32bis_14400_constellation[128] =
#endif
{
    {-8, -3},       /* 0x00 */
    { 9,  2},       /* 0x01 */
    { 2, -9},       /* 0x02 */
    {-3,  8},       /* 0x03 */
    { 8,  3},       /* 0x04 */
    {-9, -2},       /* 0x05 */
    {-2,  9},       /* 0x06 */
    { 3, -8},       /* 0x07 */
    {-8,  1},       /* 0x08 */
    { 9, -2},       /* 0x09 */
    {-2, -9},       /* 0x0A */
    { 1,  8},       /* 0x0B */
    { 8, -1},       /* 0x0C */
    {-9,  2},       /* 0x0D */
    { 2,  9},       /* 0x0E */
    {-1, -8},       /* 0x0F */
    {-4, -3},       /* 0x10 */
    { 5,  2},       /* 0x11 */
    { 2, -5},       /* 0x12 */
    {-3,  4},       /* 0x13 */
    { 4,  3},       /* 0x14 */
    {-5, -2},       /* 0x15 */
    {-2,  5},       /* 0x16 */
    { 3, -4},       /* 0x17 */
    {-4,  1},       /* 0x18 */
    { 5, -2},       /* 0x19 */
    {-2, -5},       /* 0x1A */
    { 1,  4},       /* 0x1B */
    { 4, -1},       /* 0x1C */
    {-5,  2},       /* 0x1D */
    { 2,  5},       /* 0x1E */
    {-1, -4},       /* 0x1F */
    { 4, -3},       /* 0x20 */
    {-3,  2},       /* 0x21 */
    { 2,  3},       /* 0x22 */
    {-3, -4},       /* 0x23 */
    {-4,  3},       /* 0x24 */
    { 3, -2},       /* 0x25 */
    {-2, -3},       /* 0x26 */
    { 3,  4},       /* 0x27 */
    { 4,  1},       /* 0x28 */
    {-3, -2},       /* 0x29 */
    {-2,  3},       /* 0x2A */
    { 1, -4},       /* 0x2B */
    {-4, -1},       /* 0x2C */
    { 3,  2},       /* 0x2D */
    { 2, -3},       /* 0x2E */
    {-1,  4},       /* 0x2F */
    { 0, -3},       /* 0x30 */
    { 1,  2},       /* 0x31 */
    { 2, -1},       /* 0x32 */
    {-3,  0},       /* 0x33 */
    { 0,  3},       /* 0x34 */
    {-1, -2},       /* 0x35 */
    {-2,  1},       /* 0x36 */
    { 3,  0},       /* 0x37 */
    { 0,  1},       /* 0x38 */
    { 1, -2},       /* 0x39 */
    {-2, -1},       /* 0x3A */
    { 1,  0},       /* 0x3B */
    { 0, -1},       /* 0x3C */
    {-1,  2},       /* 0x3D */
    { 2,  1},       /* 0x3E */
    {-1,  0},       /* 0x3F */
    { 8, -3},       /* 0x40 */
    {-7,  2},       /* 0x41 */
    { 2,  7},       /* 0x42 */
    {-3, -8},       /* 0x43 */
    {-8,  3},       /* 0x44 */
    { 7, -2},       /* 0x45 */
    {-2, -7},       /* 0x46 */
    { 3,  8},       /* 0x47 */
    { 8,  1},       /* 0x48 */
    {-7, -2},       /* 0x49 */
    {-2,  7},       /* 0x4A */
    { 1, -8},       /* 0x4B */
    {-8, -1},       /* 0x4C */
    { 7,  2},       /* 0x4D */
    { 2, -7},       /* 0x4E */
    {-1,  8},       /* 0x4F */
    {-4, -7},       /* 0x50 */
    { 5,  6},       /* 0x51 */
    { 6, -5},       /* 0x52 */
    {-7,  4},       /* 0x53 */
    { 4,  7},       /* 0x54 */
    {-5, -6},       /* 0x55 */
    {-6,  5},       /* 0x56 */
    { 7, -4},       /* 0x57 */
    {-4,  5},       /* 0x58 */
    { 5, -6},       /* 0x59 */
    {-6, -5},       /* 0x5A */
    { 5,  4},       /* 0x5B */
    { 4, -5},       /* 0x5C */
    {-5,  6},       /* 0x5D */
    { 6,  5},       /* 0x5E */
    {-5, -4},       /* 0x5F */
    { 4, -7},       /* 0x60 */
    {-3,  6},       /* 0x61 */
    { 6,  3},       /* 0x62 */
    {-7, -4},       /* 0x63 */
    {-4,  7},       /* 0x64 */
    { 3, -6},       /* 0x65 */
    {-6, -3},       /* 0x66 */
    { 7,  4},       /* 0x67 */
    { 4,  5},       /* 0x68 */
    {-3, -6},       /* 0x69 */
    {-6,  3},       /* 0x6A */
    { 5, -4},       /* 0x6B */
    {-4, -5},       /* 0x6C */
    { 3,  6},       /* 0x6D */
    { 6, -3},       /* 0x6E */
    {-5,  4},       /* 0x6F */
    { 0, -7},       /* 0x70 */
    { 1,  6},       /* 0x71 */
    { 6, -1},       /* 0x72 */
    {-7,  0},       /* 0x73 */
    { 0,  7},       /* 0x74 */
    {-1, -6},       /* 0x75 */
    {-6,  1},       /* 0x76 */
    { 7,  0},       /* 0x77 */
    { 0,  5},       /* 0x78 */
    { 1, -6},       /* 0x79 */
    {-6, -1},       /* 0x7A */
    { 5,  0},       /* 0x7B */
    { 0, -5},       /* 0x7C */
    {-1,  6},       /* 0x7D */
    { 6,  1},       /* 0x7E */
    {-5,  0}        /* 0x7F */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_12000_constellation[64] =
#else
static const complexf_t v17_v32bis_12000_constellation[64] =
#endif
{
    { 7,  1},       /* 0x00 */
    {-5, -1},       /* 0x01 */
    {-1,  5},       /* 0x02 */
    { 1, -7},       /* 0x03 */
    {-7, -1},       /* 0x04 */
    { 5,  1},       /* 0x05 */
    { 1, -5},       /* 0x06 */
    {-1,  7},       /* 0x07 */
    { 3, -3},       /* 0x08 */
    {-1,  3},       /* 0x09 */
    { 3,  1},       /* 0x0A */
    {-3, -3},       /* 0x0B */
    {-3,  3},       /* 0x0C */
    { 1, -3},       /* 0x0D */
    {-3, -1},       /* 0x0E */
    { 3,  3},       /* 0x0F */
    { 7, -7},       /* 0x10 */
    {-5,  7},       /* 0x11 */
    { 7,  5},       /* 0x12 */
    {-7, -7},       /* 0x13 */
    {-7,  7},       /* 0x14 */
    { 5, -7},       /* 0x15 */
    {-7, -5},       /* 0x16 */
    { 7,  7},       /* 0x17 */
    {-1, -7},       /* 0x18 */
    { 3,  7},       /* 0x19 */
    { 7, -3},       /* 0x1A */
    {-7,  1},       /* 0x1B */
    { 1,  7},       /* 0x1C */
    {-3, -7},       /* 0x1D */
    {-7,  3},       /* 0x1E */
    { 7, -1},       /* 0x1F */
    { 3,  5},       /* 0x20 */
    {-1, -5},       /* 0x21 */
    {-5,  1},       /* 0x22 */
    { 5, -3},       /* 0x23 */
    {-3, -5},       /* 0x24 */
    { 1,  5},       /* 0x25 */
    { 5, -1},       /* 0x26 */
    {-5,  3},       /* 0x27 */
    {-1,  1},       /* 0x28 */
    { 3, -1},       /* 0x29 */
    {-1, -3},       /* 0x2A */
    { 1,  1},       /* 0x2B */
    { 1, -1},       /* 0x2C */
    {-3,  1},       /* 0x2D */
    { 1,  3},       /* 0x2E */
    {-1, -1},       /* 0x2F */
    {-5,  5},       /* 0x30 */
    { 7, -5},       /* 0x31 */
    {-5, -7},       /* 0x32 */
    { 5,  5},       /* 0x33 */
    { 5, -5},       /* 0x34 */
    {-7,  5},       /* 0x35 */
    { 5,  7},       /* 0x36 */
    {-5, -5},       /* 0x37 */
    {-5, -3},       /* 0x38 */
    { 7,  3},       /* 0x39 */
    { 3, -7},       /* 0x3A */
    {-3,  5},       /* 0x3B */
    { 5,  3},       /* 0x3C */
    {-7, -3},       /* 0x3D */
    {-3,  7},       /* 0x3E */
    { 3, -5}        /* 0x3F */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_9600_constellation[32] =
#else
static const complexf_t v17_v32bis_9600_constellation[32] =
#endif
{
    {-8,  2},       /* 0x00 */
    {-6, -4},       /* 0x01 */
    {-4,  6},       /* 0x02 */
    { 2,  8},       /* 0x03 */
    { 8, -2},       /* 0x04 */
    { 6,  4},       /* 0x05 */
    { 4, -6},       /* 0x06 */
    {-2, -8},       /* 0x07 */
    { 0,  2},       /* 0x08 */
    {-6,  4},       /* 0x09 */
    { 4,  6},       /* 0x0A */
    { 2,  0},       /* 0x0B */
    { 0, -2},       /* 0x0C */
    { 6, -4},       /* 0x0D */
    {-4, -6},       /* 0x0E */
    {-2,  0},       /* 0x0F */
    { 0, -6},       /* 0x10 */
    { 2, -4},       /* 0x11 */
    {-4, -2},       /* 0x12 */
    {-6,  0},       /* 0x13 */
    { 0,  6},       /* 0x14 */
    {-2,  4},       /* 0x15 */
    { 4,  2},       /* 0x16 */
    { 6,  0},       /* 0x17 */
    { 8,  2},       /* 0x18 */
    { 2,  4},       /* 0x19 */
    { 4, -2},       /* 0x1A */
    { 2, -8},       /* 0x1B */
    {-8, -2},       /* 0x1C */
    {-2, -4},       /* 0x1D */
    {-4,  2},       /* 0x1E */
    {-2,  8}        /* 0x1F */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_7200_constellation[16] =
#else
static const complexf_t v17_v32bis_7200_constellation[16] =
#endif
{
    { 6, -6},       /* 0x00 */
    {-2,  6},       /* 0x01 */
    { 6,  2},       /* 0x02 */
    {-6, -6},       /* 0x03 */
    {-6,  6},       /* 0x04 */
    { 2, -6},       /* 0x05 */
    {-6, -2},       /* 0x06 */
    { 6,  6},       /* 0x07 */
    {-2,  2},       /* 0x08 */
    { 6, -2},       /* 0x09 */
    {-2, -6},       /* 0x0A */
    { 2,  2},       /* 0x0B */
    { 2, -2},       /* 0x0C */
    {-6,  2},       /* 0x0D */
    { 2,  6},       /* 0x0E */
    {-2, -2}        /* 0x0F */
};

/* This one does not exist in V.17 as a data constellation. It is only
   the equaliser training constellation. In V.32/V.32bis it is a data mode. */
#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_4800_constellation[4] =
#else
static const complexf_t v17_v32bis_4800_constellation[4] =
#endif
{
    {-6, -2},       /* 0x00 */
    {-2,  6},       /* 0x01 */
    { 2, -6},       /* 0x02 */
    { 6,  2}        /* 0x03 */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_abcd_constellation[4] =
#else
static const complexf_t v17_v32bis_abcd_constellation[4] =
#endif
{
    {-6, -2},       /* A */
    { 2, -6},       /* B */
    { 6,  2},       /* C */
    {-2,  6}        /* D */
};

/*- End of file ------------------------------------------------------------*/
