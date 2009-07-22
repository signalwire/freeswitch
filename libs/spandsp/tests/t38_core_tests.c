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
 *
 * $Id: t38_core_tests.c,v 1.16 2009/07/14 13:54:22 steveu Exp $
 */

/*! \file */

/*! \page t38_core_tests_page T.38 core tests
\section t38_core_tests_page_sec_1 What does it do?
These tests exercise the T.38 core ASN.1 processing code.
*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

#define MAX_FIELDS      42
#define MAX_FIELD_LEN   8192

int t38_version;
int succeeded = TRUE;
int ok_indicator_packets;
int bad_indicator_packets;
int ok_data_packets;
int bad_data_packets;
int missing_packets;

int current_indicator;
int current_data_type;
int current_field_type;
int skip;

uint8_t field_body[MAX_FIELDS][MAX_FIELD_LEN];
int field_len[MAX_FIELDS];

static int rx_missing_handler(t38_core_state_t *s, void *user_data, int rx_seq_no, int expected_seq_no)
{
    missing_packets++;
    //printf("Hit missing\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_indicator_handler(t38_core_state_t *s, void *user_data, int indicator)
{
    if (indicator == current_indicator)
        ok_indicator_packets++;
    else
        bad_indicator_packets++;
    //printf("Hit indicator %d\n", indicator);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_data_handler(t38_core_state_t *s, void *user_data, int data_type, int field_type, const uint8_t *buf, int len)
{
    if (--skip >= 0)
    {
        if (data_type == current_data_type  &&  field_type == current_field_type)
            ok_data_packets++;
        else
            bad_data_packets++;
    }
    else
    {
        if (data_type == current_data_type  &&  field_type == T38_FIELD_T4_NON_ECM_SIG_END)
            ok_data_packets++;
        else
            bad_data_packets++;
    }
    //printf("Hit data %d, field %d\n", data_type, field_type);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
    t38_core_state_t *t;
    static int seq_no = 0;
    
    t = (t38_core_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Send seq %d, len %d, count %d\n", s->tx_seq_no, len, count);
    if (t38_core_rx_ifp_packet(t, buf, len, seq_no) < 0)
        succeeded = FALSE;
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

    /* Try all the indicator types */
    for (i = 0;  i < 100;  i++)
    {
        current_indicator = i;
        if (t38_core_send_indicator(a, i) < 0)
            break;
    }

    /* Try all the data types, as single field messages with no data */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            current_data_type = i;
            current_field_type = j;
            skip = 99;
            if (t38_core_send_data(a, i, j, (uint8_t *) "", 0, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
                break;
        }
        if (j == 0)
            break;
    }

    /* Try all the data types and field types, as single field messages with data */
    for (i = 0;  i < 100;  i++)
    {
        for (j = 0;  j < 100;  j++)
        {
            current_data_type = i;
            current_field_type = j;
            skip = 99;
            if (t38_core_send_data(a, i, j, (uint8_t *) "ABCD", 4, T38_PACKET_CATEGORY_CONTROL_DATA) < 0)
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
            current_data_type = i;
            current_field_type = j;
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
                break;
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

static int attack_tests(t38_core_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    t38_core_state_t t38_core_a;
    t38_core_state_t t38_core_b;

    for (t38_version = 0;  t38_version < 2;  t38_version++)
    {
        printf("Using T.38 version %d\n", t38_version);

        if (t38_core_init(&t38_core_a,
                          rx_indicator_handler,
                          rx_data_handler,
                          rx_missing_handler,
                          &t38_core_b,
                          tx_packet_handler,
                          &t38_core_b) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }
        if (t38_core_init(&t38_core_b,
                          rx_indicator_handler,
                          rx_data_handler,
                          rx_missing_handler,
                          &t38_core_a,
                          tx_packet_handler,
                          &t38_core_a) == NULL)
        {
            fprintf(stderr, "Cannot start the T.38 core\n");
            exit(2);
        }

        t38_set_t38_version(&t38_core_a, t38_version);
        t38_set_t38_version(&t38_core_b, t38_version);

        span_log_set_level(&t38_core_a.logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(&t38_core_a.logging, "T.38-A");
        span_log_set_level(&t38_core_b.logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(&t38_core_b.logging, "T.38-B");

        if (encode_decode_tests(&t38_core_a, &t38_core_b))
        {
            printf("Encode/decode tests failed\n");
            exit(2);
        }
        if (attack_tests(&t38_core_a))
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
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
