/*
 * SpanDSP - a series of DSP components for telephony
 *
 * async.c - Asynchronous serial bit stream encoding and decoding
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
 *
 * $Id: async.c,v 1.11 2008/05/13 13:17:21 steveu Exp $
 */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/async.h"

async_rx_state_t *async_rx_init(async_rx_state_t *s,
                                int data_bits,
                                int parity,
                                int stop_bits,
                                int use_v14,
                                put_byte_func_t put_byte,
                                void *user_data)
{
    if (s == NULL)
    {
        if ((s = (async_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    s->data_bits = data_bits;
    s->parity = parity;
    s->stop_bits = stop_bits;
    s->use_v14 = use_v14;

    s->put_byte = put_byte;
    s->user_data = user_data;

    s->byte_in_progress = 0;
    s->bitpos = 0;
    s->parity_bit = 0;

    s->parity_errors = 0;
    s->framing_errors = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

void async_rx_put_bit(void *user_data, int bit)
{
    async_rx_state_t *s;

    s = (async_rx_state_t *) user_data;
    if (bit < 0)
    {
        /* Special conditions */
        switch (bit)
        {
        case PUTBIT_CARRIER_UP:
        case PUTBIT_CARRIER_DOWN:
        case PUTBIT_TRAINING_IN_PROGRESS:
        case PUTBIT_TRAINING_SUCCEEDED:
        case PUTBIT_TRAINING_FAILED:
        case PUTBIT_END_OF_DATA:
            s->put_byte(s->user_data, bit);
            s->bitpos = 0;
            s->byte_in_progress = 0;
            break;
        default:
            //printf("Eh!\n");
            break;
        }
        return;
    }
    if (s->bitpos == 0)
    {
        /* Search for the start bit */
        s->bitpos += (bit ^ 1);
        s->parity_bit = 0;
        s->byte_in_progress = 0;
    }
    else if (s->bitpos <= s->data_bits)
    {
        s->byte_in_progress = (s->byte_in_progress >> 1) | (bit << 7);
        s->parity_bit ^= bit;
        s->bitpos++;
    }
    else if (s->parity  &&  s->bitpos == s->data_bits + 1)
    {
        if (s->parity == ASYNC_PARITY_ODD)
            s->parity_bit ^= 1;

        if (s->parity_bit != bit)
            s->parity_errors++;
        s->bitpos++;
    }
    else
    {
        /* Stop bit */
        if (bit == 1)
        {
            /* Align the received value */
            if (s->data_bits < 8)
                s->byte_in_progress >>= (8 - s->data_bits);
            s->put_byte(s->user_data, s->byte_in_progress);
            s->bitpos = 0;
        }
        else if (s->use_v14)
        {
            /* This is actually the start bit for the next character, and
               the stop bit has been dropped from the stream. This is the
               rate adaption specified in V.14 */
            /* Align the received value */
            if (s->data_bits < 8)
                s->byte_in_progress >>= (8 - s->data_bits);
            s->put_byte(s->user_data, s->byte_in_progress);
            s->bitpos = 1;
            s->parity_bit = 0;
            s->byte_in_progress = 0;
        }
        else
        {
            s->framing_errors++;
            s->bitpos = 0;
        }
    }
}
/*- End of function --------------------------------------------------------*/

async_tx_state_t *async_tx_init(async_tx_state_t *s,
                                int data_bits,
                                int parity,
                                int stop_bits,
                                int use_v14,
                                get_byte_func_t get_byte,
                                void *user_data)
{
    if (s == NULL)
    {
        if ((s = (async_tx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    /* We have a use_v14 parameter for completeness, but right now V.14 only
       applies to the receive side. We are unlikely to have an application where
       flow control does not exist, so V.14 stuffing is not needed. */
    s->data_bits = data_bits;
    s->parity = parity;
    s->stop_bits = stop_bits;
    if (parity != ASYNC_PARITY_NONE)
        s->stop_bits++;
        
    s->get_byte = get_byte;
    s->user_data = user_data;

    s->byte_in_progress = 0;
    s->bitpos = 0;
    s->parity_bit = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

int async_tx_get_bit(void *user_data)
{
    async_tx_state_t *s;
    int bit;
    
    s = (async_tx_state_t *) user_data;
    if (s->bitpos == 0)
    {
        if ((s->byte_in_progress = s->get_byte(s->user_data)) < 0)
        {
            /* No more data */
            bit = PUTBIT_END_OF_DATA;
        }
        else
        {
            /* Start bit */
            bit = 0;
            s->parity_bit = 0;
            s->bitpos++;
        }
    }
    else if (s->bitpos <= s->data_bits)
    {
        bit = s->byte_in_progress & 1;
        s->byte_in_progress >>= 1;
        s->parity_bit ^= bit;
        s->bitpos++;
    }
    else if (s->parity  &&  s->bitpos == s->data_bits + 1)
    {
        if (s->parity == ASYNC_PARITY_ODD)
            s->parity_bit ^= 1;
        bit = s->parity_bit;
        s->bitpos++;
    }
    else
    {
        /* Stop bit(s) */
        bit = 1;
        if (++s->bitpos > s->data_bits + s->stop_bits)
            s->bitpos = 0;
    }
    return bit;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
