/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722.c - The ITU G.722 codec.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
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
 * Based in part on a single channel G.722 codec which is:
 *
 * Copyright (c) CMU 1993
 * Computer Science, Speech Group
 * Chengxiang Lu and Alex Hauptmann
 *
 * $Id: g722.c,v 1.10 2009/04/22 12:57:40 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <memory.h>
#include <stdlib.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/saturated.h"
#include "spandsp/vector_int.h"
#include "spandsp/g722.h"

#include "spandsp/private/g722.h"

static const int16_t qmf_coeffs_fwd[12] =
{
      3,  -11,   12,   32, -210,  951, 3876, -805,  362, -156,   53,  -11,
};

static const int16_t qmf_coeffs_rev[12] =
{
    -11,   53, -156,  362, -805, 3876,  951, -210,   32,   12,  -11,    3
};

static const int16_t qm2[4] =
{
    -7408,  -1616,   7408,   1616
};

static const int16_t qm4[16] =
{
         0, -20456, -12896,  -8968,
     -6288,  -4240,  -2584,  -1200,
     20456,  12896,   8968,   6288,
      4240,   2584,   1200,      0
};

static const int16_t qm5[32] =
{
      -280,   -280, -23352, -17560,
    -14120, -11664,  -9752,  -8184,
     -6864,  -5712,  -4696,  -3784,
     -2960,  -2208,  -1520,   -880,
     23352,  17560,  14120,  11664,
      9752,   8184,   6864,   5712,
      4696,   3784,   2960,   2208,
      1520,    880,    280,   -280
};

static const int16_t qm6[64] =
{
      -136,   -136,   -136,   -136,
    -24808, -21904, -19008, -16704,
    -14984, -13512, -12280, -11192,
    -10232,  -9360,  -8576,  -7856,
     -7192,  -6576,  -6000,  -5456,
     -4944,  -4464,  -4008,  -3576,
     -3168,  -2776,  -2400,  -2032,
     -1688,  -1360,  -1040,   -728,
     24808,  21904,  19008,  16704,
     14984,  13512,  12280,  11192,
     10232,   9360,   8576,   7856,
      7192,   6576,   6000,   5456,
      4944,   4464,   4008,   3576,
      3168,   2776,   2400,   2032,
      1688,   1360,   1040,    728,
       432,    136,   -432,   -136
};

static const int16_t q6[32] =
{
         0,     35,     72,    110,
       150,    190,    233,    276,
       323,    370,    422,    473,
       530,    587,    650,    714,
       786,    858,    940,   1023,
      1121,   1219,   1339,   1458,
      1612,   1765,   1980,   2195,
      2557,   2919,      0,      0
};

static const int16_t ilb[32] =
{
      2048,   2093,   2139,   2186,
      2233,   2282,   2332,   2383,
      2435,   2489,   2543,   2599,
      2656,   2714,   2774,   2834,
      2896,   2960,   3025,   3091,
      3158,   3228,   3298,   3371,
      3444,   3520,   3597,   3676,
      3756,   3838,   3922,   4008
};

static const int16_t iln[32] =
{
     0, 63, 62, 31, 30, 29, 28, 27,
    26, 25, 24, 23, 22, 21, 20, 19,
    18, 17, 16, 15, 14, 13, 12, 11,
    10,  9,  8,  7,  6,  5,  4,  0
};

static const int16_t ilp[32] =
{
     0, 61, 60, 59, 58, 57, 56, 55,
    54, 53, 52, 51, 50, 49, 48, 47,
    46, 45, 44, 43, 42, 41, 40, 39,
    38, 37, 36, 35, 34, 33, 32,  0
};

static const int16_t ihn[3] =
{
    0,  1,  0
};

static const int16_t ihp[3] =
{
    0,  3,  2
};

static const int16_t wl[8] =
{
    -60, -30, 58, 172, 334, 538, 1198, 3042
};

static const int16_t rl42[16] =
{
    0,  7,  6,  5,  4,  3,  2,  1,
    7,  6,  5,  4,  3,  2,  1,  0
};

static const int16_t wh[3] =
{
    0, -214, 798
};

static const int16_t rh2[4] =
{
    2,  1,  2,  1
};

