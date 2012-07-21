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
 */

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_14400_constellation[128] =
#else
static const complexf_t v17_v32bis_14400_constellation[128] =
#endif
{
    {FP_SCALE(-8.0f), FP_SCALE(-3.0f)},         /* 0x00 */
    {FP_SCALE( 9.0f), FP_SCALE( 2.0f)},         /* 0x01 */
    {FP_SCALE( 2.0f), FP_SCALE(-9.0f)},         /* 0x02 */
    {FP_SCALE(-3.0f), FP_SCALE( 8.0f)},         /* 0x03 */
    {FP_SCALE( 8.0f), FP_SCALE( 3.0f)},         /* 0x04 */
    {FP_SCALE(-9.0f), FP_SCALE(-2.0f)},         /* 0x05 */
    {FP_SCALE(-2.0f), FP_SCALE( 9.0f)},         /* 0x06 */
    {FP_SCALE( 3.0f), FP_SCALE(-8.0f)},         /* 0x07 */
    {FP_SCALE(-8.0f), FP_SCALE( 1.0f)},         /* 0x08 */
    {FP_SCALE( 9.0f), FP_SCALE(-2.0f)},         /* 0x09 */
    {FP_SCALE(-2.0f), FP_SCALE(-9.0f)},         /* 0x0A */
    {FP_SCALE( 1.0f), FP_SCALE( 8.0f)},         /* 0x0B */
    {FP_SCALE( 8.0f), FP_SCALE(-1.0f)},         /* 0x0C */
    {FP_SCALE(-9.0f), FP_SCALE( 2.0f)},         /* 0x0D */
    {FP_SCALE( 2.0f), FP_SCALE( 9.0f)},         /* 0x0E */
    {FP_SCALE(-1.0f), FP_SCALE(-8.0f)},         /* 0x0F */
    {FP_SCALE(-4.0f), FP_SCALE(-3.0f)},         /* 0x10 */
    {FP_SCALE( 5.0f), FP_SCALE( 2.0f)},         /* 0x11 */
    {FP_SCALE( 2.0f), FP_SCALE(-5.0f)},         /* 0x12 */
    {FP_SCALE(-3.0f), FP_SCALE( 4.0f)},         /* 0x13 */
    {FP_SCALE( 4.0f), FP_SCALE( 3.0f)},         /* 0x14 */
    {FP_SCALE(-5.0f), FP_SCALE(-2.0f)},         /* 0x15 */
    {FP_SCALE(-2.0f), FP_SCALE( 5.0f)},         /* 0x16 */
    {FP_SCALE( 3.0f), FP_SCALE(-4.0f)},         /* 0x17 */
    {FP_SCALE(-4.0f), FP_SCALE( 1.0f)},         /* 0x18 */
    {FP_SCALE( 5.0f), FP_SCALE(-2.0f)},         /* 0x19 */
    {FP_SCALE(-2.0f), FP_SCALE(-5.0f)},         /* 0x1A */
    {FP_SCALE( 1.0f), FP_SCALE( 4.0f)},         /* 0x1B */
    {FP_SCALE( 4.0f), FP_SCALE(-1.0f)},         /* 0x1C */
    {FP_SCALE(-5.0f), FP_SCALE( 2.0f)},         /* 0x1D */
    {FP_SCALE( 2.0f), FP_SCALE( 5.0f)},         /* 0x1E */
    {FP_SCALE(-1.0f), FP_SCALE(-4.0f)},         /* 0x1F */
    {FP_SCALE( 4.0f), FP_SCALE(-3.0f)},         /* 0x20 */
    {FP_SCALE(-3.0f), FP_SCALE( 2.0f)},         /* 0x21 */
    {FP_SCALE( 2.0f), FP_SCALE( 3.0f)},         /* 0x22 */
    {FP_SCALE(-3.0f), FP_SCALE(-4.0f)},         /* 0x23 */
    {FP_SCALE(-4.0f), FP_SCALE( 3.0f)},         /* 0x24 */
    {FP_SCALE( 3.0f), FP_SCALE(-2.0f)},         /* 0x25 */
    {FP_SCALE(-2.0f), FP_SCALE(-3.0f)},         /* 0x26 */
    {FP_SCALE( 3.0f), FP_SCALE( 4.0f)},         /* 0x27 */
    {FP_SCALE( 4.0f), FP_SCALE( 1.0f)},         /* 0x28 */
    {FP_SCALE(-3.0f), FP_SCALE(-2.0f)},         /* 0x29 */
    {FP_SCALE(-2.0f), FP_SCALE( 3.0f)},         /* 0x2A */
    {FP_SCALE( 1.0f), FP_SCALE(-4.0f)},         /* 0x2B */
    {FP_SCALE(-4.0f), FP_SCALE(-1.0f)},         /* 0x2C */
    {FP_SCALE( 3.0f), FP_SCALE( 2.0f)},         /* 0x2D */
    {FP_SCALE( 2.0f), FP_SCALE(-3.0f)},         /* 0x2E */
    {FP_SCALE(-1.0f), FP_SCALE( 4.0f)},         /* 0x2F */
    {FP_SCALE( 0.0f), FP_SCALE(-3.0f)},         /* 0x30 */
    {FP_SCALE( 1.0f), FP_SCALE( 2.0f)},         /* 0x31 */
    {FP_SCALE( 2.0f), FP_SCALE(-1.0f)},         /* 0x32 */
    {FP_SCALE(-3.0f), FP_SCALE( 0.0f)},         /* 0x33 */
    {FP_SCALE( 0.0f), FP_SCALE( 3.0f)},         /* 0x34 */
    {FP_SCALE(-1.0f), FP_SCALE(-2.0f)},         /* 0x35 */
    {FP_SCALE(-2.0f), FP_SCALE( 1.0f)},         /* 0x36 */
    {FP_SCALE( 3.0f), FP_SCALE( 0.0f)},         /* 0x37 */
    {FP_SCALE( 0.0f), FP_SCALE( 1.0f)},         /* 0x38 */
    {FP_SCALE( 1.0f), FP_SCALE(-2.0f)},         /* 0x39 */
    {FP_SCALE(-2.0f), FP_SCALE(-1.0f)},         /* 0x3A */
    {FP_SCALE( 1.0f), FP_SCALE( 0.0f)},         /* 0x3B */
    {FP_SCALE( 0.0f), FP_SCALE(-1.0f)},         /* 0x3C */
    {FP_SCALE(-1.0f), FP_SCALE( 2.0f)},         /* 0x3D */
    {FP_SCALE( 2.0f), FP_SCALE( 1.0f)},         /* 0x3E */
    {FP_SCALE(-1.0f), FP_SCALE( 0.0f)},         /* 0x3F */
    {FP_SCALE( 8.0f), FP_SCALE(-3.0f)},         /* 0x40 */
    {FP_SCALE(-7.0f), FP_SCALE( 2.0f)},         /* 0x41 */
    {FP_SCALE( 2.0f), FP_SCALE( 7.0f)},         /* 0x42 */
    {FP_SCALE(-3.0f), FP_SCALE(-8.0f)},         /* 0x43 */
    {FP_SCALE(-8.0f), FP_SCALE( 3.0f)},         /* 0x44 */
    {FP_SCALE( 7.0f), FP_SCALE(-2.0f)},         /* 0x45 */
    {FP_SCALE(-2.0f), FP_SCALE(-7.0f)},         /* 0x46 */
    {FP_SCALE( 3.0f), FP_SCALE( 8.0f)},         /* 0x47 */
    {FP_SCALE( 8.0f), FP_SCALE( 1.0f)},         /* 0x48 */
    {FP_SCALE(-7.0f), FP_SCALE(-2.0f)},         /* 0x49 */
    {FP_SCALE(-2.0f), FP_SCALE( 7.0f)},         /* 0x4A */
    {FP_SCALE( 1.0f), FP_SCALE(-8.0f)},         /* 0x4B */
    {FP_SCALE(-8.0f), FP_SCALE(-1.0f)},         /* 0x4C */
    {FP_SCALE( 7.0f), FP_SCALE( 2.0f)},         /* 0x4D */
    {FP_SCALE( 2.0f), FP_SCALE(-7.0f)},         /* 0x4E */
    {FP_SCALE(-1.0f), FP_SCALE( 8.0f)},         /* 0x4F */
    {FP_SCALE(-4.0f), FP_SCALE(-7.0f)},         /* 0x50 */
    {FP_SCALE( 5.0f), FP_SCALE( 6.0f)},         /* 0x51 */
    {FP_SCALE( 6.0f), FP_SCALE(-5.0f)},         /* 0x52 */
    {FP_SCALE(-7.0f), FP_SCALE( 4.0f)},         /* 0x53 */
    {FP_SCALE( 4.0f), FP_SCALE( 7.0f)},         /* 0x54 */
    {FP_SCALE(-5.0f), FP_SCALE(-6.0f)},         /* 0x55 */
    {FP_SCALE(-6.0f), FP_SCALE( 5.0f)},         /* 0x56 */
    {FP_SCALE( 7.0f), FP_SCALE(-4.0f)},         /* 0x57 */
    {FP_SCALE(-4.0f), FP_SCALE( 5.0f)},         /* 0x58 */
    {FP_SCALE( 5.0f), FP_SCALE(-6.0f)},         /* 0x59 */
    {FP_SCALE(-6.0f), FP_SCALE(-5.0f)},         /* 0x5A */
    {FP_SCALE( 5.0f), FP_SCALE( 4.0f)},         /* 0x5B */
    {FP_SCALE( 4.0f), FP_SCALE(-5.0f)},         /* 0x5C */
    {FP_SCALE(-5.0f), FP_SCALE( 6.0f)},         /* 0x5D */
    {FP_SCALE( 6.0f), FP_SCALE( 5.0f)},         /* 0x5E */
    {FP_SCALE(-5.0f), FP_SCALE(-4.0f)},         /* 0x5F */
    {FP_SCALE( 4.0f), FP_SCALE(-7.0f)},         /* 0x60 */
    {FP_SCALE(-3.0f), FP_SCALE( 6.0f)},         /* 0x61 */
    {FP_SCALE( 6.0f), FP_SCALE( 3.0f)},         /* 0x62 */
    {FP_SCALE(-7.0f), FP_SCALE(-4.0f)},         /* 0x63 */
    {FP_SCALE(-4.0f), FP_SCALE( 7.0f)},         /* 0x64 */
    {FP_SCALE( 3.0f), FP_SCALE(-6.0f)},         /* 0x65 */
    {FP_SCALE(-6.0f), FP_SCALE(-3.0f)},         /* 0x66 */
    {FP_SCALE( 7.0f), FP_SCALE( 4.0f)},         /* 0x67 */
    {FP_SCALE( 4.0f), FP_SCALE( 5.0f)},         /* 0x68 */
    {FP_SCALE(-3.0f), FP_SCALE(-6.0f)},         /* 0x69 */
    {FP_SCALE(-6.0f), FP_SCALE( 3.0f)},         /* 0x6A */
    {FP_SCALE( 5.0f), FP_SCALE(-4.0f)},         /* 0x6B */
    {FP_SCALE(-4.0f), FP_SCALE(-5.0f)},         /* 0x6C */
    {FP_SCALE( 3.0f), FP_SCALE( 6.0f)},         /* 0x6D */
    {FP_SCALE( 6.0f), FP_SCALE(-3.0f)},         /* 0x6E */
    {FP_SCALE(-5.0f), FP_SCALE( 4.0f)},         /* 0x6F */
    {FP_SCALE( 0.0f), FP_SCALE(-7.0f)},         /* 0x70 */
    {FP_SCALE( 1.0f), FP_SCALE( 6.0f)},         /* 0x71 */
    {FP_SCALE( 6.0f), FP_SCALE(-1.0f)},         /* 0x72 */
    {FP_SCALE(-7.0f), FP_SCALE( 0.0f)},         /* 0x73 */
    {FP_SCALE( 0.0f), FP_SCALE( 7.0f)},         /* 0x74 */
    {FP_SCALE(-1.0f), FP_SCALE(-6.0f)},         /* 0x75 */
    {FP_SCALE(-6.0f), FP_SCALE( 1.0f)},         /* 0x76 */
    {FP_SCALE( 7.0f), FP_SCALE( 0.0f)},         /* 0x77 */
    {FP_SCALE( 0.0f), FP_SCALE( 5.0f)},         /* 0x78 */
    {FP_SCALE( 1.0f), FP_SCALE(-6.0f)},         /* 0x79 */
    {FP_SCALE(-6.0f), FP_SCALE(-1.0f)},         /* 0x7A */
    {FP_SCALE( 5.0f), FP_SCALE( 0.0f)},         /* 0x7B */
    {FP_SCALE( 0.0f), FP_SCALE(-5.0f)},         /* 0x7C */
    {FP_SCALE(-1.0f), FP_SCALE( 6.0f)},         /* 0x7D */
    {FP_SCALE( 6.0f), FP_SCALE( 1.0f)},         /* 0x7E */
    {FP_SCALE(-5.0f), FP_SCALE( 0.0f)}          /* 0x7F */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_12000_constellation[64] =
#else
static const complexf_t v17_v32bis_12000_constellation[64] =
#endif
{
    {FP_SCALE( 7.0f), FP_SCALE( 1.0f)},         /* 0x00 */
    {FP_SCALE(-5.0f), FP_SCALE(-1.0f)},         /* 0x01 */
    {FP_SCALE(-1.0f), FP_SCALE( 5.0f)},         /* 0x02 */
    {FP_SCALE( 1.0f), FP_SCALE(-7.0f)},         /* 0x03 */
    {FP_SCALE(-7.0f), FP_SCALE(-1.0f)},         /* 0x04 */
    {FP_SCALE( 5.0f), FP_SCALE( 1.0f)},         /* 0x05 */
    {FP_SCALE( 1.0f), FP_SCALE(-5.0f)},         /* 0x06 */
    {FP_SCALE(-1.0f), FP_SCALE( 7.0f)},         /* 0x07 */
    {FP_SCALE( 3.0f), FP_SCALE(-3.0f)},         /* 0x08 */
    {FP_SCALE(-1.0f), FP_SCALE( 3.0f)},         /* 0x09 */
    {FP_SCALE( 3.0f), FP_SCALE( 1.0f)},         /* 0x0A */
    {FP_SCALE(-3.0f), FP_SCALE(-3.0f)},         /* 0x0B */
    {FP_SCALE(-3.0f), FP_SCALE( 3.0f)},         /* 0x0C */
    {FP_SCALE( 1.0f), FP_SCALE(-3.0f)},         /* 0x0D */
    {FP_SCALE(-3.0f), FP_SCALE(-1.0f)},         /* 0x0E */
    {FP_SCALE( 3.0f), FP_SCALE( 3.0f)},         /* 0x0F */
    {FP_SCALE( 7.0f), FP_SCALE(-7.0f)},         /* 0x10 */
    {FP_SCALE(-5.0f), FP_SCALE( 7.0f)},         /* 0x11 */
    {FP_SCALE( 7.0f), FP_SCALE( 5.0f)},         /* 0x12 */
    {FP_SCALE(-7.0f), FP_SCALE(-7.0f)},         /* 0x13 */
    {FP_SCALE(-7.0f), FP_SCALE( 7.0f)},         /* 0x14 */
    {FP_SCALE( 5.0f), FP_SCALE(-7.0f)},         /* 0x15 */
    {FP_SCALE(-7.0f), FP_SCALE(-5.0f)},         /* 0x16 */
    {FP_SCALE( 7.0f), FP_SCALE( 7.0f)},         /* 0x17 */
    {FP_SCALE(-1.0f), FP_SCALE(-7.0f)},         /* 0x18 */
    {FP_SCALE( 3.0f), FP_SCALE( 7.0f)},         /* 0x19 */
    {FP_SCALE( 7.0f), FP_SCALE(-3.0f)},         /* 0x1A */
    {FP_SCALE(-7.0f), FP_SCALE( 1.0f)},         /* 0x1B */
    {FP_SCALE( 1.0f), FP_SCALE( 7.0f)},         /* 0x1C */
    {FP_SCALE(-3.0f), FP_SCALE(-7.0f)},         /* 0x1D */
    {FP_SCALE(-7.0f), FP_SCALE( 3.0f)},         /* 0x1E */
    {FP_SCALE( 7.0f), FP_SCALE(-1.0f)},         /* 0x1F */
    {FP_SCALE( 3.0f), FP_SCALE( 5.0f)},         /* 0x20 */
    {FP_SCALE(-1.0f), FP_SCALE(-5.0f)},         /* 0x21 */
    {FP_SCALE(-5.0f), FP_SCALE( 1.0f)},         /* 0x22 */
    {FP_SCALE( 5.0f), FP_SCALE(-3.0f)},         /* 0x23 */
    {FP_SCALE(-3.0f), FP_SCALE(-5.0f)},         /* 0x24 */
    {FP_SCALE( 1.0f), FP_SCALE( 5.0f)},         /* 0x25 */
    {FP_SCALE( 5.0f), FP_SCALE(-1.0f)},         /* 0x26 */
    {FP_SCALE(-5.0f), FP_SCALE( 3.0f)},         /* 0x27 */
    {FP_SCALE(-1.0f), FP_SCALE( 1.0f)},         /* 0x28 */
    {FP_SCALE( 3.0f), FP_SCALE(-1.0f)},         /* 0x29 */
    {FP_SCALE(-1.0f), FP_SCALE(-3.0f)},         /* 0x2A */
    {FP_SCALE( 1.0f), FP_SCALE( 1.0f)},         /* 0x2B */
    {FP_SCALE( 1.0f), FP_SCALE(-1.0f)},         /* 0x2C */
    {FP_SCALE(-3.0f), FP_SCALE( 1.0f)},         /* 0x2D */
    {FP_SCALE( 1.0f), FP_SCALE( 3.0f)},         /* 0x2E */
    {FP_SCALE(-1.0f), FP_SCALE(-1.0f)},         /* 0x2F */
    {FP_SCALE(-5.0f), FP_SCALE( 5.0f)},         /* 0x30 */
    {FP_SCALE( 7.0f), FP_SCALE(-5.0f)},         /* 0x31 */
    {FP_SCALE(-5.0f), FP_SCALE(-7.0f)},         /* 0x32 */
    {FP_SCALE( 5.0f), FP_SCALE( 5.0f)},         /* 0x33 */
    {FP_SCALE( 5.0f), FP_SCALE(-5.0f)},         /* 0x34 */
    {FP_SCALE(-7.0f), FP_SCALE( 5.0f)},         /* 0x35 */
    {FP_SCALE( 5.0f), FP_SCALE( 7.0f)},         /* 0x36 */
    {FP_SCALE(-5.0f), FP_SCALE(-5.0f)},         /* 0x37 */
    {FP_SCALE(-5.0f), FP_SCALE(-3.0f)},         /* 0x38 */
    {FP_SCALE( 7.0f), FP_SCALE( 3.0f)},         /* 0x39 */
    {FP_SCALE( 3.0f), FP_SCALE(-7.0f)},         /* 0x3A */
    {FP_SCALE(-3.0f), FP_SCALE( 5.0f)},         /* 0x3B */
    {FP_SCALE( 5.0f), FP_SCALE( 3.0f)},         /* 0x3C */
    {FP_SCALE(-7.0f), FP_SCALE(-3.0f)},         /* 0x3D */
    {FP_SCALE(-3.0f), FP_SCALE( 7.0f)},         /* 0x3E */
    {FP_SCALE( 3.0f), FP_SCALE(-5.0f)}          /* 0x3F */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_9600_constellation[32] =
#else
static const complexf_t v17_v32bis_9600_constellation[32] =
#endif
{
    {FP_SCALE(-8.0f), FP_SCALE( 2.0f)},         /* 0x00 */
    {FP_SCALE(-6.0f), FP_SCALE(-4.0f)},         /* 0x01 */
    {FP_SCALE(-4.0f), FP_SCALE( 6.0f)},         /* 0x02 */
    {FP_SCALE( 2.0f), FP_SCALE( 8.0f)},         /* 0x03 */
    {FP_SCALE( 8.0f), FP_SCALE(-2.0f)},         /* 0x04 */
    {FP_SCALE( 6.0f), FP_SCALE( 4.0f)},         /* 0x05 */
    {FP_SCALE( 4.0f), FP_SCALE(-6.0f)},         /* 0x06 */
    {FP_SCALE(-2.0f), FP_SCALE(-8.0f)},         /* 0x07 */
    {FP_SCALE( 0.0f), FP_SCALE( 2.0f)},         /* 0x08 */
    {FP_SCALE(-6.0f), FP_SCALE( 4.0f)},         /* 0x09 */
    {FP_SCALE( 4.0f), FP_SCALE( 6.0f)},         /* 0x0A */
    {FP_SCALE( 2.0f), FP_SCALE( 0.0f)},         /* 0x0B */
    {FP_SCALE( 0.0f), FP_SCALE(-2.0f)},         /* 0x0C */
    {FP_SCALE( 6.0f), FP_SCALE(-4.0f)},         /* 0x0D */
    {FP_SCALE(-4.0f), FP_SCALE(-6.0f)},         /* 0x0E */
    {FP_SCALE(-2.0f), FP_SCALE( 0.0f)},         /* 0x0F */
    {FP_SCALE( 0.0f), FP_SCALE(-6.0f)},         /* 0x10 */
    {FP_SCALE( 2.0f), FP_SCALE(-4.0f)},         /* 0x11 */
    {FP_SCALE(-4.0f), FP_SCALE(-2.0f)},         /* 0x12 */
    {FP_SCALE(-6.0f), FP_SCALE( 0.0f)},         /* 0x13 */
    {FP_SCALE( 0.0f), FP_SCALE( 6.0f)},         /* 0x14 */
    {FP_SCALE(-2.0f), FP_SCALE( 4.0f)},         /* 0x15 */
    {FP_SCALE( 4.0f), FP_SCALE( 2.0f)},         /* 0x16 */
    {FP_SCALE( 6.0f), FP_SCALE( 0.0f)},         /* 0x17 */
    {FP_SCALE( 8.0f), FP_SCALE( 2.0f)},         /* 0x18 */
    {FP_SCALE( 2.0f), FP_SCALE( 4.0f)},         /* 0x19 */
    {FP_SCALE( 4.0f), FP_SCALE(-2.0f)},         /* 0x1A */
    {FP_SCALE( 2.0f), FP_SCALE(-8.0f)},         /* 0x1B */
    {FP_SCALE(-8.0f), FP_SCALE(-2.0f)},         /* 0x1C */
    {FP_SCALE(-2.0f), FP_SCALE(-4.0f)},         /* 0x1D */
    {FP_SCALE(-4.0f), FP_SCALE( 2.0f)},         /* 0x1E */
    {FP_SCALE(-2.0f), FP_SCALE( 8.0f)}          /* 0x1F */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_7200_constellation[16] =
#else
static const complexf_t v17_v32bis_7200_constellation[16] =
#endif
{
    {FP_SCALE( 6.0f), FP_SCALE(-6.0f)},         /* 0x00 */
    {FP_SCALE(-2.0f), FP_SCALE( 6.0f)},         /* 0x01 */
    {FP_SCALE( 6.0f), FP_SCALE( 2.0f)},         /* 0x02 */
    {FP_SCALE(-6.0f), FP_SCALE(-6.0f)},         /* 0x03 */
    {FP_SCALE(-6.0f), FP_SCALE( 6.0f)},         /* 0x04 */
    {FP_SCALE( 2.0f), FP_SCALE(-6.0f)},         /* 0x05 */
    {FP_SCALE(-6.0f), FP_SCALE(-2.0f)},         /* 0x06 */
    {FP_SCALE( 6.0f), FP_SCALE( 6.0f)},         /* 0x07 */
    {FP_SCALE(-2.0f), FP_SCALE( 2.0f)},         /* 0x08 */
    {FP_SCALE( 6.0f), FP_SCALE(-2.0f)},         /* 0x09 */
    {FP_SCALE(-2.0f), FP_SCALE(-6.0f)},         /* 0x0A */
    {FP_SCALE( 2.0f), FP_SCALE( 2.0f)},         /* 0x0B */
    {FP_SCALE( 2.0f), FP_SCALE(-2.0f)},         /* 0x0C */
    {FP_SCALE(-6.0f), FP_SCALE( 2.0f)},         /* 0x0D */
    {FP_SCALE( 2.0f), FP_SCALE( 6.0f)},         /* 0x0E */
    {FP_SCALE(-2.0f), FP_SCALE(-2.0f)}          /* 0x0F */
};

/* This one does not exist in V.17 as a data constellation. It is only
   the equaliser training constellation. In V.32/V.32bis it is a data mode. */
#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_4800_constellation[4] =
#else
static const complexf_t v17_v32bis_4800_constellation[4] =
#endif
{
    {FP_SCALE(-6.0f), FP_SCALE(-2.0f)},         /* 0x00 */
    {FP_SCALE(-2.0f), FP_SCALE( 6.0f)},         /* 0x01 */
    {FP_SCALE( 2.0f), FP_SCALE(-6.0f)},         /* 0x02 */
    {FP_SCALE( 6.0f), FP_SCALE( 2.0f)}          /* 0x03 */
};

#if defined(SPANDSP_USE_FIXED_POINTx)
static const complexi16_t v17_v32bis_abcd_constellation[4] =
#else
static const complexf_t v17_v32bis_abcd_constellation[4] =
#endif
{
    {FP_SCALE(-6.0f), FP_SCALE(-2.0f)},         /* A */
    {FP_SCALE( 2.0f), FP_SCALE(-6.0f)},         /* B */
    {FP_SCALE( 6.0f), FP_SCALE( 2.0f)},         /* C */
    {FP_SCALE(-2.0f), FP_SCALE( 6.0f)}          /* D */
};

/*- End of file ------------------------------------------------------------*/
