/*
 * SpanDSP - a series of DSP components for telephony
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

#if !defined(_SPANDSP_TELEPHONY_H_)
#define _SPANDSP_TELEPHONY_H_

#if defined(_M_IX86)  ||  defined(_M_X64)
#if defined(LIBSPANDSP_EXPORTS)
#define SPAN_DECLARE(type)              __declspec(dllexport) type __stdcall
#define SPAN_DECLARE_NONSTD(type)       __declspec(dllexport) type __cdecl
#define SPAN_DECLARE_DATA               __declspec(dllexport)
#else
#define SPAN_DECLARE(type)              __declspec(dllimport) type __stdcall
#define SPAN_DECLARE_NONSTD(type)       __declspec(dllimport) type __cdecl
#define SPAN_DECLARE_DATA               __declspec(dllimport)
#endif
#elif defined(SPANDSP_USE_EXPORT_CAPABILITY)  &&  (defined(__GNUC__)  ||  defined(__SUNCC__))
#define SPAN_DECLARE(type)              __attribute__((visibility("default"))) type
#define SPAN_DECLARE_NONSTD(type)       __attribute__((visibility("default"))) type
#define SPAN_DECLARE_DATA               __attribute__((visibility("default")))
#else
#define SPAN_DECLARE(type)              /**/ type
#define SPAN_DECLARE_NONSTD(type)       /**/ type
#define SPAN_DECLARE_DATA               /**/
#endif

#define SAMPLE_RATE                 8000

/* This is based on A-law, but u-law is only 0.03dB different */
#define DBM0_MAX_POWER              (3.14f + 3.02f)
#define DBM0_MAX_SINE_POWER         (3.14f)
/* This is based on the ITU definition of dbOv in G.100.1 */
#define DBOV_MAX_POWER              (0.0f)
#define DBOV_MAX_SINE_POWER         (-3.02f)

/*! \brief A handler for pure receive. The buffer cannot be altered. */
typedef int (span_rx_handler_t)(void *s, const int16_t amp[], int len);

/*! \brief A handler for receive, where the buffer can be altered. */
typedef int (span_mod_handler_t)(void *s, int16_t amp[], int len);

/*! \brief A handler for missing receive data fill-in. */
typedef int (span_rx_fillin_handler_t)(void *s, int len);

/*! \brief A handler for transmit, where the buffer will be filled. */
typedef int (span_tx_handler_t)(void *s, int16_t amp[], int max_len);

#define ms_to_samples(t)            ((t)*(SAMPLE_RATE/1000))
#define us_to_samples(t)            ((t)/(1000000/SAMPLE_RATE))

#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

/* Fixed point constant macros */
#define FP_Q_9_7(x) ((int16_t) (128.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_8_8(x) ((int16_t) (256.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_7_9(x) ((int16_t) (512.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_6_10(x) ((int16_t) (1024.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_5_11(x) ((int16_t) (2048.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_4_12(x) ((int16_t) (4096.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_3_13(x) ((int16_t) (8192.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_2_14(x) ((int16_t) (16384.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_1_15(x) ((int16_t) (32768.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))

#define FP_Q_9_23(x) ((int32_t) (65536.0*128.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_8_24(x) ((int32_t) (65536.0*256.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_7_25(x) ((int32_t) (65536.0*512.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_6_26(x) ((int32_t) (65536.0*1024.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_5_27(x) ((int32_t) (65536.0*2048.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_4_28(x) ((int32_t) (65536.0*4096.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_3_29(x) ((int32_t) (65536.0*8192.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_2_30(x) ((int32_t) (65536.0*16384.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q_1_31(x) ((int32_t) (65536.0*32768.0*x + ((x >= 0.0)  ?  0.5  :  -0.5)))

#if defined(__cplusplus)
/* C++ doesn't seem to have sane rounding functions/macros yet */
#if !defined(WIN32)
#define lrint(x) ((long int) (x))
#define lrintf(x) ((long int) (x))
#endif
#endif

#endif
/*- End of file ------------------------------------------------------------*/
