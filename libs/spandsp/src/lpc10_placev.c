/*
 * SpanDSP - a series of DSP components for telephony
 *
 * lpc10_placev.c - LPC10 low bit rate speech codec.
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
 * This code is based on the U.S. Department of Defense reference
 * implementation of the LPC-10 2400 bps Voice Coder. They do not
 * exert copyright claims on their code, and it may be freely used.
 *
 * $Id: lpc10_placev.c,v 1.19 2009/01/28 03:41:27 steveu Exp $
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <memory.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/dc_restore.h"
#include "spandsp/lpc10.h"

#include "lpc10_encdecs.h"

#define subsc(x,y) (((x) << 1) + (y))

void lpc10_placea(int32_t *ipitch,
                  int32_t voibuf[3][2],
                  int32_t *obound,
                  int32_t af,
                  int32_t vwin[3][2],
                  int32_t awin[3][2],
                  int32_t ewin[3][2], 
                  int32_t lframe,
                  int32_t maxwin)
{
    int allv;
    int winv;
    int32_t i;
    int32_t j;
    int32_t k;
    int32_t l;
    int32_t hrange;
    int ephase;
    int32_t lrange;
    
    lrange = (af - 2)*lframe + 1;
    hrange = af*lframe;

    /* Place the analysis window based on the voicing window placement,
       onsets, tentative voicing decision, and pitch. */

    /* Case 1:  Sustained voiced speech
       If the five most recent voicing decisions are
       voiced, then the window is placed phase-synchronously with the
       previous window, as close to the present voicing window if possible.
       If onsets bound the voicing window, then preference is given to
       a phase-synchronous placement which does not overlap these onsets. */

    /* Case 2:  Voiced transition
       If at least one voicing decision in AF is voicied, and there are no
       onsets, then the window is placed as in case 1. */

    /* Case 3:  Unvoiced speech or onsets
       If both voicing decisions in AF are unvoiced, or there are onsets
       then the window is placed coincident with the voicing window. */

    /* Note:  During phase-synchronous placement of windows, the length
       is not altered from MAXWIN, since this would defeat the purpose
       of phase-synchronous placement. */

    /* Check for case 1 and case 2 */
    allv = voibuf[af - 2][1] == 1
           &&
           voibuf[af - 1][0] == 1
           &&
           voibuf[af - 1][1] == 1
           &&
           voibuf[af][0] == 1
           &&
           voibuf[af][1] == 1;
    winv = voibuf[af][0] == 1  ||  voibuf[af][1] == 1;
    if (allv  ||  (winv  &&  *obound == 0))
    {
        /* APHASE:  Phase synchronous window placement. */
        /* Get minimum lower index of the window. */
        i = (lrange + *ipitch - 1 - awin[af - 2][0]) / *ipitch;
        i *= *ipitch;
        i += awin[af - 2][0];
        /* l = the actual length of this frame's analysis window. */
        l = maxwin;
        /* Calculate the location where a perfectly centered window would start. */
        k = (vwin[af - 1][0] + vwin[af - 1][1] + 1 - l)/2;
        /* Choose the actual location to be the pitch multiple closest to this */
        awin[af - 1][0] = i + ((int) floorf((float) (k - i)/(float) *ipitch + 0.5f))*(*ipitch);
        awin[af - 1][1] = awin[af - 1][0] + l - 1;
        /* If there is an onset bounding the right of the voicing window and the
           analysis window overlaps that, then move the analysis window backward
           to avoid this onset. */
        if (*obound >= 2  &&  awin[af - 1][1] > vwin[af - 1][1])
        {
            awin[af - 1][0] -= *ipitch;
            awin[af - 1][1] -= *ipitch;
        }
        /* Similarly for the left of the voicing window. */
        if ((*obound == 1  ||  *obound == 3)  &&  awin[af - 1][0] < vwin[af - 1][0])
        {
            awin[af - 1][0] += *ipitch;
            awin[af - 1][1] += *ipitch;
        }
        /* If this placement puts the analysis window above HRANGE, then
           move it backward an integer number of pitch periods. */
        while (awin[af - 1][1] > hrange)
        {
            awin[af - 1][0] -= *ipitch;
            awin[af - 1][1] -= *ipitch;
        }
        /* Similarly if the placement puts the analysis window below LRANGE. */
        while (awin[af - 1][0] < lrange)
        {
            awin[af - 1][0] += *ipitch;
            awin[af - 1][1] += *ipitch;
        }
        /* Make energy window be phase-synchronous. */
        ephase = TRUE;
    }
    else
    {
        /* Case 3 */
        awin[af - 1][0] = vwin[af - 1][0];
        awin[af - 1][1] = vwin[af - 1][1];
        ephase = FALSE;
    }
    /* RMS is computed over an integer number of pitch periods in the analysis
       window.  When it is not placed phase-synchronously, it is placed as close
       as possible to onsets. */
    j = (awin[af - 1][1] - awin[af - 1][0] + 1) / *ipitch * *ipitch;
    if (j == 0  ||  !winv)
    {
        ewin[af - 1][0] = vwin[af - 1][0];
        ewin[af - 1][1] = vwin[af - 1][1];
    }
    else if (!ephase  &&  *obound == 2)
    {
        ewin[af - 1][0] = awin[af - 1][1] - j + 1;
        ewin[af - 1][1] = awin[af - 1][1];
    }
    else
    {
        ewin[af - 1][0] = awin[af - 1][0];
        ewin[af - 1][1] = awin[af - 1][0] + j - 1;
    }
}
/*- End of function --------------------------------------------------------*/

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
                  int32_t dvwinh)
{
    int32_t i1;
    int32_t i2;
    int crit;
    int32_t q;
    int32_t osptr1;
    int32_t hrange;
    int32_t lrange;
    int i;

    /* Voicing window placement */

    /*         __________________ __________________ ______________ */
    /*        |                  |                  |               */
    /*        |        1F        |        2F        |        3F ... */
    /*        |__________________|__________________|______________ */

    /*    Previous | */
    /*      Window | */
    /*  ...________| */

    /*             |                                | */
    /*      ------>| This window's placement range  |<------ */
    /*             |                                | */

    /* There are three cases.  Note these are different from those
       given in the LPC-10e phase 1 report. */

    /* 1.  If there are no onsets in this range, then the voicing window
       is centered in the pitch window.  If such a placement is not within
       the window's placement range, then the window is placed in the left-most
       portion of the placement range.  Its length is always MAXWIN. */

    /* 2.  If the first onset is in 2F and there is sufficient room to place
       the window immediately before this onset, then the window is placed
       there, and its length is set to the maximum possible under these
       constraints. */

    /* "Critical Region Exception":  If there is another onset in 2F
       such that a window can be placed between the two onsets, the
       window is placed there (ie, as in case 3). */

    /* 3.  Otherwise, the window is placed immediately after the onset.  The
       window's length is the longest length that can fit in the range under these
       constraints, except that the window may be shortened even further to avoid
       overlapping other onsets in the placement range.  In any case, the window's
       length is at least MINWIN. */

    /* Note that the values of MINWIN and LFRAME must be chosen such
       that case 2 = false implies case 3 = true.   This means that
       MINWIN <= LFRAME/2.  If this were not the case, then a fourth case
       would have to be added for when the window cannot fit either before
       or after the onset. */

    /* Note also that onsets which weren't in 2F last time may be in 1F this
       time, due to the filter delays in computing onsets.  The result is that
       occasionally a voicing window will overlap that onset.  The only way
       to circumvent this problem is to add more delay in processing input
       speech.  In the trade-off between delay and window-placement, window
       placement lost. */

    /* Compute the placement range */

    /* Computing MAX */
    i1 = vwin[af - 2][1] + 1;
    i2 = (af - 2)*lframe + 1;
    lrange = max(i1, i2);
    hrange = af*lframe;
    /* Compute OSPTR1, so the following code only looks at relevant onsets. */
    for (osptr1 = *osptr - 1;  osptr1 >= 1;  osptr1--)
    {
        if (osbuf[osptr1 - 1] <= hrange)
            break;
    }
    osptr1++;
    /* Check for case 1 first (fast case) */
    if (osptr1 <= 1  ||  osbuf[osptr1 - 2] < lrange)
    {
        /* Compute max */
        i1 = vwin[af - 2][1] + 1;
        vwin[af - 1][0] = max(i1, dvwinl);
        vwin[af - 1][1] = vwin[af - 1][0] + maxwin - 1;
        *obound = 0;
    }
    else
    {
        /* Search backward in OSBUF for first onset in range. */
        /* This code relies on the above check being performed first. */
        for (q = osptr1 - 1;  q >= 1;  q--)
        {
            if (osbuf[q - 1] < lrange)
                break;
        }
        q++;
        /* Check for case 2 (placement before onset): */
        /* Check for critical region exception: */
        crit = FALSE;
        for (i = q + 1;  i < osptr1;  i++)
        {
            if (osbuf[i - 1] - osbuf[q - 1] >= minwin)
            {
                crit = TRUE;
                break;
            }
        }
        /* Compute max */
        i1 = (af - 1)*lframe;
        i2 = lrange + minwin - 1;
        if (!crit  &&  osbuf[q - 1] > max(i1, i2))
        {
            vwin[af - 1][1] = osbuf[q - 1] - 1;
            /* Compute max */
            i2 = vwin[af - 1][1] - maxwin + 1;
            vwin[af - 1][0] = max(lrange, i2);
            *obound = 2;
        }
        else
        {
            /* Case 3 (placement after onset) */
            vwin[af - 1][0] = osbuf[q - 1];
            do
            {
                if (++q >= osptr1
                    ||
                    osbuf[q - 1] > vwin[af - 1][0] + maxwin)
                {
                    /* Compute min */
                    i1 = vwin[af - 1][0] + maxwin - 1;
                    vwin[af - 1][1] = min(i1, hrange);
                    *obound = 1;
                    return;
                }
            }
            while (osbuf[q - 1] < vwin[af - 1][0] + minwin);
            vwin[af - 1][1] = osbuf[q - 1] - 1;
            *obound = 3;
        }
    }
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
