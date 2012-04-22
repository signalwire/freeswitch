/*
 * broadvoice - a library for the BroadVoice 16 and 32 codecs
 *
 * bitstream.c - 
 *
 * Copyright 2009 by Steve Underwood <steveu@coppice.org>
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
 * $Id: bitstream.h,v 1.1.1.1 2009/11/19 12:10:48 steveu Exp $
 */

#if !defined(_BITSTREAM_H_)
#define _BITSTREAM_H_

/*! Bitstream handler state */
typedef struct
{
    /*! The bit stream. */
    uint32_t bitstream;
    /*! The residual bits in bitstream. */
    int residue;
} bitstream_state_t;

void bitstream_put(bitstream_state_t *s, uint8_t **c, uint32_t value, int bits);

uint32_t bitstream_get(bitstream_state_t *s, const uint8_t **c, int bits);

void bitstream_flush(bitstream_state_t *s, uint8_t **c);

bitstream_state_t *bitstream_init(bitstream_state_t *s);

#endif
/*- End of file ------------------------------------------------------------*/
