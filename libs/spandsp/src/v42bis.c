/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42bis.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2011 Steve Underwood
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
#include "spandsp/async.h"
#include "spandsp/v42bis.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/v42bis.h"

/* Fixed parameters from the spec. */
/* Character size (bits) */
#define V42BIS_N3                           8
/* Number of characters in the alphabet */
#define V42BIS_N4                           256
/* Index number of first dictionary entry used to store a string */
#define V42BIS_N5                           (V42BIS_N4 + V42BIS_N6)
/* Number of control codewords */
#define V42BIS_N6                           3 
/* V.42bis/9.2 */
#define V42BIS_ESC_STEP                     51

/* Compreeibility monitoring parameters for assessing automated switches between
   transparent and compressed mode */
#define COMPRESSIBILITY_MONITOR             (256*V42BIS_N3)
#define COMPRESSIBILITY_MONITOR_HYSTERESIS  11

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

static __inline__ void push_octet(v42bis_comp_state_t *s, int octet)
{
    s->output_buf[s->output_octet_count++] = (uint8_t) octet;
    if (s->output_octet_count >= s->max_output_len)
    {
        s->handler(s->user_data, s->output_buf, s->output_octet_count);
        s->output_octet_count = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void push_octets(v42bis_comp_state_t *s, const uint8_t buf[], int len)
{
    int i;
    int chunk;

    i = 0;
    while ((s->output_octet_count + len - i) >= s->max_output_len)
    {
        chunk = s->max_output_len - s->output_octet_count;
        memcpy(&s->output_buf[s->output_octet_count], &buf[i], chunk);
        s->handler(s->user_data, s->output_buf, s->max_output_len);
        s->output_octet_count = 0;
        i += chunk;
    }
    chunk = len - i;
    if (chunk > 0)
    {
        memcpy(&s->output_buf[s->output_octet_count], &buf[i], chunk);
        s->output_octet_count += chunk;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void push_compressed_code(v42bis_comp_state_t *s, int code)
{
    s->bit_buffer |= code << s->bit_count;
    s->bit_count += s->v42bis_parm_c2;
    while (s->bit_count >= 8)
    {
        push_octet(s, s->bit_buffer & 0xFF);
        s->bit_buffer >>= 8;
        s->bit_count -= 8;
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void push_octet_alignment(v42bis_comp_state_t *s)
{
    if ((s->bit_count & 7))
    {
        s->bit_count += (8 - (s->bit_count & 7));
        while (s->bit_count >= 8)
        {
            push_octet(s, s->bit_buffer & 0xFF);
            s->bit_buffer >>= 8;
            s->bit_count -= 8;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static __inline__ void flush_octets(v42bis_comp_state_t *s)
{
    if (s->output_octet_count > 0)
    {
        s->handler(s->user_data, s->output_buf, s->output_octet_count);
        s->output_octet_count = 0;
    }
}
/*- End of function --------------------------------------------------------*/

static void dictionary_init(v42bis_comp_state_t *s)
{
    int i;

    memset(s->dict, 0, sizeof(s->dict));
    for (i = 0;  i < V42BIS_N4;  i++)
        s->dict[i + V42BIS_N6].node_octet = i;
    s->v42bis_parm_c1 = V42BIS_N5;
    s->v42bis_parm_c2 = V42BIS_N3 + 1;
    s->v42bis_parm_c3 = V42BIS_N4 << 1;
    s->last_matched = 0;
    s->update_at = 0;
    s->last_added = 0;
    s->bit_buffer = 0;
    s->bit_count = 0;
    s->flushed_length = 0;
    s->string_length = 0;
    s->escape_code = 0;
    s->transparent = TRUE;
    s->escaped = FALSE;
    s->compression_performance = COMPRESSIBILITY_MONITOR;
}
/*- End of function --------------------------------------------------------*/

static uint16_t match_octet(v42bis_comp_state_t *s, uint16_t at, uint8_t octet)
{
    uint16_t e;

    if (at == 0)
        return octet + V42BIS_N6;
    e = s->dict[at].child;
    while (e)
    {
        if (s->dict[e].node_octet == octet)
            return e;
        e = s->dict[e].next;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static uint16_t add_octet_to_dictionary(v42bis_comp_state_t *s, uint16_t at, uint8_t octet)
{
    uint16_t newx;
    uint16_t next;
    uint16_t e;

    newx = s->v42bis_parm_c1;
    s->dict[newx].node_octet = octet;
    s->dict[newx].parent = at;
    s->dict[newx].child = 0;
    s->dict[newx].next = s->dict[at].child;
    s->dict[at].child = newx;
    next = newx;
    /* 6.5 Recovering a dictionary entry to use next */
    do
    {
        /* 6.5(a) and (b) */
        if (++next == s->v42bis_parm_n2)
            next = V42BIS_N5;
    }
    while (s->dict[next].child);
    /* 6.5(c) We need to reuse a leaf node */
    if (s->dict[next].parent)
    {
        /* 6.5(d) Detach the leaf node from its parent, and re-use it */
        e = s->dict[next].parent;
        if (s->dict[e].child == next)
        {
            s->dict[e].child = s->dict[next].next;
        }
        else
        {
            e = s->dict[e].child;
            while (s->dict[e].next != next)
                e = s->dict[e].next;
            s->dict[e].next = s->dict[next].next;
        }
    }
    s->v42bis_parm_c1 = next;
    return newx;
}
/*- End of function --------------------------------------------------------*/

static void send_string(v42bis_comp_state_t *s)
{
    push_octets(s, s->string, s->string_length);
    s->string_length = 0;
    s->flushed_length = 0;
}
/*- End of function --------------------------------------------------------*/

static void expand_codeword_to_string(v42bis_comp_state_t *s, uint16_t code)
{
    int i;
    uint16_t p;

    /* Work out the length */
    for (i = 0, p = code;  p;  i++)
        p = s->dict[p].parent;
    s->string_length += i;
    /* Now expand the known length of string */
    i = s->string_length - 1;
    for (p = code;  p;  )
    {
        s->string[i--] = s->dict[p].node_octet;
        p = s->dict[p].parent;
    }
}
/*- End of function --------------------------------------------------------*/

static void send_encoded_data(v42bis_comp_state_t *s, uint16_t code)
{
    int i;

    /* Update compressibility metric */
    /* Integrate at the compressed bit rate, and leak at the pre-compression bit rate */
    s->compression_performance += (s->v42bis_parm_c2 - s->compression_performance*s->string_length*V42BIS_N3/COMPRESSIBILITY_MONITOR);
    if (s->transparent)
    {
        for (i = 0;  i < s->string_length;  i++)
        {
            push_octet(s, s->string[i]);
            if (s->string[i] == s->escape_code)
            {
                push_octet(s, V42BIS_EID);
                s->escape_code += V42BIS_ESC_STEP;
            }
        }
    }
    else
    {
        /* Allow for any escape octets in the string */
        for (i = 0;  i < s->string_length;  i++)
        {
            if (s->string[i] == s->escape_code)
                s->escape_code += V42BIS_ESC_STEP;
        }
        /* 7.4 Encoding - we now have the longest matchable string, and will need to output the code for it. */
        while (code >= s->v42bis_parm_c3)
        {
            /* We need to increase the codeword size */
            /* 7.4(a) */
            push_compressed_code(s, V42BIS_STEPUP);
            /* 7.4(b) */
            s->v42bis_parm_c2++;
            /* 7.4(c) */
            s->v42bis_parm_c3 <<= 1;
            /* 7.4(d) this might need to be repeated, so we loop */
        }
        /* 7.5 Transfer - output the last state of the string */
        push_compressed_code(s, code);
    }
    s->string_length = 0;
    s->flushed_length = 0;
}
/*- End of function --------------------------------------------------------*/

static void go_compressed(v42bis_state_t *ss)
{
    v42bis_comp_state_t *s;

    s = &ss->compress;
    if (!s->transparent)
        return;
    span_log(&ss->logging, SPAN_LOG_FLOW, "Changing to compressed mode\n");
    /* Switch out of transparent now, between codes. We need to send the octet which did not
       match, just before switching. */
    if (s->last_matched)
    {
        s->update_at = s->last_matched;
        send_encoded_data(s, s->last_matched);
        s->last_matched = 0;
    }
    push_octet(s, s->escape_code);
    push_octet(s, V42BIS_ECM);
    s->bit_buffer = 0;
    s->transparent = FALSE;
}
/*- End of function --------------------------------------------------------*/

static void go_transparent(v42bis_state_t *ss)
{
    v42bis_comp_state_t *s;

    s = &ss->compress;
    if (s->transparent)
        return;
    span_log(&ss->logging, SPAN_LOG_FLOW, "Changing to transparent mode\n");
    /* Switch into transparent now, between codes, and the unmatched octet should
       go out in transparent mode, just below */
    if (s->last_matched)
    {
        s->update_at = s->last_matched;
        send_encoded_data(s, s->last_matched);
        s->last_matched = 0;
    }
    s->last_added = 0;
    push_compressed_code(s, V42BIS_ETM);
    push_octet_alignment(s);
    s->transparent = TRUE;
}
/*- End of function --------------------------------------------------------*/

static void monitor_for_mode_change(v42bis_state_t *ss)
{
    v42bis_comp_state_t *s;

    s = &ss->compress;
    switch (s->compression_mode)
    {
    case V42BIS_COMPRESSION_MODE_DYNAMIC:
        /* 7.8 Data compressibility test */
        if (s->transparent)
        {
            if (s->compression_performance < COMPRESSIBILITY_MONITOR - COMPRESSIBILITY_MONITOR_HYSTERESIS)
            {
                /* 7.8.1 Transition to compressed mode */
                go_compressed(ss);
            }
        }
        else
        {
            if (s->compression_performance > COMPRESSIBILITY_MONITOR)
            {
                /* 7.8.2 Transition to transparent mode */
                go_transparent(ss);
            }
        }
        /* 7.8.3 Reset function - TODO */
        break;
    case V42BIS_COMPRESSION_MODE_ALWAYS:
        if (s->transparent)
            go_compressed(ss);
        break;
    case V42BIS_COMPRESSION_MODE_NEVER:
        if (!s->transparent)
            go_transparent(ss);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int v42bis_comp_init(v42bis_comp_state_t *s,
                            int p1,
                            int p2,
                            put_msg_func_t handler,
                            void *user_data,
                            int max_output_len)
{
    memset(s, 0, sizeof(*s));
    s->v42bis_parm_n2 = p1;
    s->v42bis_parm_n7 = p2;
    s->handler = handler;
    s->user_data = user_data;
    s->max_output_len = (max_output_len < V42BIS_MAX_OUTPUT_LENGTH)  ?  max_output_len  :  V42BIS_MAX_OUTPUT_LENGTH;
    s->output_octet_count = 0;
    dictionary_init(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int comp_exit(v42bis_comp_state_t *s)
{
    s->v42bis_parm_n2 = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_compress(v42bis_state_t *ss, const uint8_t buf[], int len)
{
    v42bis_comp_state_t *s;
    int i;
    uint16_t code;

    s = &ss->compress;
    if (!s->v42bis_parm_p0)
    {
        /* Compression is off - just push the incoming data out */
        push_octets(s, buf, len);
        return 0;
    }
    for (i = 0;  i < len;  )
    {
        /* 6.4 Add the string to the dictionary */
        if (s->update_at)
        {
            if (match_octet(s, s->update_at, buf[i]) == 0)
                s->last_added = add_octet_to_dictionary(s, s->update_at, buf[i]);
            s->update_at = 0;
        }
        /* Match string */
        while (i < len)
        {
            code = match_octet(s, s->last_matched, buf[i]);
            if (code == 0)
            {
                s->update_at = s->last_matched;
                send_encoded_data(s, s->last_matched);
                s->last_matched = 0;
                break;
            }
            if (code == s->last_added)
            {
                s->last_added = 0;
                send_encoded_data(s, s->last_matched);
                s->last_matched = 0;
                break;
            }
            s->last_matched = code;
            /* 6.3(b) If the string matches a dictionary entry, and the entry is not that entry
                      created by the last invocation of the string matching procedure, then the
                      next character shall be read and appended to the string and this step
                      repeated. */
            s->string[s->string_length++] = buf[i++];
            /* 6.4(a) The string must not exceed N7 in length */
            if (s->string_length + s->flushed_length == s->v42bis_parm_n7)
            {
                send_encoded_data(s, s->last_matched);
                s->last_matched = 0;
                break;
            }
        }
        monitor_for_mode_change(ss);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_compress_flush(v42bis_state_t *ss)
{
    v42bis_comp_state_t *s;
    int len;
    
    s = &ss->compress;
    if (s->update_at)
        return 0;
    if (s->last_matched)
    {
        len = s->string_length;
        send_encoded_data(s, s->last_matched);
        s->flushed_length += len;
    }
    if (!s->transparent)
    {
        s->update_at = s->last_matched;
        s->last_matched = 0;
        s->flushed_length = 0;
        push_compressed_code(s, V42BIS_FLUSH);
        push_octet_alignment(s);
    }
    flush_octets(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_decompress(v42bis_state_t *ss, const uint8_t buf[], int len)
{
    v42bis_comp_state_t *s;
    int i;
    int j;
    int yyy;
    uint16_t code;
    uint16_t p;
    uint8_t ch;
    uint8_t in;

    s = &ss->decompress;
    if (!s->v42bis_parm_p0)
    {
        /* Compression is off - just push the incoming data out */
        push_octets(s, buf, len);
        return 0;
    }
    for (i = 0;  i < len;  )
    {
        if (s->transparent)
        {
            in = buf[i];
            if (s->escaped)
            {
                /* Command */
                s->escaped = FALSE;
                switch (in)
                {
                case V42BIS_ECM:
                    /* Enter compressed mode */
                    span_log(&ss->logging, SPAN_LOG_FLOW, "Hit V42BIS_ECM\n");
                    send_string(s);
                    s->transparent = FALSE;
                    s->update_at = s->last_matched;
                    s->last_matched = 0;
                    i++;
                    continue;
                case V42BIS_EID:
                    /* Escape symbol */
                    span_log(&ss->logging, SPAN_LOG_FLOW, "Hit V42BIS_EID\n");
                    in = s->escape_code;
                    s->escape_code += V42BIS_ESC_STEP;
                    break;
                case V42BIS_RESET:
                    /* Reset dictionary */
                    span_log(&ss->logging, SPAN_LOG_FLOW, "Hit V42BIS_RESET\n");
                    /* TODO: */
                    send_string(s);
                    dictionary_init(s);
                    i++;
                    continue;
                default:
                    span_log(&ss->logging, SPAN_LOG_FLOW, "Hit V42BIS_???? - %" PRIu32 "\n", in);
                    return -1;
                }
            }
            else if (in == s->escape_code)
            {
                s->escaped = TRUE;
                i++;
                continue;
            }

            yyy = TRUE;
            for (j = 0;  j < 2  &&  yyy;  j++)
            {
                if (s->update_at)
                {
                    if (match_octet(s, s->update_at, in) == 0)
                        s->last_added = add_octet_to_dictionary(s, s->update_at, in);
                    s->update_at = 0;
                }

                code = match_octet(s, s->last_matched, in);
                if (code == 0)
                {
                    s->update_at = s->last_matched;
                    send_string(s);
                    s->last_matched = 0;
                }
                else if (code == s->last_added)
                {
                    s->last_added = 0;
                    send_string(s);
                    s->last_matched = 0;
                }
                else
                {
                    s->last_matched = code;
                    s->string[s->string_length++] = in;
                    if (s->string_length + s->flushed_length == s->v42bis_parm_n7)
                    {
                        send_string(s);
                        s->last_matched = 0;
                    }
                    i++;
                    yyy = FALSE;
                }
            }
        }
        else
        {
            /* Get code from input */
            while (s->bit_count < s->v42bis_parm_c2  &&  i < len)
            {
                s->bit_buffer |= buf[i++] << s->bit_count;
                s->bit_count += 8;
            }
            if (s->bit_count < s->v42bis_parm_c2)
                continue;
            code = s->bit_buffer & ((1 << s->v42bis_parm_c2) - 1);
            s->bit_buffer >>= s->v42bis_parm_c2;
            s->bit_count -= s->v42bis_parm_c2;

            if (code < V42BIS_N6)
            {
                /* We have a control code. */
                switch (code)
                {
                case V42BIS_ETM:
                    /* Enter transparent mode */
                    span_log(&ss->logging, SPAN_LOG_FLOW, "Hit V42BIS_ETM\n");
                    s->bit_count = 0;
                    s->transparent = TRUE;
                    s->last_matched = 0;
                    s->last_added = 0;
                    break;
                case V42BIS_FLUSH:
                    /* Flush signal */
                    span_log(&ss->logging, SPAN_LOG_FLOW, "Hit V42BIS_FLUSH\n");
                    s->bit_count = 0;
                    break;
                case V42BIS_STEPUP:
                    /* Increase code word size */
                    span_log(&ss->logging, SPAN_LOG_FLOW, "Hit V42BIS_STEPUP\n");
                    s->v42bis_parm_c2++;
                    s->v42bis_parm_c3 <<= 1;
                    if (s->v42bis_parm_c2 > (s->v42bis_parm_n2 >> 3))
                        return -1;
                    break;
                }
                continue;
            }
            /* Regular codeword */
            if (code == s->v42bis_parm_c1)
                return -1;
            expand_codeword_to_string(s, code);
            if (s->update_at)
            {
                ch = s->string[0];
                if ((p = match_octet(s, s->update_at, ch)) == 0)
                {
                    s->last_added = add_octet_to_dictionary(s, s->update_at, ch);
                    if (code == s->v42bis_parm_c1)
                        return -1;
                }
                else if (p == s->last_added)
                {
                    s->last_added = 0;
                }
            }
            s->update_at = ((s->string_length + s->flushed_length) == s->v42bis_parm_n7)  ?  0  :  code;
            /* Allow for any escapes which may be in this string */
            for (j = 0;  j < s->string_length;  j++)
            {
                if (s->string[j] == s->escape_code)
                    s->escape_code += V42BIS_ESC_STEP;
            }
            send_string(s);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42bis_decompress_flush(v42bis_state_t *ss)
{
    v42bis_comp_state_t *s;
    int len;
    
    s = &ss->decompress;
    len = s->string_length;
    send_string(s);
    s->flushed_length += len;
    flush_octets(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42bis_compression_control(v42bis_state_t *s, int mode)
{
    s->compress.compression_mode = mode;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v42bis_state_t *) v42bis_init(v42bis_state_t *s,
                                           int negotiated_p0,
                                           int negotiated_p1,
                                           int negotiated_p2,
                                           put_msg_func_t encode_handler,
                                           void *encode_user_data,
                                           int max_encode_len,
                                           put_msg_func_t decode_handler,
                                           void *decode_user_data,
                                           int max_decode_len)
{
    int ret;

    if (negotiated_p1 < V42BIS_MIN_DICTIONARY_SIZE  ||  negotiated_p1 > 65535)
        return NULL;
    if (negotiated_p2 < V42BIS_MIN_STRING_SIZE  ||  negotiated_p2 > V42BIS_MAX_STRING_SIZE)
        return NULL;
    if (s == NULL)
    {
        if ((s = (v42bis_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "V.42bis");

    if ((ret = v42bis_comp_init(&s->compress, negotiated_p1, negotiated_p2, encode_handler, encode_user_data, max_encode_len)))
        return NULL;
    if ((ret = v42bis_comp_init(&s->decompress, negotiated_p1, negotiated_p2, decode_handler, decode_user_data, max_decode_len)))
    {
        comp_exit(&s->compress);
        return NULL;
    }
    s->compress.v42bis_parm_p0 = negotiated_p0 & 2;
    s->decompress.v42bis_parm_p0 = negotiated_p0 & 1;

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
    comp_exit(&s->compress);
    comp_exit(&s->decompress);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
