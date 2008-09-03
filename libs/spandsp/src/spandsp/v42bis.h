/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42bis.h
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
 * $Id: v42bis.h,v 1.22 2008/04/17 14:27:01 steveu Exp $
 */

/*! \page v42bis_page V.42bis modem data compression
\section v42bis_page_sec_1 What does it do?
The v.42bis specification defines a data compression scheme, to work in
conjunction with the error correction scheme defined in V.42.

\section v42bis_page_sec_2 How does it work?
*/

#if !defined(_SPANDSP_V42BIS_H_)
#define _SPANDSP_V42BIS_H_

#define V42BIS_MAX_BITS         12
#define V42BIS_MAX_CODEWORDS    4096    /* 2^V42BIS_MAX_BITS */
#define V42BIS_TABLE_SIZE       5021    /* This should be a prime >(2^V42BIS_MAX_BITS) */
#define V42BIS_MAX_STRING_SIZE  250

enum
{
    V42BIS_P0_NEITHER_DIRECTION = 0,
    V42BIS_P0_INITIATOR_RESPONDER,
    V42BIS_P0_RESPONDER_INITIATOR,
    V42BIS_P0_BOTH_DIRECTIONS
};

enum
{
    V42BIS_COMPRESSION_MODE_DYNAMIC = 0,
    V42BIS_COMPRESSION_MODE_ALWAYS,
    V42BIS_COMPRESSION_MODE_NEVER
};

typedef void (*v42bis_frame_handler_t)(void *user_data, const uint8_t *pkt, int len);
typedef void (*v42bis_data_handler_t)(void *user_data, const uint8_t *buf, int len);

/*!
    V.42bis dictionary node.
*/
typedef struct
{
    /*! \brief The prior code for each defined code. */
    uint16_t parent_code;
    /*! \brief The number of leaf nodes this node has */
    int16_t leaves;
    /*! \brief This leaf octet for each defined code. */
    uint8_t node_octet;
    /*! \brief Bit map of the children which exist */
    uint32_t children[8];
} v42bis_dict_node_t;

/*!
    V.42bis compression. This defines the working state for a single instance
    of V.42bis compression.
*/
typedef struct
{
    /*! \brief Compression mode. */
    int compression_mode;
    /*! \brief Callback function to handle received frames. */
    v42bis_frame_handler_t handler;
    /*! \brief An opaque pointer passed in calls to frame_handler. */
    void *user_data;
    /*! \brief The maximum frame length allowed */
    int max_len;

    uint32_t string_code;
    uint32_t latest_code;
    int string_length;
    uint32_t output_bit_buffer;
    int output_bit_count;
    int output_octet_count;
    uint8_t output_buf[1024];
    v42bis_dict_node_t dict[V42BIS_MAX_CODEWORDS];
    /*! \brief TRUE if we are in transparent (i.e. uncompressable) mode */
    int transparent;
    int change_transparency;
    /*! \brief IIR filter state, used in assessing compressibility. */
    int compressibility_filter;
    int compressibility_persistence;
    
    /*! \brief Next empty dictionary entry */
    uint32_t v42bis_parm_c1;
    /*! \brief Current codeword size */
    int v42bis_parm_c2;
    /*! \brief Threshold for codeword size change */
    uint32_t v42bis_parm_c3;

    /*! \brief Mark that this is the first octet/code to be processed */
    int first;
    uint8_t escape_code;
} v42bis_compress_state_t;

/*!
    V.42bis decompression. This defines the working state for a single instance
    of V.42bis decompression.
*/
typedef struct
{
    /*! \brief Callback function to handle decompressed data. */
    v42bis_data_handler_t handler;
    /*! \brief An opaque pointer passed in calls to data_handler. */
    void *user_data;
    /*! \brief The maximum decompressed data block length allowed */
    int max_len;

    uint32_t old_code;
    uint32_t last_old_code;
    uint32_t input_bit_buffer;
    int input_bit_count;
    int octet;
    int last_length;
    int output_octet_count;
    uint8_t output_buf[1024];
    v42bis_dict_node_t dict[V42BIS_MAX_CODEWORDS];
    /*! \brief TRUE if we are in transparent (i.e. uncompressable) mode */
    int transparent;

    int last_extra_octet;

    /*! \brief Next empty dictionary entry */
    uint32_t v42bis_parm_c1;
    /*! \brief Current codeword size */
    int v42bis_parm_c2;
    /*! \brief Threshold for codeword size change */
    uint32_t v42bis_parm_c3;
        
    /*! \brief Mark that this is the first octet/code to be processed */
    int first;
    uint8_t escape_code;
    int escaped;
} v42bis_decompress_state_t;

/*!
    V.42bis compression/decompression descriptor. This defines the working state for a
    single instance of V.42bis compress/decompression.
*/
typedef struct
{
    /*! \brief V.42bis data compression directions. */
    int v42bis_parm_p0;

    /*! \brief Compression state. */
    v42bis_compress_state_t compress;
    /*! \brief Decompression state. */
    v42bis_decompress_state_t decompress;
    
    /*! \brief Maximum codeword size (bits) */
    int v42bis_parm_n1;
    /*! \brief Total number of codewords */
    uint32_t v42bis_parm_n2;
    /*! \brief Maximum string length */
    int v42bis_parm_n7;
} v42bis_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Compress a block of octets.
    \param s The V.42bis context.
    \param buf The data to be compressed.
    \param len The length of the data buffer.
    \return 0 */
int v42bis_compress(v42bis_state_t *s, const uint8_t *buf, int len);

/*! Flush out any data remaining in a compression buffer.
    \param s The V.42bis context.
    \return 0 */
int v42bis_compress_flush(v42bis_state_t *s);

/*! Decompress a block of octets.
    \param s The V.42bis context.
    \param buf The data to be decompressed.
    \param len The length of the data buffer.
    \return 0 */
int v42bis_decompress(v42bis_state_t *s, const uint8_t *buf, int len);
    
/*! Flush out any data remaining in the decompression buffer.
    \param s The V.42bis context.
    \return 0 */
int v42bis_decompress_flush(v42bis_state_t *s);

/*! Initialise a V.42bis context.
    \param s The V.42bis context.
    \param negotiated_p0 The negotiated P0 parameter, from the V.42bis spec.
    \param negotiated_p1 The negotiated P1 parameter, from the V.42bis spec.
    \param negotiated_p2 The negotiated P2 parameter, from the V.42bis spec.
    \param frame_handler .
    \param frame_user_data .
    \param max_frame_len The maximum length that should be passed to the frame handler.
    \param data_handler .
    \param data_user_data .
    \param max_data_len The maximum length that should be passed to the data handler.
    \return The V.42bis context. */
v42bis_state_t *v42bis_init(v42bis_state_t *s,
                            int negotiated_p0,
                            int negotiated_p1,
                            int negotiated_p2,
                            v42bis_frame_handler_t frame_handler,
                            void *frame_user_data,
                            int max_frame_len,
                            v42bis_data_handler_t data_handler,
                            void *data_user_data,
                            int max_data_len);

/*! Set the compression mode.
    \param s The V.42bis context.
    \param mode One of the V.42bis compression modes -
            V42BIS_COMPRESSION_MODE_DYNAMIC,
            V42BIS_COMPRESSION_MODE_ALWAYS,
            V42BIS_COMPRESSION_MODE_NEVER */
void v42bis_compression_control(v42bis_state_t *s, int mode);

/*! Release a V.42bis context.
    \param s The V.42bis context.
    \return 0 if OK */
int v42bis_release(v42bis_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
