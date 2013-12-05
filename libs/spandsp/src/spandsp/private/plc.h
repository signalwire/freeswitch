/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/plc.h
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

/*! \file */

#if !defined(_SPANDSP_PRIVATE_PLC_H_)
#define _SPANDSP_PRIVATE_PLC_H_

/*! Minimum allowed pitch (66 Hz) */
#define PLC_PITCH_MIN           120
/*! Maximum allowed pitch (200 Hz) */
#define PLC_PITCH_MAX           40
/*! Maximum pitch OLA window */
#define PLC_PITCH_OVERLAP_MAX   (PLC_PITCH_MIN >> 2)
/*! The length over which the AMDF function looks for similarity (20 ms) */
#define CORRELATION_SPAN        160
/*! History buffer length. The buffer much also be at leat 1.25 times
    PLC_PITCH_MIN, but that is much smaller than the buffer needs to be for
    the pitch assessment. */
#define PLC_HISTORY_LEN         (CORRELATION_SPAN + PLC_PITCH_MIN)

/*!
    The generic packet loss concealer context.
*/
struct plc_state_s
{
    /*! Consecutive erased samples */
    int missing_samples;
    /*! Current offset into pitch period */
    int pitch_offset;
    /*! Pitch estimate */
    int pitch;
    /*! Buffer for a cycle of speech */
    float pitchbuf[PLC_PITCH_MIN];
    /*! History buffer */
    int16_t history[PLC_HISTORY_LEN];
    /*! Current pointer into the history buffer */
    int buf_ptr;
};

#endif
/*- End of file ------------------------------------------------------------*/
