/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v29tx_constellation_maps.h - ITU V.29 modem transmit part.
 *                              Constellation mapping.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008, 2012 Steve Underwood
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

#if defined(SPANDSP_USE_FIXED_POINT)
static const complexi16_t v29_abab_constellation[6] =
#else
static const complexf_t v29_abab_constellation[6] =
#endif
{
    {FP_CONSTELLATION_SCALE( 3.0f), FP_CONSTELLATION_SCALE(-3.0f)},         /* 315deg high 9600 */
    {FP_CONSTELLATION_SCALE(-3.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /* 180deg low       */
    {FP_CONSTELLATION_SCALE( 1.0f), FP_CONSTELLATION_SCALE(-1.0f)},         /* 315deg low 7200  */
    {FP_CONSTELLATION_SCALE(-3.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /* 180deg low       */
    {FP_CONSTELLATION_SCALE( 0.0f), FP_CONSTELLATION_SCALE(-3.0f)},         /* 270deg low 4800  */
    {FP_CONSTELLATION_SCALE(-3.0f), FP_CONSTELLATION_SCALE( 0.0f)}          /* 180deg low       */
};

#if defined(SPANDSP_USE_FIXED_POINT)
static const complexi16_t v29_cdcd_constellation[6] =
#else
static const complexf_t v29_cdcd_constellation[6] =
#endif
{
    {FP_CONSTELLATION_SCALE( 3.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /*   0deg low 9600  */
    {FP_CONSTELLATION_SCALE(-3.0f), FP_CONSTELLATION_SCALE( 3.0f)},         /* 135deg high      */
    {FP_CONSTELLATION_SCALE( 3.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /*   0deg low 7200  */
    {FP_CONSTELLATION_SCALE(-1.0f), FP_CONSTELLATION_SCALE( 1.0f)},         /* 135deg low       */
    {FP_CONSTELLATION_SCALE( 3.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /*   0deg low 4800  */
    {FP_CONSTELLATION_SCALE( 0.0f), FP_CONSTELLATION_SCALE( 3.0f)}          /*  90deg low       */
};

#if defined(SPANDSP_USE_FIXED_POINT)
static const complexi16_t v29_9600_constellation[16] =
#else
static const complexf_t v29_9600_constellation[16] =
#endif
{
    {FP_CONSTELLATION_SCALE( 3.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /*   0deg low  */
    {FP_CONSTELLATION_SCALE( 1.0f), FP_CONSTELLATION_SCALE( 1.0f)},         /*  45deg low  */
    {FP_CONSTELLATION_SCALE( 0.0f), FP_CONSTELLATION_SCALE( 3.0f)},         /*  90deg low  */
    {FP_CONSTELLATION_SCALE(-1.0f), FP_CONSTELLATION_SCALE( 1.0f)},         /* 135deg low  */
    {FP_CONSTELLATION_SCALE(-3.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /* 180deg low  */
    {FP_CONSTELLATION_SCALE(-1.0f), FP_CONSTELLATION_SCALE(-1.0f)},         /* 225deg low  */
    {FP_CONSTELLATION_SCALE( 0.0f), FP_CONSTELLATION_SCALE(-3.0f)},         /* 270deg low  */
    {FP_CONSTELLATION_SCALE( 1.0f), FP_CONSTELLATION_SCALE(-1.0f)},         /* 315deg low  */
    {FP_CONSTELLATION_SCALE( 5.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /*   0deg high */
    {FP_CONSTELLATION_SCALE( 3.0f), FP_CONSTELLATION_SCALE( 3.0f)},         /*  45deg high */
    {FP_CONSTELLATION_SCALE( 0.0f), FP_CONSTELLATION_SCALE( 5.0f)},         /*  90deg high */
    {FP_CONSTELLATION_SCALE(-3.0f), FP_CONSTELLATION_SCALE( 3.0f)},         /* 135deg high */
    {FP_CONSTELLATION_SCALE(-5.0f), FP_CONSTELLATION_SCALE( 0.0f)},         /* 180deg high */
    {FP_CONSTELLATION_SCALE(-3.0f), FP_CONSTELLATION_SCALE(-3.0f)},         /* 225deg high */
    {FP_CONSTELLATION_SCALE( 0.0f), FP_CONSTELLATION_SCALE(-5.0f)},         /* 270deg high */
    {FP_CONSTELLATION_SCALE( 3.0f), FP_CONSTELLATION_SCALE(-3.0f)}          /* 315deg high */
};

/*- End of file ------------------------------------------------------------*/
