/*
 * SpanDSP - a series of DSP components for telephony
 *
 * lpc10_encdecs.h - LPC10 low bit rate speech codec.
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
 */

#define LPC10_ORDER     10

#if !defined(min)
#define min(a,b) ((a) <= (b) ? (a) : (b))
#endif
#if !defined(max)
#define max(a,b) ((a) >= (b) ? (a) : (b))
#endif

void lpc10_placea(int32_t *ipitch,
                  int32_t voibuf[4][2],
                  int32_t *obound,
                  int32_t af,
                  int32_t vwin[3][2],
                  int32_t awin[3][2],
                  int32_t ewin[3][2], 
                  int32_t lframe,
                  int32_t maxwin);

void lpc10_placev(int32_t *osbuf,
                  int32_t *osptr,
                  int32_t oslen, 
                  int32_t *obound,
                  int32_t vwin[3][2],
                  int32_t af,
                  int32_t lframe,
                  int32_t minwin,
                  int32_t maxwin,
                  int32_t dvwinl,
                  int32_t dvwinh);

void lpc10_voicing(lpc10_encode_state_t *st,
                   int32_t *vwin,
                   float *inbuf,
                   float *lpbuf,
                   const int32_t buflim[],
                   int32_t half,
                   float *minamd,
                   float *maxamd, 
	               int32_t *mintau,
                   float *ivrc,
                   int32_t *obound);

void lpc10_analyse(lpc10_encode_state_t *st, float *speech, int32_t *voice, int32_t *pitch, float *rms, float rc[]);

static __inline__ int32_t pow_ii(int32_t x, int32_t n)
{
    int32_t pow;
    uint32_t u;

    if (n <= 0)
    {
        if (n == 0  ||  x == 1)
            return 1;
        if (x != -1)
            return (x == 0)  ?  1/x  :  0;
        n = -n;
    }
    u = n;
    for (pow = 1;  ;  )
    {
        if ((u & 1))
            pow *= x;
        if ((u >>= 1) == 0)
            break;
        x *= x;
    }
    return pow;
}
/*- End of function --------------------------------------------------------*/

static __inline__ float r_sign(float a, float b)
{
    float x;

    x = fabsf(a);
    return (b >= 0.0f)  ?  x  :  -x;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
