/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v42bis.h
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
 */

#if !defined(_SPANDSP_PRIVATE_V42BIS_H_)
#define _SPANDSP_PRIVATE_V42BIS_H_

/*!
    V.42bis dictionary node.
    Note that 0 is not a valid node to point to (0 is always a control code), so 0 is used
    as a "no such value" marker in this structure.
*/
typedef struct
{
    /*! \brief The value of the octet represented by the current dictionary node */
    uint8_t node_octet;
    /*! \brief The parent of this node */
    uint16_t parent;
    /*! \brief The first child of this node */
    uint16_t child;
    /*! \brief The next node at the same depth */
    uint16_t next;
} v42bis_dict_node_t;

/*!
    V.42bis compression or decompression. This defines the working state for a single instance
    of V.42bis compression or decompression.
*/
typedef struct
{
    /*! \brief Compression enabled. */
    int v42bis_parm_p0;
    /*! \brief Compression mode. */
    int compression_mode;
    /*! \brief Callback function to handle output data. */
    put_msg_func_t handler;
    /*! \brief An opaque pointer passed in calls to the data handler. */
    void *user_data;
    /*! \brief The maximum amount to be passed to the data handler. */
    int max_output_len;

    /*! \brief True if we are in transparent (i.e. uncompressable) mode */
    bool transparent;
    /*! \brief Next empty dictionary entry */
    uint16_t v42bis_parm_c1;
    /*! \brief Current codeword size */
    uint16_t v42bis_parm_c2;
    /*! \brief Threshold for codeword size change */
    uint16_t v42bis_parm_c3;
    /*! \brief The current update point in the dictionary */
    uint16_t update_at;
    /*! \brief The last entry matched in the dictionary */
    uint16_t last_matched;
    /*! \brief The last entry added to the dictionary */
    uint16_t last_added;
    /*! \brief Total number of codewords in the dictionary */
    int v42bis_parm_n2;
    /*! \brief Maximum permitted string length */
    int v42bis_parm_n7;
    /*! \brief The dictionary */
    v42bis_dict_node_t dict[V42BIS_MAX_CODEWORDS];

    /*! \brief The octet string in progress */
    uint8_t string[V42BIS_MAX_STRING_SIZE];
    /*! \brief The current length of the octet string in progress */
    int string_length;
    /*! \brief The amount of the octet string in progress which has already
        been flushed out of the buffer */
    int flushed_length;

    /*! \brief Compression performance metric */
    uint16_t compression_performance;

    /*! \brief Outgoing bit buffer (compression), or incoming bit buffer (decompression) */
    uint32_t bit_buffer;
    /*! \brief Outgoing bit count (compression), or incoming bit count (decompression) */
    int bit_count;

    /*! \brief The output composition buffer */
    uint8_t output_buf[V42BIS_MAX_OUTPUT_LENGTH];
    /*! \brief The length of the contents of the output composition buffer */
    int output_octet_count;

    /*! \brief The current value of the escape code */
    uint8_t escape_code;
    /*! \brief True if we just hit an escape code, and are waiting for the following octet */
    bool escaped;
} v42bis_comp_state_t;

/*!
    V.42bis compression/decompression descriptor. This defines the working state for a
    single instance of V.42bis compress/decompression.
*/
struct v42bis_state_s
{
    /*! \brief Compression state. */
    v42bis_comp_state_t compress;
    /*! \brief Decompression state. */
    v42bis_comp_state_t decompress;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
