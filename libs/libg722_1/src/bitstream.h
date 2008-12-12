/*
 * g722_1 - a library for the G.722.1 and Annex C codecs
 *
 * bitstream.h
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: bitstream.h,v 1.2 2008/10/17 13:18:21 steveu Exp $
 */

/*! \file */

#if !defined(_G722_1_BITSTREAM_H_)
#define _G722_1_BITSTREAM_H_

#if 0
/*! Bitstream handler state */
typedef struct
{
    /*! The bit stream. */
    uint32_t bitstream;
    /*! The residual bits in bitstream. */
    int residue;
} g722_1_bitstream_state_t;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Put a chunk of bits into the output buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream output buffer.
    \param value The value to be pushed into the output buffer.
    \param bits The number of bits of value to be pushed. 1 to 32 bits is valid. */
void g722_1_bitstream_put(g722_1_bitstream_state_t *s, uint8_t **c, uint32_t value, int bits);

/*! \brief Get a chunk of bits from the input buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream input buffer.
    \param bits The number of bits of value to be grabbed. 1 to 32 bits is valid.
    \return The value retrieved from the input buffer. */
uint32_t g722_1_bitstream_get(g722_1_bitstream_state_t *s, const uint8_t **c, int bits);

/*! \brief Flush any residual bit to the output buffer.
    \param s A pointer to the bitstream context.
    \param c A pointer to the bitstream output buffer. */
void g722_1_bitstream_flush(g722_1_bitstream_state_t *s, uint8_t **c);

/*! \brief Initialise a bitstream context.
    \param s A pointer to the bitstream context.
    \return A pointer to the bitstream context. */
g722_1_bitstream_state_t *g722_1_bitstream_init(g722_1_bitstream_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
