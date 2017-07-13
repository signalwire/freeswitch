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
#define SPAN_DECLARE(type)              __declspec(dllexport) type
#define SPAN_DECLARE_DATA               __declspec(dllexport)
#else
#define SPAN_DECLARE(type)              __declspec(dllimport) type
#define SPAN_DECLARE_DATA               __declspec(dllimport)
#endif
#elif defined(SPANDSP_USE_EXPORT_CAPABILITY)  &&  (defined(__GNUC__)  ||  defined(__SUNCC__))
#define SPAN_DECLARE(type)              __attribute__((visibility("default"))) type
#define SPAN_DECLARE_DATA               __attribute__((visibility("default")))
#else
#define SPAN_DECLARE(type)              /**/ type
#define SPAN_DECLARE_DATA               /**/
#endif

#define span_container_of(ptr, type, member) ({ \
    const typeof(((type *) 0)->member) *__mptr = (ptr); \
    (type *) ((char *) __mptr - offsetof(type, member));})

#define SAMPLE_RATE                 8000

/* This is based on A-law, but u-law is only 0.03dB different */
#define DBM0_MAX_POWER              (3.14f + 3.02f)
#define DBM0_MAX_SINE_POWER         (3.14f)
/* This is based on the ITU definition of dbOv in G.100.1 */
#define DBOV_MAX_POWER              (0.0f)
#define DBOV_MAX_SINE_POWER         (-3.02f)

/*! \brief A handler for pure receive. The buffer cannot be altered. */
typedef int (*span_rx_handler_t)(void *s, const int16_t amp[], int len);

/*! \brief A handler for receive, where the buffer can be altered. */
typedef int (*span_mod_handler_t)(void *s, int16_t amp[], int len);

/*! \brief A handler for missing receive data fill-in. */
typedef int (*span_rx_fillin_handler_t)(void *s, int len);

/*! \brief A handler for transmit, where the buffer will be filled. */
typedef int (*span_tx_handler_t)(void *s, int16_t amp[], int max_len);

#define seconds_to_samples(t)       ((t)*SAMPLE_RATE)
#define milliseconds_to_samples(t)  ((t)*(SAMPLE_RATE/1000))
#define microseconds_to_samples(t)  ((t)/(1000000/SAMPLE_RATE))

#define ms_to_samples(t)            ((t)*(SAMPLE_RATE/1000))
#define us_to_samples(t)            ((t)/(1000000/SAMPLE_RATE))

/* Fixed point constant macros for 16 bit values */
#define FP_Q16_0(x) ((int16_t) (1.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q15_1(x) ((int16_t) (2.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q14_2(x) ((int16_t) (4.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q13_3(x) ((int16_t) (8.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q12_4(x) ((int16_t) (16.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q11_5(x) ((int16_t) (32.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q10_6(x) ((int16_t) (64.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q9_7(x) ((int16_t) (128.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q8_8(x) ((int16_t) (256.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q7_9(x) ((int16_t) (512.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q6_10(x) ((int16_t) (1024.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q5_11(x) ((int16_t) (2048.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q4_12(x) ((int16_t) (4096.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q3_13(x) ((int16_t) (8192.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q2_14(x) ((int16_t) (16384.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q1_15(x) ((int16_t) (32768.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))

/* Fixed point constant macros for 32 bit values */
#define FP_Q32_0(x) ((int32_t) (1.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q31_1(x) ((int32_t) (2.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q30_2(x) ((int32_t) (4.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q29_3(x) ((int32_t) (8.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q28_4(x) ((int32_t) (16.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q27_5(x) ((int32_t) (32.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q26_6(x) ((int32_t) (64.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q25_7(x) ((int32_t) (128.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q24_8(x) ((int32_t) (256.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q23_9(x) ((int32_t) (512.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q22_10(x) ((int32_t) (1024.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q21_11(x) ((int32_t) (2048.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q20_12(x) ((int32_t) (4096.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q19_13(x) ((int32_t) (8192.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q18_14(x) ((int32_t) (16384.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q17_15(x) ((int32_t) (32768.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q16_16(x) ((int32_t) (65536.0*1.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q15_17(x) ((int32_t) (65536.0*2.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q14_18(x) ((int32_t) (65536.0*4.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q13_19(x) ((int32_t) (65536.0*8.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q12_20(x) ((int32_t) (65536.0*16.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q11_21(x) ((int32_t) (65536.0*32.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q10_22(x) ((int32_t) (65536.0*64.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q9_23(x) ((int32_t) (65536.0*128.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q8_24(x) ((int32_t) (65536.0*256.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q7_25(x) ((int32_t) (65536.0*512.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q6_26(x) ((int32_t) (65536.0*1024.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q5_27(x) ((int32_t) (65536.0*2048.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q4_28(x) ((int32_t) (65536.0*4096.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q3_29(x) ((int32_t) (65536.0*8192.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q2_30(x) ((int32_t) (65536.0*16384.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))
#define FP_Q1_31(x) ((int32_t) (65536.0*32768.0*(x) + (((x) >= 0.0)  ?  0.5  :  -0.5)))

#if defined(__cplusplus)
/* C++ doesn't seem to have sane rounding functions/macros yet */
#if !defined(WIN32)
#define lrint(x) ((long int) (x))
#define lrintf(x) ((long int) (x))
#endif
#endif

#endif
/*- End of file ------------------------------------------------------------*/
