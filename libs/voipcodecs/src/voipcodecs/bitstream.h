/*
 * VoIPcodecs - a series of DSP components for telephony
 *
 * bitstream.h - Bitstream composition and decomposition routines.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: bitstream.h,v 1.8 2007/12/13 11:31:32 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_BITSTREAM_H_)
#define _SPANDSP_BITSTREAM_H_

/*! \page bitstream_page Bitstream composition and decomposition
\section bitstream_page_sec_1 What does it do?

\section bitstream_page_sec_2 How does it work?
*/

/*! Bitstream handler state */
typedef struct
{
    /*! The bit stream. */
    unsigned int bitstream;
    /*! The residual bits in bitstream. */
    unsigned int residue;
} bitstream_state_t;


#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Put a chunk of bits into the output buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream output buffer.
    \param value The value to be pushed into the output buffer.
    \param bits The number of bits of value to be pushed. 1 to 25 bit is valid. */
void bitstream_put(bitstream_state_t *s, uint8_t **c, unsigned int value, int bits);

void bitstream_put2(bitstream_state_t *s, uint8_t **c, unsigned int value, int bits);

/*! \brief Get a chunk of bits from the input buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream input buffer.
    \param bits The number of bits of value to be grabbed. 1 to 25 bit is valid.
    \return The value retrieved from the input buffer. */
unsigned int bitstream_get(bitstream_state_t *s, const uint8_t **c, int bits);

unsigned int bitstream_get2(bitstream_state_t *s, const uint8_t **c, int bits);

/*! \brief Flush any residual bit to the output buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream output buffer. */
void bitstream_flush(bitstream_state_t *s, uint8_t **c);

void bitstream_flush2(bitstream_state_t *s, uint8_t **c);

/*! \brief Initialise a bitstream context.
    \param s A pointer to the bitstream context.
    \return A pointer to the bitstream context. */
bitstream_state_t *bitstream_init(bitstream_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
