/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42bis.c
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
 * $Id: v42bis.c,v 1.37 2009/02/10 13:06:47 steveu Exp $
 */

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. 
   Currently it performs the core compression and decompression functions OK.
   However, a number of the bells and whistles in V.42bis are incomplete. */

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/v42bis.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v42bis.h"

/* Fixed parameters from the spec. */
#define V42BIS_N3               8   /* Character size (bits) */
#define V42BIS_N4               256 /* Number of characters in the alphabet */
#define V42BIS_N5               (V42BIS_N4 + V42BIS_N6)  /* Index number of first dictionary entry used to store a string */
#define V42BIS_N6               3   /* Number of control codewords */

/* Control code words in compressed mode */
enum
{
    V42BIS_ETM = 0,         /* Enter transparent mode */
    V42BIS_FLUSH = 1,       /* Flush data */
    V42BIS_STEPUP = 2       /* Step up codeword size */
};

/* Command codes in transparent mode */
enum
{
    V42BIS_ECM = 0,         /* Enter compression mode */
    V42BIS_EID = 1,         /* Escape character in data */
    V42BIS_RESET = 2        /* Force reinitialisation */
};

static __inline__ void push_compressed_raw_octet(v42bis_compress_state_t *ss, int octet)
{
    ss->output_buf[ss->output_octet_count++] = (uint8_t) octet;
    if (ss->output_octet_count >= ss->max_len)
    {
        ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
        ss->output_octet_count = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void push_compressed_code(v42bis_compress_state_t *ss, int code)
{
    ss->output_bit_buffer |= code << (32 - ss->v42bis_parm_c2 - ss->output_bit_count);
    ss->output_bit_count += ss->v42bis_parm_c2;
    while (ss->output_bit_count >= 8)
    {
        push_compressed_raw_octet(ss, ss->output_bit_buffer >> 24);
        ss->output_bit_buffer <<= 8;
        ss->output_bit_count -= 8;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void push_compressed_octet(v42bis_compress_state_t *ss, int code)
{
    ss->output_bit_buffer |= code << (32 - 8 - ss->output_bit_count);
    ss->output_bit_count += 8;
    while (ss->output_bit_count >= 8)
    {
        push_compressed_raw_octet(ss, ss->output_bit_buffer >> 24);
        ss->output_bit_buffer <<= 8;
        ss->output_bit_count -= 8;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_compress(v42bis_state_t *s, const uint8_t *buf, int len)
{
    int ptr;
    int i;
    uint32_t octet;
    uint32_t code;
    v42bis_compress_state_t *ss;

    ss = &s->compress;
    if ((s->v42bis_parm_p0 & 2) == 0)
    {
        /* Compression is off - just push the incoming data out */
        for (i = 0;  i < len - ss->max_len;  i += ss->max_len)
            ss->handler(ss->user_data, buf + i, ss->max_len);
        if (i < len)
            ss->handler(ss->user_data, buf + i, len - i);
        return 0;
    }
    ptr = 0;
    if (ss->first  &&  len > 0)
    {
        octet = buf[ptr++];
        ss->string_code = octet + V42BIS_N6;
        if (ss->transparent)
            push_compressed_octet(ss, octet);
        ss->first = FALSE;
    }
    while (ptr < len)
    {
        octet = buf[ptr++];
        if ((ss->dict[ss->string_code].children[octet >> 5] & (1 << (octet & 0x1F))))
        {
            /* The leaf exists. Now find it in the table. */
            /* TODO: This is a brute force scan for a match. We need something better. */
            for (code = 0;  code < ss->v42bis_parm_c3;  code++)
            {
                if (ss->dict[code].parent_code == ss->string_code  &&  ss->dict[code].node_octet == octet)
                    break;
            }
        }
        else
        {
            /* The leaf does not exist. */
            code = s->v42bis_parm_n2;
        }
        /* 6.3(b) If the string matches a dictionary entry, and the entry is not that entry
                  created by the last invocation of the string matching procedure, then the
                  next character shall be read and appended to the string and this step
                  repeated. */
        if (code < ss->v42bis_parm_c3  &&  code != ss->latest_code)
        {
            /* The string was found */
            ss->string_code = code;
            ss->string_length++;
        }
        else
        {
            /* The string is not in the table. */
            if (!ss->transparent)
            {
                /* 7.4 Encoding - we now have the longest matchable string, and will need to output the code for it. */
                while (ss->v42bis_parm_c1 >= ss->v42bis_parm_c3  &&  ss->v42bis_parm_c3 <= s->v42bis_parm_n2)
                {
                    /* We need to increase the codeword size */
                    /* 7.4(a) */
                    push_compressed_code(ss, V42BIS_STEPUP);
                    /* 7.4(b) */
                    ss->v42bis_parm_c2++;
                    /* 7.4(c) */
                    ss->v42bis_parm_c3 <<= 1;
                    /* 7.4(d) this might need to be repeated, so we loop */
                }
                /* 7.5 Transfer - output the last state of the string */
                push_compressed_code(ss, ss->string_code);
            }
            /* 7.6    Dictionary updating */
            /* 6.4    Add the string to the dictionary */
            /* 6.4(b) The string is not in the table. */
            if (code != ss->latest_code  &&  ss->string_length < s->v42bis_parm_n7)
            {
                ss->latest_code = ss->v42bis_parm_c1;
                /* 6.4(a) The length of the string is in range for adding to the dictionary */
                /* If the last code was a leaf, it no longer is */
                ss->dict[ss->string_code].leaves++;
                ss->dict[ss->string_code].children[octet >> 5] |= (1 << (octet & 0x1F));
                /* The new one is definitely a leaf */
                ss->dict[ss->v42bis_parm_c1].parent_code = (uint16_t) ss->string_code;
                ss->dict[ss->v42bis_parm_c1].leaves = 0;
                ss->dict[ss->v42bis_parm_c1].node_octet = (uint8_t) octet;
                /* 7.7    Node recovery */
                /* 6.5    Recovering a dictionary entry to use next */
                for (;;)
                {
                    /* 6.5(a) and (b) */
                    if ((int) (++ss->v42bis_parm_c1) >= s->v42bis_parm_n2)
                        ss->v42bis_parm_c1 = V42BIS_N5;
                    /* 6.5(c) We need to reuse a leaf node */
                    if (ss->dict[ss->v42bis_parm_c1].leaves)
                        continue;
                    if (ss->dict[ss->v42bis_parm_c1].parent_code == 0xFFFF)
                        break;
                    /* 6.5(d) Detach the leaf node from its parent, and re-use it */
                    /* Possibly make the parent a leaf node again */
                    ss->dict[ss->dict[ss->v42bis_parm_c1].parent_code].leaves--;
                    ss->dict[ss->dict[ss->v42bis_parm_c1].parent_code].children[ss->dict[ss->v42bis_parm_c1].node_octet >> 5] &= ~(1 << (ss->dict[ss->v42bis_parm_c1].node_octet & 0x1F));
                    ss->dict[ss->v42bis_parm_c1].parent_code = 0xFFFF;
                    break;
                }
            }
            else
            {
                ss->latest_code = 0xFFFFFFFF;
            }
            /* 7.8 Data compressibility test */
            /* Filter on the balance of what went into the compressor, and what came out */
            ss->compressibility_filter += ((((8*ss->string_length - ss->v42bis_parm_c2) << 20) - ss->compressibility_filter) >> 10);
            if (ss->compression_mode == V42BIS_COMPRESSION_MODE_DYNAMIC)
            {
                /* Work out if it is appropriate to change between transparent and
                   compressed mode. */
                if (ss->transparent)
                {
                    if (ss->compressibility_filter > 0)
                    {
                        if (++ss->compressibility_persistence > 1000)
                        {
                            /* Schedule a switch to compressed mode */
                            ss->change_transparency = -1;
                            ss->compressibility_persistence = 0;
                        }
                    }
                    else
                    {
                        ss->compressibility_persistence = 0;
                    }
                }
                else
                {
                    if (ss->compressibility_filter < 0)
                    {
                        if (++ss->compressibility_persistence > 1000)
                        {
                            /* Schedule a switch to transparent mode */
                            ss->change_transparency = 1;
                            ss->compressibility_persistence = 0;
                        }
                    }
                    else
                    {
                        ss->compressibility_persistence = 0;
                    }
                }
            }
            if (ss->change_transparency)
            {
                if (ss->change_transparency < 0)
                {
                    if (ss->transparent)
                    {
                        printf("Going compressed\n");
                        /* 7.8.1 Transition to compressed mode */
                        /* Switch out of transparent now, between codes. We need to send the octet which did not
                        match, just before switching. */
                        if (octet == ss->escape_code)
                        {
                            push_compressed_octet(ss, ss->escape_code++);
                            push_compressed_octet(ss, V42BIS_EID);
                        }
                        else
                        {
                            push_compressed_octet(ss, octet);
                        }
                        push_compressed_octet(ss, ss->escape_code++);
                        push_compressed_octet(ss, V42BIS_ECM);
                        ss->transparent = FALSE;
                    }
                }
                else
                {
                    if (!ss->transparent)
                    {
                        printf("Going transparent\n");
                        /* 7.8.2 Transition to transparent mode */
                        /* Switch into transparent now, between codes, and the unmatched octet should
                           go out in transparent mode, just below */
                        push_compressed_code(ss, V42BIS_ETM);
                        ss->transparent = TRUE;
                    }
                }
                ss->change_transparency = 0;
            }
            /* 7.8.3 Reset function - TODO */
            ss->string_code = octet + V42BIS_N6;
            ss->string_length = 1;
        }
        if (ss->transparent)
        {
            if (octet == ss->escape_code)
            {
                push_compressed_octet(ss, ss->escape_code++);
                push_compressed_octet(ss, V42BIS_EID);
            }
            else
            {
                push_compressed_octet(ss, octet);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_compress_flush(v42bis_state_t *s)
{
    v42bis_compress_state_t *ss;

    ss = &s->compress;
    if (!ss->transparent)
    {
        /* Output the last state of the string */
        push_compressed_code(ss, ss->string_code);
        /* TODO: We use a positive FLUSH at all times. It is really needed, if the
           previous step resulted in no leftover bits. */
        push_compressed_code(ss, V42BIS_FLUSH);
    }
    while (ss->output_bit_count > 0)
    {
        push_compressed_raw_octet(ss, ss->output_bit_buffer >> 24);
        ss->output_bit_buffer <<= 8;
        ss->output_bit_count -= 8;
    }
    /* Now push out anything remaining. */
    if (ss->output_octet_count > 0)
    {
        ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
        ss->output_octet_count = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if 0
SPAN_DECLARE(int) v42bis_compress_dump(v42bis_state_t *s)
{
    int i;
    
    for (i = 0;  i < V42BIS_MAX_CODEWORDS;  i++)
    {
        if (s->compress.dict[i].parent_code != 0xFFFF)
        {
            printf("Entry %4x, prior %4x, leaves %d, octet %2x\n", i, s->compress.dict[i].parent_code, s->compress.dict[i].leaves, s->compress.dict[i].node_octet);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(int) v42bis_decompress(v42bis_state_t *s, const uint8_t *buf, int len)
{
    int ptr;
    int i;
    int this_length;
    uint8_t *string;
    uint32_t code;
    uint32_t new_code;
    int code_len;
    v42bis_decompress_state_t *ss;
    uint8_t decode_buf[V42BIS_MAX_STRING_SIZE];

    ss = &s->decompress;
    if ((s->v42bis_parm_p0 & 1) == 0)
    {
        /* Compression is off - just push the incoming data out */
        for (i = 0;  i < len - ss->max_len;  i += ss->max_len)
            ss->handler(ss->user_data, buf + i, ss->max_len);
        if (i < len)
            ss->handler(ss->user_data, buf + i, len - i);
        return 0;
    }
    ptr = 0;
    code_len = (ss->transparent)  ?  8  :  ss->v42bis_parm_c2;
    for (;;)
    {
        /* Fill up the bit buffer. */
        while (ss->input_bit_count < 32 - 8  &&  ptr < len)
        {
            ss->input_bit_count += 8;
            ss->input_bit_buffer |= (uint32_t) buf[ptr++] << (32 - ss->input_bit_count);
        }
        if (ss->input_bit_count < code_len)
            break;
        new_code = ss->input_bit_buffer >> (32 - code_len);
        ss->input_bit_count -= code_len;
        ss->input_bit_buffer <<= code_len;
        if (ss->transparent)
        {
            code = new_code;
            if (ss->escaped)
            {
                ss->escaped = FALSE;
                if (code == V42BIS_ECM)
                {
                    printf("Hit V42BIS_ECM\n");
                    ss->transparent = FALSE;
                    code_len = ss->v42bis_parm_c2;
                }
                else if (code == V42BIS_EID)
                {
                    printf("Hit V42BIS_EID\n");
                    ss->output_buf[ss->output_octet_count++] = ss->escape_code - 1;
                    if (ss->output_octet_count >= ss->max_len - s->v42bis_parm_n7)
                    {
                        ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
                        ss->output_octet_count = 0;
                    }
                }
                else if (code == V42BIS_RESET)
                {
                    printf("Hit V42BIS_RESET\n");
                }
                else
                {
                    printf("Hit V42BIS_???? - %" PRIu32 "\n", code);
                }
            }
            else if (code == ss->escape_code)
            {
                ss->escape_code++;
                ss->escaped = TRUE;
            }
            else
            {
                ss->output_buf[ss->output_octet_count++] = (uint8_t) code;
                if (ss->output_octet_count >= ss->max_len - s->v42bis_parm_n7)
                {
                    ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
                    ss->output_octet_count = 0;
                }
            }
        }
        else
        {
            if (new_code < V42BIS_N6)
            {
                /* We have a control code. */
                switch (new_code)
                {
                case V42BIS_ETM:
                    printf("Hit V42BIS_ETM\n");
                    ss->transparent = TRUE;
                    code_len = 8;
                    break;
                case V42BIS_FLUSH:
                    printf("Hit V42BIS_FLUSH\n");
                    v42bis_decompress_flush(s);
                    break;
                case V42BIS_STEPUP:
                    /* We need to increase the codeword size */
                    printf("Hit V42BIS_STEPUP\n");
                    if (ss->v42bis_parm_c3 >= s->v42bis_parm_n2)
                    {
                        /* Invalid condition */
                        return -1;
                    }
                    code_len = ++ss->v42bis_parm_c2;
                    ss->v42bis_parm_c3 <<= 1;
                    break;
                }
                continue;
            }
            if (ss->first)
            {
                ss->first = FALSE;
                ss->octet = new_code - V42BIS_N6;
                ss->output_buf[0] = (uint8_t) ss->octet;
                ss->output_octet_count = 1;
                if (ss->output_octet_count >= ss->max_len - s->v42bis_parm_n7)
                {
                    ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
                    ss->output_octet_count = 0;
                }
                ss->old_code = new_code;
                continue;
            }
            /* Start at the end of the buffer, and decode backwards */
            string = &decode_buf[V42BIS_MAX_STRING_SIZE - 1];
            /* Check the received code is valid. It can't be too big, as we pulled only the expected number
            of bits from the input stream. It could, however, be unknown. */
            if (ss->dict[new_code].parent_code == 0xFFFF)
                return -1;
            /* Otherwise we do a straight decode of the new code. */
            code = new_code;
            /* Trace back through the octets which form the string, and output them. */
            while (code >= V42BIS_N5)
            {
if (code > 4095) {printf("Code is 0x%" PRIu32 "\n", code); exit(2);}
                *string-- = ss->dict[code].node_octet;
                code = ss->dict[code].parent_code;
            }
            *string = (uint8_t) (code - V42BIS_N6);
            ss->octet = code - V42BIS_N6;
            /* Output the decoded string. */
            this_length = V42BIS_MAX_STRING_SIZE - (int) (string - decode_buf);
            memcpy(ss->output_buf + ss->output_octet_count, string, this_length);
            ss->output_octet_count += this_length;
            if (ss->output_octet_count >= ss->max_len - s->v42bis_parm_n7)
            {
                ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
                ss->output_octet_count = 0;
            }
            /* 6.4 Add the string to the dictionary */
            if (ss->last_length < s->v42bis_parm_n7)
            {
                /* 6.4(a) The string does not exceed N7 in length */
                if (ss->last_old_code != ss->old_code
                    ||
                    ss->last_extra_octet != *string)
                {
                    /* 6.4(b) The string is not in the table. */
                    ss->dict[ss->old_code].leaves++;
                    /* The new one is definitely a leaf */
                    ss->dict[ss->v42bis_parm_c1].parent_code = (uint16_t) ss->old_code;
                    ss->dict[ss->v42bis_parm_c1].leaves = 0;
                    ss->dict[ss->v42bis_parm_c1].node_octet = (uint8_t) ss->octet;
                    /* 6.5 Recovering a dictionary entry to use next */
                    for (;;)
                    {
                        /* 6.5(a) and (b) */
                        if (++ss->v42bis_parm_c1 >= s->v42bis_parm_n2)
                            ss->v42bis_parm_c1 = V42BIS_N5;
                        /* 6.5(c) We need to reuse a leaf node */
                        if (ss->dict[ss->v42bis_parm_c1].leaves)
                            continue;
                        /* 6.5(d) This is a leaf node, so re-use it */
                        /* Possibly make the parent a leaf node again */
                        if (ss->dict[ss->v42bis_parm_c1].parent_code != 0xFFFF)
                            ss->dict[ss->dict[ss->v42bis_parm_c1].parent_code].leaves--;
                        ss->dict[ss->v42bis_parm_c1].parent_code = 0xFFFF;
                        break;
                    }
                }
            }
            /* Record the addition to the dictionary, so we can check for repeat attempts
               at the next code - see II.4.3 */
            ss->last_old_code = ss->old_code;
            ss->last_extra_octet = *string;

            ss->old_code = new_code;
            ss->last_length = this_length;
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_decompress_flush(v42bis_state_t *s)
{
    v42bis_decompress_state_t *ss;

    ss = &s->decompress;
    /* Push out anything remaining. */
    if (ss->output_octet_count > 0)
    {
        ss->handler(ss->user_data, ss->output_buf, ss->output_octet_count);
        ss->output_octet_count = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

#if 0
SPAN_DECLARE(int) v42bis_decompress_dump(v42bis_state_t *s)
{
    int i;
    
    for (i = 0;  i < V42BIS_MAX_CODEWORDS;  i++)
    {
        if (s->decompress.dict[i].parent_code != 0xFFFF)
        {
            printf("Entry %4x, prior %4x, leaves %d, octet %2x\n", i, s->decompress.dict[i].parent_code, s->decompress.dict[i].leaves, s->decompress.dict[i].node_octet);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
#endif

SPAN_DECLARE(void) v42bis_compression_control(v42bis_state_t *s, int mode)
{
    s->compress.compression_mode = mode;
    switch (mode)
    {
    case V42BIS_COMPRESSION_MODE_ALWAYS:
        s->compress.change_transparency = -1;
        break;
    case V42BIS_COMPRESSION_MODE_NEVER:
        s->compress.change_transparency = 1;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v42bis_state_t *) v42bis_init(v42bis_state_t *s,
                                           int negotiated_p0,
                                           int negotiated_p1,
                                           int negotiated_p2,
                                           v42bis_frame_handler_t frame_handler,
                                           void *frame_user_data,
                                           int max_frame_len,
                                           v42bis_data_handler_t data_handler,
                                           void *data_user_data,
                                           int max_data_len)
{
    int i;

    if (negotiated_p1 < 512  ||  negotiated_p1 > 65535)
        return NULL;
    if (negotiated_p2 < 6  ||  negotiated_p2 > V42BIS_MAX_STRING_SIZE)
        return NULL;
    if (s == NULL)
    {
        if ((s = (v42bis_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->compress.handler = frame_handler;
    s->compress.user_data = frame_user_data;
    s->compress.max_len = (max_frame_len < 1024)  ?  max_frame_len  :  1024;

    s->decompress.handler = data_handler;
    s->decompress.user_data = data_user_data;
    s->decompress.max_len = (max_data_len < 1024)  ?  max_data_len  :  1024;

    s->v42bis_parm_p0 = negotiated_p0;  /* default is both ways off */

    s->v42bis_parm_n1 = top_bit(negotiated_p1 - 1) + 1;
    s->v42bis_parm_n2 = negotiated_p1;
    s->v42bis_parm_n7 = negotiated_p2;

    /* 6.5 */
    s->compress.v42bis_parm_c1 =
    s->decompress.v42bis_parm_c1 = V42BIS_N5;

    s->compress.v42bis_parm_c2 =
    s->decompress.v42bis_parm_c2 = V42BIS_N3 + 1;

    s->compress.v42bis_parm_c3 =
    s->decompress.v42bis_parm_c3 = 2*V42BIS_N4;

    s->compress.first =
    s->decompress.first = TRUE;
    for (i = 0;  i < V42BIS_MAX_CODEWORDS;  i++)
    {
        s->compress.dict[i].parent_code =
        s->decompress.dict[i].parent_code = 0xFFFF;
        s->compress.dict[i].leaves =
        s->decompress.dict[i].leaves = 0;
    }
    /* Point the root nodes for decompression to themselves. It doesn't matter much what
       they are set to, as long as they are considered "known" codes. */
    for (i = 0;  i < V42BIS_N5;  i++)
        s->decompress.dict[i].parent_code = (uint16_t) i;
    s->compress.string_code = 0xFFFFFFFF;
    s->compress.latest_code = 0xFFFFFFFF;
    
    s->decompress.last_old_code = 0xFFFFFFFF;
    s->decompress.last_extra_octet = -1;

    s->compress.compression_mode = V42BIS_COMPRESSION_MODE_DYNAMIC;

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_release(v42bis_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_free(v42bis_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
