/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_core_tests.c - Tests for the T.38 FoIP core module.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
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
 */

/*! \file */

/*! \page t38_core_tests_page T.38 core tests
\section t38_core_tests_page_sec_1 What does it do?
These tests exercise the T.38 core ASN.1 processing code.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#if !defined(WIN32)
#include <unistd.h>
#endif

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#define MAX_FIELDS      42
#define MAX_FIELD_LEN   8192

static bool succeeded = true;
static int t38_version;
static int ok_indicator_packets;
static int bad_indicator_packets;
static int ok_data_packets;
static int bad_data_packets;
static int missing_packets;

static int skip;

static uint8_t field_body[MAX_FIELDS][MAX_FIELD_LEN];
static int field_len[MAX_FIELDS];
static int seq_no;

static int msg_list[1000000];
static int msg_list_ptr;
static int msg_list_ptr2;
static uint8_t concat[1000000];
static int concat_len;

static int rx_missing_attack_handler(t38_core_state_t *s, void *user_data, int rx_seq_no, int expected_seq_no)
{
    //printf("Hit missing\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_indicator_attack_handler(t38_core_state_t *s, void *user_data, int indicator)
{
    //printf("Hit indicator %d\n", indicator);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_data_attack_handler(t38_core_state_t *s, void *user_data, int data_type, int field_type, const uint8_t *buf, int len)
{
    //printf("Hit data %d, field %d\n", data_type, field_type);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_missing_handler(t38_core_state_t *s, void *user_data, int rx_seq_no, int expected_seq_no)
{
    missing_packets++;
    //printf("Hit missing\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_indicator_handler(t38_core_state_t *s, void *user_data, int indicator)
{
    if (indicator == msg_list[msg_list_ptr2++])
        ok_indicator_packets++;
    else
        bad_indicator_packets++;
    //printf("Hit indicator %d\n", indicator);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_data_handler(t38_core_state_t *s, void *user_data, int data_type, int field_type, const uint8_t *buf, int len)
{
    if (data_type == msg_list[msg_list_ptr2]  &&  field_type == msg_list[msg_list_ptr2 + 1])
        ok_data_packets++;
    else
        bad_data_packets++;
    msg_list_ptr2 += 2;
    //printf("Hit data %d, field %d\n", data_type, field_type);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    t38_core_state_t *t;

    t = (t38_core_state_t *) user_data;
    span_log(t38_core_get_logging_state(s), SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);
    if (t38_core_rx_ifp_packet(t, buf, len, seq_no) < 0)
        succeeded = false;
    seq_no++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_concat_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    span_log(t38_core_get_logging_state(s), SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);
    memcpy(&concat[concat_len], buf, len);
    concat_len += len;
    seq_no++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int encode_decode_tests(t38_core_state_t *a, t38_core_state_t *b)
{
    t38_data_field_t field[MAX_FIELDS];
    int i;
    int j;

    ok_indicator_packets = 0;
    bad_indicator_packets = 0;
    ok_data_packets = 0;
    bad_data_packets = 0;
    missing_packets = 0;

    msg_list_ptr = 0;
    msg_list_ptr2 = 0;

    /* Try all the indicator types */
    for (i = 0;  i < 100;  i++)
    {
        msg_list[msg_list_ptr++] = i;
        if (t38_core_send_indicator(a, i) < 0)
        {
            msg_list_ptr--;
            break;
        }
    }

    /* Try all the data types, as single field messages with no data */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = j;
            skip = 99;
            if (t38_core_send_data(a, i, j, (uint8_t *) "", 0, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
            {
                msg_list_ptr -= 2;
                break;
            }
        }
        if (j == 0)
            break;
    }

    /* Try all the data types and field types, as single field messages with data */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = j;
            skip = 99;
            if (t38_core_send_data(a, i, j, (uint8_t *) "ABCD", 4, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
            {
                msg_list_ptr -= 2;
                break;
            }
        }
        if (j == 0)
            break;
    }

    /* Try all the data types and field types, as multi-field messages, but with 0 fields */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            skip = 1;
            if (t38_core_send_data_multi_field(a, i, field, 0, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
                break;
        }
        if (j == 0)
            break;
    }

    /* Try all the data types and field types, as multi-field messages */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = j;
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = T38_FIELD_T4_NON_ECM_SIG_END;
            skip = 1;

            field_len[0] = 444;
            field_len[1] = 333;

            field[0].field_type = j;
            field[0].field = field_body[0];
            field[0].field_len = field_len[0];
            field[1].field_type = T38_FIELD_T4_NON_ECM_SIG_END;
            field[1].field = field_body[1];
            field[1].field_len = field_len[1];
            if (t38_core_send_data_multi_field(a, i, field, 2, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
            {
                msg_list_ptr -= 4;
                break;
            }
        }
        if (j == 0)
            break;
    }

    printf("Indicator packets: OK = %d, bad = %d\n", ok_indicator_packets, bad_indicator_packets);
    printf("Data packets: OK = %d, bad = %d\n", ok_data_packets, bad_data_packets);
    printf("Missing packets = %d\n", missing_packets);

    if (t38_version == 0)
    {
        if (ok_indicator_packets != 16  ||  bad_indicator_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
        if (ok_data_packets != 288  ||  bad_data_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
    }
    else
    {
        if (ok_indicator_packets != 23  ||  bad_indicator_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
        if (ok_data_packets != 720  ||  bad_data_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
    }
    if (missing_packets > 0)
    {
        printf("Tests failed\n");
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int encode_then_decode_tests(t38_core_state_t *a, t38_core_state_t *b)
{
    t38_data_field_t field[MAX_FIELDS];
    int len;
    int i;
    int j;

    ok_indicator_packets = 0;
    bad_indicator_packets = 0;
    ok_data_packets = 0;
    bad_data_packets = 0;
    missing_packets = 0;

    msg_list_ptr = 0;
    msg_list_ptr2 = 0;

    /* Try all the indicator types */
    for (i = 0;  i < 100;  i++)
    {
        msg_list[msg_list_ptr++] = i;
        if (t38_core_send_indicator(a, i) < 0)
        {
            msg_list_ptr--;
            break;
        }
    }

    /* Try all the data types, as single field messages with no data */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = j;
            skip = 99;
            if (t38_core_send_data(a, i, j, (uint8_t *) "", 0, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
            {
                msg_list_ptr -= 2;
                break;
            }
        }
        if (j == 0)
            break;
    }

    /* Try all the data types and field types, as single field messages with data */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = j;
            skip = 99;
            if (t38_core_send_data(a, i, j, (uint8_t *) "ABCD", 4, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
            {
                msg_list_ptr -= 2;
                break;
            }
        }
        if (j == 0)
            break;
    }

    /* Try all the data types and field types, as multi-field messages, but with 0 fields */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            skip = 1;
            if (t38_core_send_data_multi_field(a, i, field, 0, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
                break;
        }
        if (j == 0)
            break;
    }

    /* Try all the data types and field types, as multi-field messages */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = j;
            msg_list[msg_list_ptr++] = i;
            msg_list[msg_list_ptr++] = T38_FIELD_T4_NON_ECM_SIG_END;
            skip = 1;

            field_len[0] = 444;
            field_len[1] = 333;

            field[0].field_type = j;
            field[0].field = field_body[0];
            field[0].field_len = field_len[0];
            field[1].field_type = T38_FIELD_T4_NON_ECM_SIG_END;
            field[1].field = field_body[1];
            field[1].field_len = field_len[1];
            if (t38_core_send_data_multi_field(a, i, field, 2, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
            {
                msg_list_ptr -= 4;
                break;
            }
        }
        if (j == 0)
            break;
    }

    /* Now split up the big concatented block of IFP packets. */
    for (i = 0, seq_no = 0;  i < concat_len;  i += len)
    {
        if ((len = t38_core_rx_ifp_stream(b, &concat[i], concat_len - i, seq_no)) < 0)
            succeeded = false;
        seq_no++;
    }

    printf("Indicator packets: OK = %d, bad = %d\n", ok_indicator_packets, bad_indicator_packets);
    printf("Data packets: OK = %d, bad = %d\n", ok_data_packets, bad_data_packets);
    printf("Missing packets = %d\n", missing_packets);

    if (t38_version == 0)
    {
        if (ok_indicator_packets != 16  ||  bad_indicator_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
        if (ok_data_packets != 288  ||  bad_data_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
    }
    else
    {
        if (ok_indicator_packets != 23  ||  bad_indicator_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
        if (ok_data_packets != 720  ||  bad_data_packets != 0)
        {
            printf("Tests failed\n");
            return -1;
        }
    }
    if (missing_packets > 0)
    {
        printf("Tests failed\n");
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int attack_tests(t38_core_state_t *s, int packets)
{
    int i;
    int j;
    int len;
    uint8_t buf[1024];
    int seq_no;

    srand(1234567);
    /* Send lots of random junk, of increasing length. Much of this will decode
       as valid IFP frames, but none of it should cause trouble. */
    seq_no = 0;
    for (len = 1;  len < 70;  len++)
    {
        for (i = 0;  i < packets;  i++)
        {
            for (j = 0;  j < len;  j++)
                buf[j] = (rand() >> 16) & 0xFF;
            t38_core_rx_ifp_packet(s, buf, len, seq_no);
            seq_no = (seq_no + 1) & 0xFFFF;
        }
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    t38_core_state_t t38_core_ax;
    t38_core_state_t t38_core_bx;
    t38_core_state_t *t38_core_a;
    t38_core_state_t *t38_core_b;
    int attack_packets;
    int opt;

    attack_packets = 100000;
    while ((opt = getopt(argc, argv, "a:")) != -1)
    {
        switch (opt)
        {
        case 'a':
            attack_packets = atoi(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    /* Tests in UDP type mode, for UDPTL and RTP */
    for (t38_version = 0;  t38_version < 2;  t38_version++)
    {
        seq_no = 0;

        printf("Using T.38 version %d\n", t38_version);

        if ((t38_core_a = t38_core_init(&t38_core_ax,
                                        rx_indicator_handler,
                                        rx_data_handler,
                                        rx_missing_handler,
                                        &t38_core_bx,
                                        tx_packet_handler,
                                        &t38_core_bx)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }
        if ((t38_core_b = t38_core_init(&t38_core_bx,
                                        rx_indicator_handler,
                                        rx_data_handler,
                                        rx_missing_handler,
                                        &t38_core_ax,
                                        tx_packet_handler,
                                        &t38_core_ax)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }

        t38_set_t38_version(t38_core_a, t38_version);
        t38_set_t38_version(t38_core_b, t38_version);

        span_log_set_level(t38_core_get_logging_state(t38_core_a), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(t38_core_get_logging_state(t38_core_a), "T.38-A");
        span_log_set_level(t38_core_get_logging_state(t38_core_b), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(t38_core_get_logging_state(t38_core_b), "T.38-B");

        /* Encode and decode all possible frame types, one by one */
        if (encode_decode_tests(t38_core_a, t38_core_b))
        {
            printf("Encode/decode tests failed\n");
            exit(2);
        }

        if ((t38_core_a = t38_core_init(&t38_core_ax,
                                        rx_indicator_attack_handler,
                                        rx_data_attack_handler,
                                        rx_missing_attack_handler,
                                        &t38_core_bx,
                                        tx_packet_handler,
                                        &t38_core_bx)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }

        t38_set_t38_version(t38_core_a, t38_version);

        //span_log_set_level(t38_core_get_logging_state(t38_core_a), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        //span_log_set_tag(t38_core_get_logging_state(t38_core_a), "T.38-A");

        if (attack_tests(t38_core_a, attack_packets))
        {
            printf("Attack tests failed\n");
            exit(2);
        }
    }

    /* Tests in TCP without TPKT mode, like T.38 version 0 */
    for (t38_version = 0;  t38_version < 2;  t38_version++)
    {
        seq_no = 0;
        concat_len = 0;

        printf("Using T.38 version %d\n", t38_version);

        if ((t38_core_a = t38_core_init(&t38_core_ax,
                                        rx_indicator_handler,
                                        rx_data_handler,
                                        rx_missing_handler,
                                        &t38_core_bx,
                                        tx_concat_packet_handler,
                                        &t38_core_bx)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }
        if ((t38_core_b = t38_core_init(&t38_core_bx,
                                        rx_indicator_handler,
                                        rx_data_handler,
                                        rx_missing_handler,
                                        &t38_core_ax,
                                        tx_concat_packet_handler,
                                        &t38_core_ax)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }

        t38_set_t38_version(t38_core_a, t38_version);
        t38_set_t38_version(t38_core_b, t38_version);

        t38_set_pace_transmission(t38_core_a, false);
        t38_set_pace_transmission(t38_core_b, false);

        t38_set_data_transport_protocol(t38_core_a, T38_TRANSPORT_TCP);
        t38_set_data_transport_protocol(t38_core_b, T38_TRANSPORT_TCP);

        span_log_set_level(t38_core_get_logging_state(t38_core_a), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(t38_core_get_logging_state(t38_core_a), "T.38-A");
        span_log_set_level(t38_core_get_logging_state(t38_core_b), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(t38_core_get_logging_state(t38_core_b), "T.38-B");

        /* Encode all possible frames types into a large block, and then decode them */
        if (encode_then_decode_tests(t38_core_a, t38_core_b))
        {
            printf("Encode then decode tests failed\n");
            exit(2);
        }

        if ((t38_core_a = t38_core_init(&t38_core_ax,
                                        rx_indicator_attack_handler,
                                        rx_data_attack_handler,
                                        rx_missing_attack_handler,
                                        &t38_core_bx,
                                        tx_packet_handler,
                                        &t38_core_bx)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }

        t38_set_t38_version(t38_core_a, t38_version);

        t38_set_pace_transmission(t38_core_a, false);

        t38_set_data_transport_protocol(t38_core_a, T38_TRANSPORT_TCP);

        //span_log_set_level(t38_core_get_logging_state(t38_core_a), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        //span_log_set_tag(t38_core_get_logging_state(t38_core_a), "T.38-A");

        if (attack_tests(t38_core_a, attack_packets))
        {
            printf("Attack tests failed\n");
            exit(2);
        }
    }

    /* Tests in TCP with TPKT mode, like T.38 versions >0 */
    for (t38_version = 0;  t38_version < 2;  t38_version++)
    {
        seq_no = 0;
        concat_len = 0;

        printf("Using T.38 version %d\n", t38_version);

        if ((t38_core_a = t38_core_init(&t38_core_ax,
                                        rx_indicator_handler,
                                        rx_data_handler,
                                        rx_missing_handler,
                                        &t38_core_bx,
                                        tx_concat_packet_handler,
                                        &t38_core_bx)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }
        if ((t38_core_b = t38_core_init(&t38_core_bx,
                                        rx_indicator_handler,
                                        rx_data_handler,
                                        rx_missing_handler,
                                        &t38_core_ax,
                                        tx_concat_packet_handler,
                                        &t38_core_ax)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }

        t38_set_t38_version(t38_core_a, t38_version);
        t38_set_t38_version(t38_core_b, t38_version);

        t38_set_pace_transmission(t38_core_a, false);
        t38_set_pace_transmission(t38_core_b, false);

        t38_set_data_transport_protocol(t38_core_a, T38_TRANSPORT_TCP_TPKT);
        t38_set_data_transport_protocol(t38_core_b, T38_TRANSPORT_TCP_TPKT);

        span_log_set_level(t38_core_get_logging_state(t38_core_a), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(t38_core_get_logging_state(t38_core_a), "T.38-A");
        span_log_set_level(t38_core_get_logging_state(t38_core_b), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(t38_core_get_logging_state(t38_core_b), "T.38-B");

        /* Encode all possible frames types into a large block, and then decode them */
        if (encode_then_decode_tests(t38_core_a, t38_core_b))
        {
            printf("Encode then decode tests failed\n");
            exit(2);
        }

        if ((t38_core_a = t38_core_init(&t38_core_ax,
                                        rx_indicator_attack_handler,
                                        rx_data_attack_handler,
                                        rx_missing_attack_handler,
                                        &t38_core_bx,
                                        tx_packet_handler,
                                        &t38_core_bx)) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }
        t38_set_t38_version(t38_core_a, t38_version);

        t38_set_pace_transmission(t38_core_a, false);

        t38_set_data_transport_protocol(t38_core_a, T38_TRANSPORT_TCP_TPKT);

        //span_log_set_level(t38_core_get_logging_state(t38_core_a), SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        //span_log_set_tag(t38_core_get_logging_state(t38_core_a), "T.38-A");

        if (attack_tests(t38_core_a, attack_packets))
        {
            printf("Attack tests failed\n");
            exit(2);
        }
    }

    if (!succeeded)
    {
        printf("Tests failed\n");
        exit(2);
    }
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
