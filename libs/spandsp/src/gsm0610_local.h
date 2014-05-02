/*
 * SpanDSP - a series of DSP components for telephony
 *
 * gsm0610_local.h - GSM 06.10 full rate speech codec.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * This code is based on the widely used GSM 06.10 code available from
 * http://kbs.cs.tu-berlin.de/~jutta/toast.html
 */

#if !defined(_GSM0610_LOCAL_H_)
#define _GSM0610_LOCAL_H_

#define GSM0610_FRAME_LEN               160

#define GSM0610_MAGIC                   0xD

#include "spandsp/private/gsm0610.h"

static __inline__ int16_t gsm_add(int16_t a, int16_t b)
{
#if defined(__GNUC__)  &&  (defined(__i386__)  ||  defined(__x86_64__))
    __asm__ __volatile__(
        " addw %2,%0;\n"
        " jno 0f;\n"
        " movw $0x7fff,%0;\n"
        " adcw $0,%0;\n"
        "0:"
        : "=&r" (a)
        : "0" (a), "ir" (b)
        : "cc"
    );
    return a;
#else
    int32_t sum;

    sum = (int32_t) a + (int32_t) b;
    return  saturate16(sum);
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t gsm_l_add(int32_t a, int32_t b)
{
#if defined(__GNUC__)  &&  (defined(__i386__)  ||  defined(__x86_64__))
    __asm__ __volatile__(
        " addl %2,%0;\n"
        " jno 0f;\n"
        " movl $0x7fffffff,%0;\n"
        " adcl $0,%0;\n"
        "0:"
        : "=&r" (a)
        : "0" (a), "ir" (b)
        : "cc"
    );
    return a;
#else
    uint32_t A;

    if (a < 0)
    {
        if (b >= 0)
            return  a + b;
        /*endif*/
        A = (uint32_t) -(a + 1) + (uint32_t) -(b + 1);
        return  (A >= INT32_MAX)  ?  INT32_MIN  :  -(int32_t) A - 2;
    }
    /*endif*/
    if (b <= 0)
        return  a + b;
    /*endif*/
    A = (uint32_t) a + (uint32_t) b;
    return  (A > INT32_MAX)  ?  INT32_MAX  :  A;
#endif
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t gsm_sub(int16_t a, int16_t b)
{
    int32_t diff;

    diff = (int32_t) a - (int32_t) b;
    return  saturate16(diff);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t gsm_mult(int16_t a, int16_t b)
{
    if (a == INT16_MIN  &&  b == INT16_MIN)
        return  INT16_MAX;
    /*endif*/
    return  (int16_t) (((int32_t) a * (int32_t) b) >> 15);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int32_t gsm_l_mult(int16_t a, int16_t b)
{
    assert (a != INT16_MIN  ||  b != INT16_MIN);
    return  ((int32_t) a * (int32_t) b) << 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t gsm_mult_r(int16_t a, int16_t b)
{
    int32_t prod;

    if (b == INT16_MIN  &&  a == INT16_MIN)
        return  INT16_MAX;
    /*endif*/
    prod = (int32_t) a * (int32_t) b + 16384;
    prod >>= 15;
    return  (int16_t) (prod & 0xFFFF);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t gsm_abs(int16_t a)
{
    return  (a == INT16_MIN)  ?  INT16_MAX  :  (int16_t) abs(a);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t gsm_asr(int16_t a, int n)
{
    if (n >= 16)
        return  (int16_t) (-(a < 0));
    /*endif*/
    if (n <= -16)
        return  0;
    /*endif*/
    if (n < 0)
        return (int16_t) (a << -n);
    /*endif*/
    return  (int16_t) (a >> n);
}
/*- End of function --------------------------------------------------------*/

static __inline__ int16_t gsm_asl(int16_t a, int n)
{
    if (n >= 16)
        return  0;
    /*endif*/
    if (n <= -16)
        return  (int16_t) (-(a < 0));
    /*endif*/
    if (n < 0)
        return  gsm_asr(a, -n);
    /*endif*/
    return  (int16_t) (a << n);
}
/*- End of function --------------------------------------------------------*/

extern void gsm0610_long_term_predictor(gsm0610_state_t *s,
                                        int16_t d[40],
                                        int16_t *dp,        /* [-120..-1] d'        IN  */
                                        int16_t e[40],
                                        int16_t dpp[40],
                                        int16_t *Nc,
                                        int16_t *bc);

extern void gsm0610_lpc_analysis(gsm0610_state_t *s,
                                 int16_t amp[160],
                                 int16_t LARc[8]);

extern void gsm0610_preprocess(gsm0610_state_t *s,
                               const int16_t amp[],
                               int16_t so[]);

extern void gsm0610_short_term_analysis_filter(gsm0610_state_t *s,
                                               int16_t LARc[8],
                                               int16_t amp[160]);

extern void gsm0610_long_term_synthesis_filtering(gsm0610_state_t *s,
                                                  int16_t Ncr,
                                                  int16_t bcr,
                                                  int16_t erp[40],
                                                  int16_t *drp);             /* [-120..-1] IN, [0..40] OUT */

extern void gsm0610_rpe_decoding(gsm0610_state_t *s,
                                 int16_t xmaxcr,
                                 int16_t Mcr,
                                 int16_t *xMcr,             /* [0..12], 3 bits             IN      */
                                 int16_t erp[40]);

extern void gsm0610_rpe_encoding(gsm0610_state_t *s,
                                 int16_t *e,                /* [-5..-1][0..39][40..44]     IN/OUT  */
                                 int16_t *xmaxc,
                                 int16_t *Mc,
                                 int16_t xMc[13]);

extern void gsm0610_short_term_synthesis_filter(gsm0610_state_t *s,
                                                int16_t LARcr[8],
                                                int16_t drp[40],
                                                int16_t amp[160]);

extern int16_t gsm0610_norm(int32_t a);

#endif

/*- End of include ---------------------------------------------------------*/
