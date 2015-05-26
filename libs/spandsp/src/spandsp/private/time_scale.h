/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/time_scale.h - Time scaling for linear speech data
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

#if !defined(_SPANDSP_PRIVATE_TIME_SCALE_H_)
#define _SPANDSP_PRIVATE_TIME_SCALE_H_

#define TIME_SCALE_MAX_SAMPLE_RATE  48000
#define TIME_SCALE_MIN_PITCH        60
#define TIME_SCALE_MAX_PITCH        250
#define TIME_SCALE_BUF_LEN          (2*TIME_SCALE_MAX_SAMPLE_RATE/TIME_SCALE_MIN_PITCH)

/*! Audio time scaling descriptor. */
struct time_scale_state_s
{
    /*! \brief The sample rate of both the incoming and outgoing signal */
    int sample_rate;
    /*! \brief The minimum pitch we will search for, in samples per cycle */
    int min_pitch;
    /*! \brief The maximum pitch we will search for, in samples per cycle */
    int max_pitch;
    /*! \brief The playout speed, as the fraction output time/input time.
         (i.e. >1.0 == slow down, 1.0 == no speed change, <1.0 == speed up) */
    float playout_rate;
    /*! \brief */
    double rcomp;
    /*! \brief The fractional sample adjustment, to allow for non-integer values of lcp. */
    double rate_nudge;
    /*! \brief */
    int lcp;
    /*! \brief The active length of buf at the current sample rate. */
    int buf_len;
    /*! \brief The number of samples in buf */
    int fill;
    /*! \brief Buffer for residual samples kept over from one call of time_scale() to
        the next. */
    int16_t buf[TIME_SCALE_BUF_LEN];
};

#endif
/*- End of file ------------------------------------------------------------*/
