/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_echo.c - An echo cancellor, suitable for electrical echos in GSTN modems
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2003, 2004 Steve Underwood
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

/* The FIR taps must be adapted as 32 bit values, to get the necessary finesse
   in the adaption process. However, they are applied as 16 bit values (bits 30-15
   of the 32 bit values) in the FIR. For the working 16 bit values, we need 4 sets.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/bit_operations.h"
#include "spandsp/dc_restore.h"
#include "spandsp/modem_echo.h"

#include "spandsp/private/modem_echo.h"

SPAN_DECLARE(void) modem_echo_can_free(modem_echo_can_state_t *ec)
{
    fir16_free(&ec->fir_state);
    free(ec->fir_taps32);
    free(ec->fir_taps16);
    free(ec);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(modem_echo_can_state_t *) modem_echo_can_init(int len)
{
    modem_echo_can_state_t *ec;

    if ((ec = (modem_echo_can_state_t *) malloc(sizeof(*ec))) == NULL)
        return NULL;
    memset(ec, 0, sizeof(*ec));
    ec->taps = len;
    ec->curr_pos = ec->taps - 1;
    if ((ec->fir_taps32 = (int32_t *) malloc(ec->taps*sizeof(int32_t))) == NULL)
    {
        free(ec);
        return NULL;
    }
    memset(ec->fir_taps32, 0, ec->taps*sizeof(int32_t));
    if ((ec->fir_taps16 = (int16_t *) malloc(ec->taps*sizeof(int16_t))) == NULL)
    {
        free(ec->fir_taps32);
        free(ec);
        return NULL;
    }
    memset(ec->fir_taps16, 0, ec->taps*sizeof(int16_t));
    if (fir16_create(&ec->fir_state, ec->fir_taps16, ec->taps) == NULL)
    {
        free(ec->fir_taps16);
        free(ec->fir_taps32);
        free(ec);
        return NULL;
    }
    return ec;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) modem_echo_can_flush(modem_echo_can_state_t *ec)
{
    ec->tx_power = 0;

    fir16_flush(&ec->fir_state);
    ec->fir_state.curr_pos = ec->taps - 1;
    memset(ec->fir_taps32, 0, ec->taps*sizeof(int32_t));
    memset(ec->fir_taps16, 0, ec->taps*sizeof(int16_t));
    ec->curr_pos = ec->taps - 1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) modem_echo_can_adaption_mode(modem_echo_can_state_t *ec, int adapt)
{
    ec->adapt = adapt;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int16_t) modem_echo_can_update(modem_echo_can_state_t *ec, int16_t tx, int16_t rx)
{
    int32_t echo_value;
    int clean_rx;
    int shift;
    int i;
    int offset1;
    int offset2;

    /* Evaluate the echo - i.e. apply the FIR filter */
    /* Assume the gain of the FIR does not exceed unity. Exceeding unity
       would seem like a rather poor thing for an echo cancellor to do :)
       This means we can compute the result with a total disregard for
       overflows. 16bits x 16bits -> 31bits, so no overflow can occur in
       any multiply. While accumulating we may overflow and underflow the
       32 bit scale often. However, if the gain does not exceed unity,
       everything should work itself out, and the final result will be
       OK, without any saturation logic. */
    /* Overflow is very much possible here, and we do nothing about it because
       of the compute costs */
    echo_value = fir16(&ec->fir_state, tx);

    /* And the answer is..... */
    clean_rx = rx - echo_value;
    //printf("%8d %8d %8d %8d\n", tx, rx, echo_value, clean_rx);
    if (ec->adapt)
    {
        /* Calculate short term power levels using very simple single pole IIRs */
        /* TODO: Is the nasty modulus approach the fastest, or would a real
                 tx*tx power calculation actually be faster? Using the squares
                 makes the numbers grow a lot! */
        ec->tx_power += ((tx*tx - ec->tx_power) >> 5);

        shift = 1;
        /* Update the FIR taps */
        offset2 = ec->curr_pos;
        offset1 = ec->taps - offset2;
        for (i = ec->taps - 1;  i >= offset1;  i--)
        {
            /* Leak to avoid the coefficients drifting beyond the ability of the
               adaption process to bring them back under control. */
            ec->fir_taps32[i] -= (ec->fir_taps32[i] >> 23);
            ec->fir_taps32[i] += (ec->fir_state.history[i - offset1]*clean_rx) >> shift;
            ec->fir_taps16[i] = (int16_t) (ec->fir_taps32[i] >> 15);
        }
        for (  ;  i >= 0;  i--)
        {
            ec->fir_taps32[i] -= (ec->fir_taps32[i] >> 23);
            ec->fir_taps32[i] += (ec->fir_state.history[i + offset2]*clean_rx) >> shift;
            ec->fir_taps16[i] = (int16_t) (ec->fir_taps32[i] >> 15);
        }
    }

    /* Roll around the rolling buffer */
    if (ec->curr_pos <= 0)
        ec->curr_pos = ec->taps;
    ec->curr_pos--;
    return (int16_t) clean_rx;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