static void block4(g722_band_t *s, int16_t dx)
{
    int16_t wd1;
    int16_t wd2;
    int16_t wd3;
    int16_t sp;
    int16_t r;
    int16_t p;
    int16_t ap[2];
    int32_t wd32;
    int32_t sz;
    int i;

    /* RECONS */
    r = saturated_add16(s->s, dx);
    /* PARREC */
    p = saturated_add16(s->sz, dx);

    /* UPPOL2 */
    wd1 = saturate((int32_t) s->a[0] << 2);
    wd32 = ((p ^ s->p[0]) & 0x8000)  ?  wd1  :  -wd1;
    if (wd32 > 32767)
        wd32 = 32767;
    wd3 = (int16_t) ((((p ^ s->p[1]) & 0x8000)  ?  -128  :  128)
                     + (wd32 >> 7)
                     + (((int32_t) s->a[1]*(int32_t) 32512) >> 15));
    if (abs(wd3) > 12288)
        wd3 = (wd3 < 0)  ?  -12288  :  12288;
    ap[1] = wd3;

    /* UPPOL1 */
    wd1 = ((p ^ s->p[0]) & 0x8000)  ?  -192  :  192;
    wd2 = (int16_t) (((int32_t) s->a[0]*(int32_t) 32640) >> 15);
    ap[0] = saturated_add16(wd1, wd2);

    wd3 = saturated_sub16(15360, ap[1]);
    if (abs(ap[0]) > wd3)
        ap[0] = (ap[0] < 0)  ?  -wd3  :  wd3;

    /* FILTEP */
    wd1 = saturated_add16(r, r);
    wd1 = (int16_t) (((int32_t) ap[0]*(int32_t) wd1) >> 15);
    wd2 = saturated_add16(s->r, s->r);
    wd2 = (int16_t) (((int32_t) ap[1]*(int32_t) wd2) >> 15);
    sp = saturated_add16(wd1, wd2);
    s->r = r;
    s->a[1] = ap[1];
    s->a[0] = ap[0];
    s->p[1] = s->p[0];
    s->p[0] = p;

    /* UPZERO */
    /* DELAYA */
    /* FILTEZ */
    wd1 = (dx == 0)  ?  0  :  128;
    s->d[0] = dx;
    sz = 0;
    for (i = 5;  i >= 0;  i--)
    {
        wd2 = ((s->d[i + 1] ^ dx) & 0x8000)  ?  -wd1  :  wd1;
        wd3 = (int16_t) (((int32_t) s->b[i]*(int32_t) 32640) >> 15);
        s->b[i] = saturated_add16(wd2, wd3);
        wd3 = saturated_add16(s->d[i], s->d[i]);
        sz += ((int32_t) s->b[i]*(int32_t) wd3) >> 15;
        s->d[i + 1] = s->d[i];
    }
    s->sz = saturate(sz);

    /* PREDIC */
    s->s = saturated_add16(sp, s->sz);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(g722_decode_state_t *) g722_decode_init(g722_decode_state_t *s, int rate, int options)
{
    if (s == NULL)
    {
        if ((s = (g722_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    if (rate == 48000)
        s->bits_per_sample = 6;
    else if (rate == 56000)
        s->bits_per_sample = 7;
    else
        s->bits_per_sample = 8;
    if ((options & G722_SAMPLE_RATE_8000))
        s->eight_k = TRUE;
    if ((options & G722_PACKED)  &&  s->bits_per_sample != 8)
        s->packed = TRUE;
    else
        s->packed = FALSE;
    s->band[0].det = 32;
    s->band[1].det = 8;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g722_decode_release(g722_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g722_decode_free(g722_decode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g722_decode(g722_decode_state_t *s, int16_t amp[], const uint8_t g722_data[], int len)
{
    int rlow;
    int ihigh;
    int16_t dlow;
    int16_t dhigh;
    int rhigh;
    int wd1;
    int wd2;
    int wd3;
    int code;
    int outlen;
    int j;

    outlen = 0;
    rhigh = 0;
    for (j = 0;  j < len;  )
    {
        if (s->packed)
        {
            /* Unpack the code bits */
            if (s->in_bits < s->bits_per_sample)
            {
                s->in_buffer |= (g722_data[j++] << s->in_bits);
                s->in_bits += 8;
            }
            code = s->in_buffer & ((1 << s->bits_per_sample) - 1);
            s->in_buffer >>= s->bits_per_sample;
            s->in_bits -= s->bits_per_sample;
        }
        else
        {
            code = g722_data[j++];
        }

        switch (s->bits_per_sample)
        {
        default:
        case 8:
            wd1 = code & 0x3F;
            ihigh = (code >> 6) & 0x03;
            wd2 = qm6[wd1];
            wd1 >>= 2;
            break;
        case 7:
            wd1 = code & 0x1F;
            ihigh = (code >> 5) & 0x03;
            wd2 = qm5[wd1];
            wd1 >>= 1;
            break;
        case 6:
            wd1 = code & 0x0F;
            ihigh = (code >> 4) & 0x03;
            wd2 = qm4[wd1];
            break;
        }
        /* Block 5L, LOW BAND INVQBL */
        wd2 = ((int32_t) s->band[0].det*(int32_t) wd2) >> 15;
        /* Block 5L, RECONS */
        /* Block 6L, LIMIT */
        rlow = saturate15(s->band[0].s + wd2);

        /* Block 2L, INVQAL */
        wd2 = qm4[wd1];
        dlow = (int16_t) (((int32_t) s->band[0].det*(int32_t) wd2) >> 15);

        /* Block 3L, LOGSCL */
        wd2 = rl42[wd1];
        wd1 = ((int32_t) s->band[0].nb*(int32_t) 127) >> 7;
        wd1 += wl[wd2];
        if (wd1 < 0)
            wd1 = 0;
        else if (wd1 > 18432)
            wd1 = 18432;
        s->band[0].nb = (int16_t) wd1;
            
        /* Block 3L, SCALEL */
        wd1 = (s->band[0].nb >> 6) & 31;
        wd2 = 8 - (s->band[0].nb >> 11);
        wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
        s->band[0].det = (int16_t) (wd3 << 2);

        block4(&s->band[0], dlow);
        
        if (!s->eight_k)
        {
            /* Block 2H, INVQAH */
            wd2 = qm2[ihigh];
            dhigh = (int16_t) (((int32_t) s->band[1].det*(int32_t) wd2) >> 15);
            /* Block 5H, RECONS */
            /* Block 6H, LIMIT */
            rhigh = saturate15(dhigh + s->band[1].s);

            /* Block 2H, INVQAH */
            wd2 = rh2[ihigh];
            wd1 = ((int32_t) s->band[1].nb*(int32_t) 127) >> 7;
            wd1 += wh[wd2];
            if (wd1 < 0)
                wd1 = 0;
            else if (wd1 > 22528)
                wd1 = 22528;
            s->band[1].nb = (int16_t) wd1;
            
            /* Block 3H, SCALEH */
            wd1 = (s->band[1].nb >> 6) & 31;
            wd2 = 10 - (s->band[1].nb >> 11);
            wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
            s->band[1].det = (int16_t) (wd3 << 2);

            block4(&s->band[1], dhigh);
        }

        if (s->itu_test_mode)
        {
            amp[outlen++] = (int16_t) (rlow << 1);
            amp[outlen++] = (int16_t) (rhigh << 1);
        }
        else
        {
            if (s->eight_k)
            {
                /* We shift by 1 to allow for the 15 bit input to the G.722 algorithm. */
                amp[outlen++] = (int16_t) (rlow << 1);
            }
            else
            {
                /* Apply the QMF to build the final signal */
                s->x[s->ptr] = (int16_t) (rlow + rhigh);
                s->y[s->ptr] = (int16_t) (rlow - rhigh);
                if (++s->ptr >= 12)
                    s->ptr = 0;
                /* We shift by 12 to allow for the QMF filters (DC gain = 4096), less 1
                   to allow for the 15 bit input to the G.722 algorithm. */
                amp[outlen++] = (int16_t) (vec_circular_dot_prodi16(s->y, qmf_coeffs_rev, 12, s->ptr) >> 11);
                amp[outlen++] = (int16_t) (vec_circular_dot_prodi16(s->x, qmf_coeffs_fwd, 12, s->ptr) >> 11);
            }
        }
    }
    return outlen;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(g722_encode_state_t *) g722_encode_init(g722_encode_state_t *s, int rate, int options)
{
    if (s == NULL)
    {
        if ((s = (g722_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    if (rate == 48000)
        s->bits_per_sample = 6;
    else if (rate == 56000)
        s->bits_per_sample = 7;
    else
        s->bits_per_sample = 8;
    if ((options & G722_SAMPLE_RATE_8000))
        s->eight_k = TRUE;
    if ((options & G722_PACKED)  &&  s->bits_per_sample != 8)
        s->packed = TRUE;
    else
        s->packed = FALSE;
    s->band[0].det = 32;
    s->band[1].det = 8;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g722_encode_release(g722_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g722_encode_free(g722_encode_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g722_encode(g722_encode_state_t *s, uint8_t g722_data[], const int16_t amp[], int len)
{
    int16_t dlow;
    int16_t dhigh;
    int el;
    int wd;
    int wd1;
    int ril;
    int wd2;
    int il4;
    int ih2;
    int wd3;
    int eh;
    int g722_bytes;
    int ihigh;
    int ilow;
    int code;
    /* Low and high band PCM from the QMF */
    int16_t xlow;
    int16_t xhigh;
    int32_t sumeven;
    int32_t sumodd;
    int mih;
    int i;
    int j;

    g722_bytes = 0;
    xhigh = 0;
    for (j = 0;  j < len;  )
    {
        if (s->itu_test_mode)
        {
            xlow =
            xhigh = amp[j++] >> 1;
        }
        else
        {
            if (s->eight_k)
            {
                /* We shift by 1 to allow for the 15 bit input to the G.722 algorithm. */
                xlow = amp[j++] >> 1;
            }
            else
            {
                /* Apply the transmit QMF */
                s->x[s->ptr] = amp[j++];
                s->y[s->ptr] = amp[j++];
                if (++s->ptr >= 12)
                    s->ptr = 0;
                sumodd = vec_circular_dot_prodi16(s->x, qmf_coeffs_fwd, 12, s->ptr);
                sumeven = vec_circular_dot_prodi16(s->y, qmf_coeffs_rev, 12, s->ptr);
                /* We shift by 12 to allow for the QMF filters (DC gain = 4096), plus 1
                   to allow for us summing two filters, plus 1 to allow for the 15 bit
                   input to the G.722 algorithm. */
                xlow = (int16_t) ((sumeven + sumodd) >> 14);
                xhigh = (int16_t) ((sumeven - sumodd) >> 14);
            }
        }
        /* Block 1L, SUBTRA */
        el = saturated_sub16(xlow, s->band[0].s);

        /* Block 1L, QUANTL */
        wd = (el >= 0)  ?  el  :  ~el;

        for (i = 1;  i < 30;  i++)
        {
            wd1 = ((int32_t) q6[i]*(int32_t) s->band[0].det) >> 12;
            if (wd < wd1)
                break;
        }
        ilow = (el < 0)  ?  iln[i]  :  ilp[i];

        /* Block 2L, INVQAL */
        ril = ilow >> 2;
        wd2 = qm4[ril];
        dlow = (int16_t) (((int32_t) s->band[0].det*(int32_t) wd2) >> 15);

        /* Block 3L, LOGSCL */
        il4 = rl42[ril];
        wd = ((int32_t) s->band[0].nb*(int32_t) 127) >> 7;
        s->band[0].nb = (int16_t) (wd + wl[il4]);
        if (s->band[0].nb < 0)
            s->band[0].nb = 0;
        else if (s->band[0].nb > 18432)
            s->band[0].nb = 18432;

        /* Block 3L, SCALEL */
        wd1 = (s->band[0].nb >> 6) & 31;
        wd2 = 8 - (s->band[0].nb >> 11);
        wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
        s->band[0].det = (int16_t) (wd3 << 2);

        block4(&s->band[0], dlow);
        
        if (s->eight_k)
        {
            /* Just leave the high bits as zero */
            code = (0xC0 | ilow) >> (8 - s->bits_per_sample);
        }
        else
        {
            /* Block 1H, SUBTRA */
            eh = saturated_sub16(xhigh, s->band[1].s);

            /* Block 1H, QUANTH */
            wd = (eh >= 0)  ?  eh  :  ~eh;
            wd1 = (564*s->band[1].det) >> 12;
            mih = (wd >= wd1)  ?  2  :  1;
            ihigh = (eh < 0)  ?  ihn[mih]  :  ihp[mih];

            /* Block 2H, INVQAH */
            wd2 = qm2[ihigh];
            dhigh = (int16_t) (((int32_t) s->band[1].det*(int32_t) wd2) >> 15);

            /* Block 3H, LOGSCH */
            ih2 = rh2[ihigh];
            wd = ((int32_t) s->band[1].nb*(int32_t) 127) >> 7;
            s->band[1].nb = (int16_t) (wd + wh[ih2]);
            if (s->band[1].nb < 0)
                s->band[1].nb = 0;
            else if (s->band[1].nb > 22528)
                s->band[1].nb = 22528;

            /* Block 3H, SCALEH */
            wd1 = (s->band[1].nb >> 6) & 31;
            wd2 = 10 - (s->band[1].nb >> 11);
            wd3 = (wd2 < 0)  ?  (ilb[wd1] << -wd2)  :  (ilb[wd1] >> wd2);
            s->band[1].det = (int16_t) (wd3 << 2);

            block4(&s->band[1], dhigh);
            code = ((ihigh << 6) | ilow) >> (8 - s->bits_per_sample);
        }

        if (s->packed)
        {
            /* Pack the code bits */
            s->out_buffer |= (code << s->out_bits);
            s->out_bits += s->bits_per_sample;
            if (s->out_bits >= 8)
            {
                g722_data[g722_bytes++] = (uint8_t) (s->out_buffer & 0xFF);
                s->out_bits -= 8;
                s->out_buffer >>= 8;
            }
        }
        else
        {
            g722_data[g722_bytes++] = (uint8_t) code;
        }
    }
    return g722_bytes;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
