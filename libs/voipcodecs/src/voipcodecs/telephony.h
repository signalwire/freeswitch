/*
 * VoIPcodecs - a series of DSP components for telephony
 *
 * telephony.h - some very basic telephony definitions
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License version 2.1, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: telephony.h,v 1.10 2007/04/05 19:20:50 steveu Exp $
 */

#if !defined(_SPANDSP_TELEPHONY_H_)
#define _SPANDSP_TELEPHONY_H_

#define SAMPLE_RATE             8000

/* This is based on A-law, but u-law is only 0.03dB different */
#define DBM0_MAX_POWER          (3.14f + 3.02f)
#define DBM0_MAX_SINE_POWER     (3.14f)
/* This is based on the ITU definition of dbOv in G.100.1 */
#define DBOV_MAX_POWER          (0.0f)
#define DBOV_MAX_SINE_POWER     (-3.02f)

/*! \brief A handler for pure receive. The buffer cannot be altered. */
typedef int (span_rx_handler_t)(void *s, const int16_t amp[], int len);

/*! \brief A handler for receive, where the buffer can be altered. */
typedef int (span_mod_handler_t)(void *s, int16_t amp[], int len);

/*! \brief A handler for transmit, where the buffer will be filled. */
typedef int (span_tx_handler_t)(void *s, int16_t amp[], int max_len);

#define ms_to_samples(t)    (((t)*SAMPLE_RATE)/1000)

#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

#include <assert.h>
#if (_MSC_VER >= 1400)			// VC8+
#define vc_assert(expr) assert(expr);__analysis_assume( expr )
#else
#define vc_assert(expr) assert(expr)
#endif

#if defined(__cplusplus)
/* C++ doesn't seem to have sane rounding functions/macros yet */
#ifndef _MSC_VER
#define lrint(x) ((long int) (x))
#define lrintf(x) ((long int) (x))
#endif
#endif

#endif
/*- End of file ------------------------------------------------------------*/
