/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tsb85_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sndfile.h>

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#include "fax_tester.h"
#include "fax_utils.h"

#define OUTPUT_TIFF_FILE_NAME   "tsb85.tif"

#define OUTPUT_WAVE_FILE_NAME   "tsb85.wav"

#define SAMPLES_PER_CHUNK       160

SNDFILE *out_handle;

bool use_receiver_not_ready = false;
bool test_local_interrupt = false;

const char *output_tiff_file_name;

bool log_audio = false;

fax_state_t *fax;
faxtester_state_t state;

uint8_t image[1000000];

uint8_t awaited[1000];
int awaited_len = 0;

char image_path[1024];

t30_exchanged_info_t expected_rx_info;

char next_tx_file[1000];

static int timein_x = -1;
static int timeout_x = -1;

static int next_step(faxtester_state_t *s);

static bool test_for_call_clear = false;
static int call_clear_timer = 0;

static bool far_end_cleared_call = false;

struct
{
    const char *tag;
    int code;
} t30_status[] =
{
    {"OK", T30_ERR_OK},
    {"CEDTONE", T30_ERR_CEDTONE},
    {"T0_EXPIRED", T30_ERR_T0_EXPIRED},
    {"T1_EXPIRED", T30_ERR_T1_EXPIRED},
    {"T3_EXPIRED", T30_ERR_T3_EXPIRED},
    {"HDLC_CARRIER", T30_ERR_HDLC_CARRIER},
    {"CANNOT_TRAIN", T30_ERR_CANNOT_TRAIN},
    {"OPER_INT_FAIL", T30_ERR_OPER_INT_FAIL},
    {"INCOMPATIBLE", T30_ERR_INCOMPATIBLE},
    {"RX_INCAPABLE", T30_ERR_RX_INCAPABLE},
    {"TX_INCAPABLE", T30_ERR_TX_INCAPABLE},
    {"NORESSUPPORT", T30_ERR_NORESSUPPORT},
    {"NOSIZESUPPORT", T30_ERR_NOSIZESUPPORT},
    {"UNEXPECTED", T30_ERR_UNEXPECTED},
    {"TX_BADDCS", T30_ERR_TX_BADDCS},
    {"TX_BADPG", T30_ERR_TX_BADPG},
    {"TX_ECMPHD", T30_ERR_TX_ECMPHD},
    {"TX_GOTDCN", T30_ERR_TX_GOTDCN},
    {"TX_INVALRSP", T30_ERR_TX_INVALRSP},
    {"TX_NODIS", T30_ERR_TX_NODIS},
    {"TX_PHBDEAD", T30_ERR_TX_PHBDEAD},
    {"TX_PHDDEAD", T30_ERR_TX_PHDDEAD},
    {"TX_T5EXP", T30_ERR_TX_T5EXP},
    {"RX_ECMPHD", T30_ERR_RX_ECMPHD},
    {"RX_GOTDCS", T30_ERR_RX_GOTDCS},
    {"RX_INVALCMD", T30_ERR_RX_INVALCMD},
    {"RX_NOCARRIER", T30_ERR_RX_NOCARRIER},
    {"RX_NOEOL", T30_ERR_RX_NOEOL},
    {"RX_NOFAX", T30_ERR_RX_NOFAX},
    {"RX_T2EXPDCN", T30_ERR_RX_T2EXPDCN},
    {"RX_T2EXPD", T30_ERR_RX_T2EXPD},
    {"RX_T2EXPFAX", T30_ERR_RX_T2EXPFAX},
    {"RX_T2EXPMPS", T30_ERR_RX_T2EXPMPS},
    {"RX_T2EXPRR", T30_ERR_RX_T2EXPRR},
    {"RX_T2EXP", T30_ERR_RX_T2EXP},
    {"RX_DCNWHY", T30_ERR_RX_DCNWHY},
    {"RX_DCNDATA", T30_ERR_RX_DCNDATA},
    {"RX_DCNFAX", T30_ERR_RX_DCNFAX},
    {"RX_DCNPHD", T30_ERR_RX_DCNPHD},
    {"RX_DCNRRD", T30_ERR_RX_DCNRRD},
    {"RX_DCNNORTN", T30_ERR_RX_DCNNORTN},
    {"FILEERROR", T30_ERR_FILEERROR},
    {"NOPAGE", T30_ERR_NOPAGE},
    {"BADTIFF", T30_ERR_BADTIFF},
    {"BADPAGE", T30_ERR_BADPAGE},
    {"BADTAG", T30_ERR_BADTAG},
    {"BADTIFFHDR", T30_ERR_BADTIFFHDR},
    {"NOMEM", T30_ERR_NOMEM},
    {"RETRYDCN", T30_ERR_RETRYDCN},
    {"CALLDROPPED", T30_ERR_CALLDROPPED},
    {"NOPOLL", T30_ERR_NOPOLL},
    {"IDENT_UNACCEPTABLE", T30_ERR_IDENT_UNACCEPTABLE},
    {"SUB_UNACCEPTABLE", T30_ERR_SUB_UNACCEPTABLE},
    {"SEP_UNACCEPTABLE", T30_ERR_SEP_UNACCEPTABLE},
    {"PSA_UNACCEPTABLE", T30_ERR_PSA_UNACCEPTABLE},
    {"SID_UNACCEPTABLE", T30_ERR_SID_UNACCEPTABLE},
    {"PWD_UNACCEPTABLE", T30_ERR_PWD_UNACCEPTABLE},
    {"TSA_UNACCEPTABLE", T30_ERR_TSA_UNACCEPTABLE},
    {"IRA_UNACCEPTABLE", T30_ERR_IRA_UNACCEPTABLE},
    {"CIA_UNACCEPTABLE", T30_ERR_CIA_UNACCEPTABLE},
    {"ISP_UNACCEPTABLE", T30_ERR_ISP_UNACCEPTABLE},
    {"CSA_UNACCEPTABLE", T30_ERR_CSA_UNACCEPTABLE},
    {NULL, -1}
};

static int phase_b_handler(void *user_data, int result)
{
    int ch;
    int status;
    t30_state_t *s;
    const char *u;

    s = (t30_state_t *) user_data;
    ch = 'A';
    status = T30_ERR_OK;
    if ((u = t30_get_rx_ident(s)))
    {
        printf("%c: Phase B: remote ident '%s'\n", ch, u);
        if (expected_rx_info.ident[0]  &&  strcmp(expected_rx_info.ident, u))
        {
            printf("%c: Phase B: remote ident incorrect! - expected '%s'\n", ch, expected_rx_info.ident);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info.ident[0])
        {
            printf("%c: Phase B: remote ident missing!\n", ch);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sub_address(s)))
    {
        printf("%c: Phase B: remote sub-address '%s'\n", ch, u);
        if (expected_rx_info.sub_address[0]  &&  strcmp(expected_rx_info.sub_address, u))
        {
            printf("%c: Phase B: remote sub-address incorrect! - expected '%s'\n", ch, expected_rx_info.sub_address);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info.sub_address[0])
        {
            printf("%c: Phase B: remote sub-address missing!\n", ch);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_polled_sub_address(s)))
    {
        printf("%c: Phase B: remote polled sub-address '%s'\n", ch, u);
        if (expected_rx_info.polled_sub_address[0]  &&  strcmp(expected_rx_info.polled_sub_address, u))
        {
            printf("%c: Phase B: remote polled sub-address incorrect! - expected '%s'\n", ch, expected_rx_info.polled_sub_address);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info.polled_sub_address[0])
        {
            printf("%c: Phase B: remote polled sub-address missing!\n", ch);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_selective_polling_address(s)))
    {
        printf("%c: Phase B: remote selective polling address '%s'\n", ch, u);
        if (expected_rx_info.selective_polling_address[0]  &&  strcmp(expected_rx_info.selective_polling_address, u))
        {
            printf("%c: Phase B: remote selective polling address incorrect! - expected '%s'\n", ch, expected_rx_info.selective_polling_address);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info.selective_polling_address[0])
        {
            printf("%c: Phase B: remote selective polling address missing!\n", ch);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sender_ident(s)))
    {
        printf("%c: Phase B: remote sender ident '%s'\n", ch, u);
        if (expected_rx_info.sender_ident[0]  &&  strcmp(expected_rx_info.sender_ident, u))
        {
            printf("%c: Phase B: remote sender ident incorrect! - expected '%s'\n", ch, expected_rx_info.sender_ident);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info.sender_ident[0])
        {
            printf("%c: Phase B: remote sender ident missing!\n", ch);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_password(s)))
    {
        printf("%c: Phase B: remote password '%s'\n", ch, u);
        if (expected_rx_info.password[0]  &&  strcmp(expected_rx_info.password, u))
        {
            printf("%c: Phase B: remote password incorrect! - expected '%s'\n", ch, expected_rx_info.password);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    else
    {
        if (expected_rx_info.password[0])
        {
            printf("%c: Phase B: remote password missing!\n", ch);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    printf("%c: Phase B handler on channel %d - (0x%X) %s\n", ch, ch, result, t30_frametype(result));
    return status;
}
/*- End of function --------------------------------------------------------*/

static int phase_d_handler(void *user_data, int result)
{
    int i;
    int ch;
    t30_state_t *s;
    char tag[20];

    i = 0;
    s = (t30_state_t *) user_data;
    ch = i + 'A';
    snprintf(tag, sizeof(tag), "%c: Phase D", ch);
    printf("%c: Phase D handler on channel %c - (0x%X) %s\n", ch, ch, result, t30_frametype(result));
    fax_log_page_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);

    if (use_receiver_not_ready)
        t30_set_receiver_not_ready(s, 3);

    if (test_local_interrupt)
    {
        if (i == 0)
        {
            printf("%c: Initiating interrupt request\n", ch);
            t30_local_interrupt_request(s, true);
        }
        else
        {
            switch (result)
            {
            case T30_PIP:
            case T30_PRI_MPS:
            case T30_PRI_EOM:
            case T30_PRI_EOP:
                printf("%c: Accepting interrupt request\n", ch);
                t30_local_interrupt_request(s, true);
                break;
            case T30_PIN:
                break;
            }
        }
    }
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void phase_e_handler(void *user_data, int result)
{
    int ch;
    t30_state_t *s;
    char tag[20];

    ch = 'A';
    s = (t30_state_t *) user_data;
    snprintf(tag, sizeof(tag), "%c: Phase E", ch);
    printf("%c: Phase E handler on channel %c - (%d) %s\n", ch, ch, result, t30_completion_code_to_str(result));
    fax_log_final_transfer_statistics(s, tag);
    fax_log_tx_parameters(s, tag);
    fax_log_rx_parameters(s, tag);
}
/*- End of function --------------------------------------------------------*/

static void t30_real_time_frame_handler(void *user_data,
                                        bool incoming,
                                        const uint8_t *msg,
                                        int len)
{
    if (msg == NULL)
    {
    }
    else
    {
        fprintf(stderr,
                "T.30: Real time frame handler - %s, %s, length = %d\n",
                (incoming)  ?  "line->T.30"  : "T.30->line",
                t30_frametype(msg[2]),
                len);
    }
}
/*- End of function --------------------------------------------------------*/

static int document_handler(void *user_data, int event)
{
    int ch;
    t30_state_t *s;

    ch = 'A';
    s = (t30_state_t *) user_data;
    fprintf(stderr, "%c: Document handler on channel %c - event %d\n", ch, ch, event);
    if (next_tx_file[0])
    {
        t30_set_tx_file(s, next_tx_file, -1, -1);
        next_tx_file[0] = '\0';
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static void faxtester_real_time_frame_handler(faxtester_state_t *s,
                                              void *user_data,
                                              int direction,
                                              const uint8_t *msg,
                                              int len)
{
    if (msg == NULL)
    {
        while (next_step(s) == 0)
            ;
        /*endwhile*/
    }
    else
    {
        fprintf(stderr,
                "TST: Real time frame handler - %s, %s, length = %d\n",
                (direction)  ?  "line->tester"  : "tester->line",
                t30_frametype(msg[2]),
                len);
        if (direction  &&  msg[1] == awaited[1])
        {
            if ((awaited_len >= 0  &&  len != abs(awaited_len))
                ||
                (awaited_len < 0  &&  len < abs(awaited_len))
                ||
                memcmp(msg, awaited, abs(awaited_len)) != 0)
            {
                span_log_buf(&s->logging, SPAN_LOG_FLOW, "Expected", awaited, abs(awaited_len));
                span_log_buf(&s->logging, SPAN_LOG_FLOW, "Received", msg, len);
                printf("Test failed\n");
                exit(2);
            }
        }
        if (msg[1] == awaited[1])
        {
            while (next_step(s) == 0)
                ;
            /*endwhile*/
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void faxtester_front_end_step_complete_handler(faxtester_state_t *s, void *user_data)
{
    while (next_step(s) == 0)
        ;
    /*endwhile*/
}
/*- End of function --------------------------------------------------------*/

static void faxtester_front_end_step_timeout_handler(faxtester_state_t *s, void *user_data)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "FAX tester step timed out\n");
    printf("Test failed\n");
    exit(2);
}
/*- End of function --------------------------------------------------------*/

static void fax_prepare(void)
{
    t30_state_t *t30;
    logging_state_t *logging;

    t30 = fax_get_t30_state(fax);
    fax_set_transmit_on_idle(fax, true);
    fax_set_tep_mode(fax, true);
#if 0
    t30_set_tx_ident(t30, "1234567890");
    t30_set_tx_sub_address(t30, "Sub-address");
    t30_set_tx_sender_ident(t30, "Sender ID");
    t30_set_tx_password(t30, "Password");
    t30_set_tx_polled_sub_address(t30, "Polled sub-address");
    t30_set_tx_selective_polling_address(t30, "Sel polling address");
#endif
    t30_set_tx_nsf(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp NSF\x00", 16);
    //t30_set_tx_nss(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp NSS\x00", 16);
    t30_set_tx_nsc(t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp NSC\x00", 16);
    t30_set_ecm_capability(t30, true);
    t30_set_supported_t30_features(t30,
                                   T30_SUPPORT_IDENTIFICATION
                                 | T30_SUPPORT_SELECTIVE_POLLING
                                 | T30_SUPPORT_SUB_ADDRESSING);
    t30_set_supported_image_sizes(t30,
                                  T4_SUPPORT_WIDTH_215MM
                                | T4_SUPPORT_WIDTH_255MM
                                | T4_SUPPORT_WIDTH_303MM
                                | T4_SUPPORT_LENGTH_US_LETTER
                                | T4_SUPPORT_LENGTH_US_LEGAL
                                | T4_SUPPORT_LENGTH_UNLIMITED);
    t30_set_supported_bilevel_resolutions(t30,
                                          T4_RESOLUTION_R8_STANDARD
                                        | T4_RESOLUTION_R8_FINE
                                        | T4_RESOLUTION_R8_SUPERFINE
                                        | T4_RESOLUTION_R16_SUPERFINE
                                        | T4_RESOLUTION_100_100
                                        | T4_RESOLUTION_200_100
                                        | T4_RESOLUTION_200_200
                                        | T4_RESOLUTION_200_400
                                        | T4_RESOLUTION_300_300
                                        | T4_RESOLUTION_300_600
                                        | T4_RESOLUTION_400_400
                                        | T4_RESOLUTION_400_800
                                        | T4_RESOLUTION_600_600
                                        | T4_RESOLUTION_600_1200
                                        | T4_RESOLUTION_1200_1200);
    t30_set_supported_colour_resolutions(t30, 0);
    t30_set_supported_modems(t30, T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17);
    t30_set_supported_compressions(t30, T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6);
    t30_set_phase_b_handler(t30, phase_b_handler, (void *) t30);
    t30_set_phase_d_handler(t30, phase_d_handler, (void *) t30);
    t30_set_phase_e_handler(t30, phase_e_handler, (void *) t30);
    t30_set_real_time_frame_handler(t30, t30_real_time_frame_handler, (void *) t30);
    t30_set_document_handler(t30, document_handler, (void *) t30);

    logging = fax_get_logging_state(fax);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "A");

    logging = t30_get_logging_state(t30);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "A");

#if 0
    span_log_set_level(&fax.modems.v27ter_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.modems.v27ter_rx.logging, "A");
    span_log_set_level(&fax.modems.v29_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.modems.v29_rx.logging, "A");
    span_log_set_level(&fax.modems.v17_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.modems.v17_rx.logging, "A");
#endif
}
/*- End of function --------------------------------------------------------*/

static int string_to_msg(uint8_t msg[], uint8_t mask[], const char buf[])
{
    int i;
    int x;
    const char *t;

    msg[0] = 0;
    mask[0] = 0xFF;
    i = 0;
    t = (char *) buf;
    while (*t)
    {
        /* Skip white space */
        while (isspace((int) *t))
            t++;
        /* If we find ... we allow arbitrary addition info beyond this point in the message */
        if (t[0] == '.'  &&  t[1] == '.'  &&  t[2] == '.')
        {
            return -i;
        }
        else if (isxdigit((int) *t))
        {
            for (  ;  isxdigit((int) *t);  t++)
            {
                x = *t;
                if (x >= 'a')
                    x -= 0x20;
                if (x >= 'A')
                    x -= ('A' - 10);
                else
                    x -= '0';
                msg[i] = (msg[i] << 4) | x;
            }
            mask[i] = 0xFF;
            if (*t == '/')
            {
                /* There is a mask following the byte */
                mask[i] = 0;
                for (t++;  isxdigit((int) *t);  t++)
                {
                    x = *t;
                    if (x >= 'a')
                        x -= 0x20;
                    if (x >= 'A')
                        x -= ('A' - 10);
                    else
                        x -= '0';
                    mask[i] = (mask[i] << 4) | x;
                }
            }
            if (*t  &&  !isspace((int) *t))
            {
                /* Bad string */
                return 0;
            }
            i++;
        }
    }
    return i;
}
/*- End of function --------------------------------------------------------*/

#if 0
static void string_test2(const uint8_t msg[], int len)
{
    int i;

    if (len > 0)
    {
        for (i = 0;  i < len - 1;  i++)
            printf("%02X ", msg[i]);
        printf("%02X", msg[i]);
    }
}
/*- End of function --------------------------------------------------------*/

static void string_test3(const char buf[])
{
    uint8_t msg[1000];
    uint8_t mask[1000];
    int len;
    int i;

    len = string_to_msg(msg, mask, buf);
    printf("Len = %d: ", len);
    string_test2(msg, abs(len));
    printf("/");
    string_test2(mask, abs(len));
    printf("\n");
}
/*- End of function --------------------------------------------------------*/

static int string_test(void)
{
    string_test3("FF C8 12 34 56 78");
    string_test3("FF C8 12/55 34 56/aA 78 ");
    string_test3("FF C8 12/55 34 56/aA 78 ...");
    string_test3("FF C8 12/55 34 56/aA 78...");
    string_test3("FF C8 12/55 34 56/aA 78 ... 99 88 77");
    exit(0);
}
/*- End of function --------------------------------------------------------*/
#endif

static void corrupt_image(faxtester_state_t *s, uint8_t image[], int len, const char *bad_rows)
{
    int i;
    int j;
    int k;
    uint32_t bits;
    uint32_t bitsx;
    int list[1000];
    int x;
    int row;
    const char *t;

    /* Form the list of rows to be hit */
    x = 0;
    t = bad_rows;
    while (*t)
    {
        while (isspace((int) *t))
            t++;
        if (sscanf(t, "%d", &list[x]) < 1)
            break;
        x++;
        while (isdigit((int) *t))
            t++;
        if (*t == ',')
            t++;
    }

    /* Go through the image, and corrupt the first bit of every listed row */
    bits = 0x7FF;
    bitsx = 0x7FF;
    row = 0;
    for (i = 0;  i < len;  i++)
    {
        bits ^= (image[i] << 11);
        bitsx ^= (image[i] << 11);
        for (j = 0;  j < 8;  j++)
        {
            if ((bits & 0xFFF) == 0x800)
            {
                /* We are at an EOL. Is this row in the list of rows to be corrupted? */
                row++;
                for (k = 0;  k < x;  k++)
                {
                    if (list[k] == row)
                    {
                        /* Corrupt this row. TSB85 says to hit the first bit after the EOL */
                        bitsx ^= 0x1000;
                    }
                }
            }
            bits >>= 1;
            bitsx >>= 1;
        }
        image[i] = (bitsx >> 3) & 0xFF;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "%d rows found. %d corrupted\n", row, x);
}
/*- End of function --------------------------------------------------------*/

static int next_step(faxtester_state_t *s)
{
    int delay;
    int flags;
    xmlChar *dir;
    xmlChar *type;
    xmlChar *modem;
    xmlChar *value;
    xmlChar *tag;
    xmlChar *bad_rows;
    xmlChar *crc_error;
    xmlChar *pattern;
    xmlChar *timein;
    xmlChar *timeout;
    xmlChar *min_bits;
    xmlChar *frame_size;
    xmlChar *block;
    xmlChar *compression;
    uint8_t buf[1000];
    uint8_t mask[1000];
    char path[1024];
    int i;
    int j;
    int hdlc;
    int short_train;
    int min_row_bits;
    int ecm_frame_size;
    int ecm_block;
    int compression_type;
    int len;
    t4_tx_state_t t4_tx_state;
    t30_state_t *t30;
    t30_stats_t t30_stats;

    test_for_call_clear = false;
    if (s->cur == NULL)
    {
        if (!s->final_delayed)
        {
            /* Add a bit of waiting at the end, to ensure everything gets flushed through,
               any timers can expire, etc. */
            faxtester_set_timeout(s, -1);
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_PAUSE, 0, 120000, false);
            s->final_delayed = true;
            return 1;
        }
        /* Finished */
        printf("Test passed\n");
        exit(0);
    }
    while (s->cur  &&  xmlStrcmp(s->cur->name, (const xmlChar *) "step") != 0)
        s->cur = s->cur->next;
    if (s->cur == NULL)
    {
        /* Finished */
        printf("Test passed\n");
        exit(0);
    }

    dir = xmlGetProp(s->cur, (const xmlChar *) "dir");
    type = xmlGetProp(s->cur, (const xmlChar *) "type");
    modem = xmlGetProp(s->cur, (const xmlChar *) "modem");
    value = xmlGetProp(s->cur, (const xmlChar *) "value");
    tag = xmlGetProp(s->cur, (const xmlChar *) "tag");
    bad_rows = xmlGetProp(s->cur, (const xmlChar *) "bad_rows");
    crc_error = xmlGetProp(s->cur, (const xmlChar *) "crc_error");
    pattern = xmlGetProp(s->cur, (const xmlChar *) "pattern");
    timein = xmlGetProp(s->cur, (const xmlChar *) "timein");
    timeout = xmlGetProp(s->cur, (const xmlChar *) "timeout");
    min_bits = xmlGetProp(s->cur, (const xmlChar *) "min_bits");
    frame_size = xmlGetProp(s->cur, (const xmlChar *) "frame_size");
    block = xmlGetProp(s->cur, (const xmlChar *) "block");
    compression = xmlGetProp(s->cur, (const xmlChar *) "compression");

    s->cur = s->cur->next;

    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "Dir - %s, type - %s, modem - %s, value - %s, timein - %s, timeout - %s, tag - %s\n",
             (dir)  ?  (const char *) dir  :  "",
             (type)  ?  (const char *) type  :  "",
             (modem)  ?  (const char *) modem  :  "",
             (value)  ?  (const char *) value  :  "",
             (timein)  ?  (const char *) timein  :  "",
             (timeout)  ?  (const char *) timeout  :  "",
             (tag)  ?  (const char *) tag  :  "");
    if (type == NULL)
        return 1;
    if (timein)
        timein_x = atoi((const char *) timein);
    else
        timein_x = -1;
    if (timeout)
        timeout_x = atoi((const char *) timeout);
    else
        timeout_x = -1;

    if (dir  &&  strcasecmp((const char *) dir, "R") == 0)
    {
        /* Receive always has a timeout applied. */
        if (timeout_x < 0)
            timeout_x = 7000;
        faxtester_set_timeout(s, timeout_x);
        if (modem)
        {
            hdlc = (strcasecmp((const char *) type, "PREAMBLE") == 0);
            short_train = (strcasecmp((const char *) type, "TCF") != 0);
            faxtester_set_tx_type(s, T30_MODEM_NONE, 0, false, false);
            if (strcasecmp((const char *) modem, "V.21") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V21, 300, false, true);
            }
            else if (strcasecmp((const char *) modem, "V.17/14400") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 14400, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/12000") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 12000, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/9600") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 9600, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/7200") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 7200, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.29/9600") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V29, 9600, false, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.29/7200") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V29, 7200, false, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.27ter/4800") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V27TER, 4800, false, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.27ter/2400") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V27TER, 2400, false, hdlc);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised modem\n");
            }
        }

        if (strcasecmp((const char *) type, "SET") == 0)
        {
            if (strcasecmp((const char *) tag, "IDENT") == 0)
                strcpy(expected_rx_info.ident, (const char *) value);
            else if (strcasecmp((const char *) tag, "SUB") == 0)
                strcpy(expected_rx_info.sub_address, (const char *) value);
            else if (strcasecmp((const char *) tag, "SEP") == 0)
                strcpy(expected_rx_info.selective_polling_address, (const char *) value);
            else if (strcasecmp((const char *) tag, "PSA") == 0)
                strcpy(expected_rx_info.polled_sub_address, (const char *) value);
            else if (strcasecmp((const char *) tag, "SID") == 0)
                strcpy(expected_rx_info.sender_ident, (const char *) value);
            else if (strcasecmp((const char *) tag, "PWD") == 0)
                strcpy(expected_rx_info.password, (const char *) value);
            return 0;
        }
        else if (strcasecmp((const char *) type, "CNG") == 0)
        {
            /* Look for CNG */
            faxtester_set_rx_type(s, T30_MODEM_CNG, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_NONE, 0, false, false);
        }
        else if (strcasecmp((const char *) type, "CED") == 0)
        {
            /* Look for CED */
            faxtester_set_rx_type(s, T30_MODEM_CED, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_NONE, 0, false, false);
        }
        else if (strcasecmp((const char *) type, "HDLC") == 0)
        {
            i = string_to_msg(buf, mask, (const char *) value);
            bit_reverse(awaited, buf, abs(i));
            awaited_len = i;
        }
        else if (strcasecmp((const char *) type, "TCF") == 0)
        {
        }
        else if (strcasecmp((const char *) type, "MSG") == 0)
        {
        }
        else if (strcasecmp((const char *) type, "PP") == 0)
        {
        }
        else if (strcasecmp((const char *) type, "SILENCE") == 0)
        {
            faxtest_set_rx_silence(s);
        }
        else if (strcasecmp((const char *) type, "CLEAR") == 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Far end should drop the call\n");
            test_for_call_clear = true;
            call_clear_timer = 0;
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised type '%s'\n", (const char *) type);
            return 0;
        }
    }
    else
    {
        faxtester_set_timeout(s, timeout_x);
        if (modem)
        {
            hdlc = (strcasecmp((const char *) type, "PREAMBLE") == 0);
            short_train = (strcasecmp((const char *) type, "TCF") != 0);
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            if (strcasecmp((const char *) modem, "V.21") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V21, 300, false, true);
            }
            else if (strcasecmp((const char *) modem, "V.17/14400") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 14400, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/12000") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 12000, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/9600") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 9600, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.17/7200") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 7200, short_train, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.29/9600") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V29, 9600, false, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.29/7200") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V29, 7200, false, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.27ter/4800") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V27TER, 4800, false, hdlc);
            }
            else if (strcasecmp((const char *) modem, "V.27ter/2400") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V27TER, 2400, false, hdlc);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised modem\n");
            }
        }

        if (strcasecmp((const char *) type, "SET") == 0)
        {
            t30 = fax_get_t30_state(fax);
            if (strcasecmp((const char *) tag, "IDENT") == 0)
                t30_set_tx_ident(t30, (const char *) value);
            else if (strcasecmp((const char *) tag, "SUB") == 0)
                t30_set_tx_sub_address(t30, (const char *) value);
            else if (strcasecmp((const char *) tag, "SEP") == 0)
                t30_set_tx_selective_polling_address(t30, (const char *) value);
            else if (strcasecmp((const char *) tag, "PSA") == 0)
                t30_set_tx_polled_sub_address(t30, (const char *) value);
            else if (strcasecmp((const char *) tag, "SID") == 0)
                t30_set_tx_sender_ident(t30, (const char *) value);
            else if (strcasecmp((const char *) tag, "PWD") == 0)
                t30_set_tx_password(t30, (const char *) value);
            else if (strcasecmp((const char *) tag, "RXFILE") == 0)
            {
                if (value)
                    t30_set_rx_file(t30, (const char *) value, -1);
                else
                    t30_set_rx_file(t30, output_tiff_file_name, -1);
            }
            else if (strcasecmp((const char *) tag, "TXFILE") == 0)
            {
                sprintf(next_tx_file, "%s/%s", image_path, (const char *) value);
                printf("Push '%s'\n", next_tx_file);
            }
            return 0;
        }
        else if (strcasecmp((const char *) type, "CALL") == 0)
        {
            fax = fax_init(NULL, false);
            fax_prepare();
            next_tx_file[0] = '\0';
            t30 = fax_get_t30_state(fax);
            t30_set_rx_file(t30, output_tiff_file_name, -1);
            /* Avoid libtiff 3.8.2 and earlier bug on complex 2D lines. */
            t30_set_supported_output_compressions(t30, T4_COMPRESSION_T4_1D);
            if (value)
            {
                sprintf(path, "%s/%s", image_path, (const char *) value);
                t30_set_tx_file(t30, path, -1, -1);
            }
            return 0;
        }
        else if (strcasecmp((const char *) type, "ANSWER") == 0)
        {
            fax = fax_init(NULL, true);
            fax_prepare();
            next_tx_file[0] = '\0';
            t30 = fax_get_t30_state(fax);
            /* Avoid libtiff 3.8.2 and earlier bug on complex 2D lines. */
            t30_set_supported_output_compressions(t30, T4_COMPRESSION_T4_1D);
            if (value)
            {
                sprintf(path, "%s/%s", image_path, (const char *) value);
                t30_set_tx_file(t30, path, -1, -1);
            }
            return 0;
        }
        else if (strcasecmp((const char *) type, "CNG") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_CNG, 0, false, false);
        }
        else if (strcasecmp((const char *) type, "CED") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_CED, 0, false, false);
        }
        else if (strcasecmp((const char *) type, "WAIT") == 0)
        {
            delay = (value)  ?  atoi((const char *) value)  :  1;
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_PAUSE, 0, delay, false);
        }
        else if (strcasecmp((const char *) type, "PREAMBLE") == 0)
        {
            flags = (value)  ?  atoi((const char *) value)  :  37;
            faxtester_send_hdlc_flags(s, flags);
        }
        else if (strcasecmp((const char *) type, "POSTAMBLE") == 0)
        {
            flags = (value)  ?  atoi((const char *) value)  :  5;
            faxtester_send_hdlc_flags(s, flags);
        }
        else if (strcasecmp((const char *) type, "HDLC") == 0)
        {
            i = string_to_msg(buf, mask, (const char *) value);
            bit_reverse(buf, buf, abs(i));
            if (crc_error  &&  strcasecmp((const char *) crc_error, "0") == 0)
                faxtester_send_hdlc_msg(s, buf, abs(i), false);
            else
                faxtester_send_hdlc_msg(s, buf, abs(i), true);
        }
        else if (strcasecmp((const char *) type, "TCF") == 0)
        {
            if (value)
                i = atoi((const char *) value);
            else
                i = 450;
            if (pattern)
            {
                /* TODO: implement proper patterns */
                j = atoi((const char *) pattern);
                memset(image, 0x55, j);
                if (i > j)
                    memset(image + j, 0, i - j);
            }
            else
            {
                memset(image, 0, i);
            }
            faxtester_set_non_ecm_image_buffer(s, image, i);
        }
        else if (strcasecmp((const char *) type, "MSG") == 0)
        {
            /* A non-ECM page */
            min_row_bits = (min_bits)  ?  atoi((const char *) min_bits)  :  0;
            sprintf(path, "%s/%s", image_path, (const char *) value);
            if (t4_tx_init(&t4_tx_state, path, -1, -1) == NULL)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to init T.4 send\n");
                printf("Test failed\n");
                exit(2);
            }
            t4_tx_set_header_info(&t4_tx_state, NULL);
            compression_type = T4_COMPRESSION_T4_1D;
            if (compression)
            {
                if (strcasecmp((const char *) compression, "T.4 2D") == 0)
                    compression_type = T4_COMPRESSION_T4_2D;
                else if (strcasecmp((const char *) compression, "T.6") == 0)
                    compression_type = T4_COMPRESSION_T6;
            }
            if (t4_tx_set_tx_image_format(&t4_tx_state,
                                          compression_type,
                                          T4_SUPPORT_WIDTH_215MM
                                        | T4_SUPPORT_LENGTH_US_LETTER
                                        | T4_SUPPORT_LENGTH_US_LEGAL
                                        | T4_SUPPORT_LENGTH_UNLIMITED,
                                          T4_RESOLUTION_R8_STANDARD
                                        | T4_RESOLUTION_R8_FINE
                                        | T4_RESOLUTION_R8_SUPERFINE
                                        | T4_RESOLUTION_R16_SUPERFINE
                                        | T4_RESOLUTION_200_100
                                        | T4_RESOLUTION_200_200
                                        | T4_RESOLUTION_200_400
                                        | T4_RESOLUTION_300_300
                                        | T4_RESOLUTION_300_600
                                        | T4_RESOLUTION_400_400
                                        | T4_RESOLUTION_400_800
                                        | T4_RESOLUTION_600_600
                                        | T4_RESOLUTION_600_1200
                                        | T4_RESOLUTION_1200_1200,
                                          T4_RESOLUTION_100_100
                                        | T4_RESOLUTION_200_200
                                        | T4_RESOLUTION_300_300
                                        | T4_RESOLUTION_400_400
                                        | T4_RESOLUTION_600_600
                                        | T4_RESOLUTION_1200_1200) < 0)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to set T.4 compression\n");
                printf("Test failed\n");
                exit(2);
            }
            t4_tx_set_min_bits_per_row(&t4_tx_state, min_row_bits);
            if (t4_tx_start_page(&t4_tx_state))
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to start T.4 send\n");
                printf("Test failed\n");
                exit(2);
            }
            len = t4_tx_get(&t4_tx_state, image, sizeof(image));
            if (bad_rows)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "We need to corrupt the image\n");
                corrupt_image(s, image, len, (const char *) bad_rows);
            }
            t4_tx_release(&t4_tx_state);
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM image is %d bytes (min row bits %d)\n", len, min_row_bits);
            faxtester_set_non_ecm_image_buffer(s, image, len);
        }
        else if (strcasecmp((const char *) type, "PP") == 0)
        {
            min_row_bits = (min_bits)  ?  atoi((const char *) min_bits)  :  0;
            ecm_block = (block)  ?  atoi((const char *) block)  :  0;
            ecm_frame_size = (frame_size)  ?  atoi((const char *) frame_size)  :  64;
            i = (crc_error)  ?  atoi((const char *) crc_error)  :  -1;
            sprintf(path, "%s/%s", image_path, (const char *) value);
            if (t4_tx_init(&t4_tx_state, path, -1, -1) == NULL)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to init T.4 send\n");
                printf("Test failed\n");
                exit(2);
            }
            t4_tx_set_header_info(&t4_tx_state, NULL);
            compression_type = T4_COMPRESSION_T4_1D;
            if (compression)
            {
                if (strcasecmp((const char *) compression, "T.4 2D") == 0)
                    compression_type = T4_COMPRESSION_T4_2D;
                else if (strcasecmp((const char *) compression, "T.6") == 0)
                    compression_type = T4_COMPRESSION_T6;
            }
            if (t4_tx_set_tx_image_format(&t4_tx_state,
                                          compression_type,
                                          T4_SUPPORT_WIDTH_215MM
                                        | T4_SUPPORT_LENGTH_US_LETTER
                                        | T4_SUPPORT_LENGTH_US_LEGAL
                                        | T4_SUPPORT_LENGTH_UNLIMITED,
                                          T4_RESOLUTION_R8_STANDARD
                                        | T4_RESOLUTION_R8_FINE
                                        | T4_RESOLUTION_R8_SUPERFINE
                                        | T4_RESOLUTION_R16_SUPERFINE
                                        | T4_RESOLUTION_200_100
                                        | T4_RESOLUTION_200_200
                                        | T4_RESOLUTION_200_400
                                        | T4_RESOLUTION_300_300
                                        | T4_RESOLUTION_300_600
                                        | T4_RESOLUTION_400_400
                                        | T4_RESOLUTION_400_800
                                        | T4_RESOLUTION_600_600
                                        | T4_RESOLUTION_600_1200
                                        | T4_RESOLUTION_1200_1200,
                                          T4_RESOLUTION_100_100
                                        | T4_RESOLUTION_200_200
                                        | T4_RESOLUTION_300_300
                                        | T4_RESOLUTION_400_400
                                        | T4_RESOLUTION_600_600
                                        | T4_RESOLUTION_1200_1200) < 0)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to set T.4 compression\n");
                printf("Test failed\n");
                exit(2);
            }
            t4_tx_set_min_bits_per_row(&t4_tx_state, min_row_bits);
            if (t4_tx_start_page(&t4_tx_state))
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to start T.4 send\n");
                printf("Test failed\n");
                exit(2);
            }
            /*endif*/
            len = t4_tx_get(&t4_tx_state, image, sizeof(image));
            if (bad_rows)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "We need to corrupt the image\n");
                corrupt_image(s, image, len, (const char *) bad_rows);
            }
            /*endif*/
            t4_tx_release(&t4_tx_state);
            span_log(&s->logging, SPAN_LOG_FLOW, "ECM image is %d bytes (min row bits %d)\n", len, min_row_bits);
            faxtester_set_ecm_image_buffer(s, image, len, ecm_block, ecm_frame_size, i);
        }
        else if (strcasecmp((const char *) type, "CLEAR") == 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Time to drop the call\n");
            t30 = fax_get_t30_state(fax);
            t30_terminate(t30);
            return 0;
        }
        else if (strcasecmp((const char *) type, "STATUS") == 0)
        {
            if (value)
            {
                for (i = 0;  t30_status[i].code >= 0;  i++)
                {
                    if (strcmp(t30_status[i].tag, (const char *) value) == 0)
                        break;
                }
                if (t30_status[i].code >= 0)
                    delay = t30_status[i].code;
                else
                    delay = atoi((const char *) value);
                t30 = fax_get_t30_state(fax);
                t30_get_transfer_statistics(t30, &t30_stats);
                span_log(&s->logging, SPAN_LOG_FLOW, "Expect status %d. Got %d\n", delay, t30_stats.current_status);
                if (delay != t30_stats.current_status)
                {
                    printf("Test failed\n");
                    exit(2);
                }
            }
            return 0;
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised type '%s'\n", (const char *) type);
            return 0;
        }
        /*endif*/
    }
    /*endif*/
    return 1;
}
/*- End of function --------------------------------------------------------*/

static void exchange(faxtester_state_t *s)
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int len;
    int i;
    int total_audio_time;
    logging_state_t *logging;

    output_tiff_file_name = OUTPUT_TIFF_FILE_NAME;

    if (log_audio)
    {
        if ((out_handle = sf_open_telephony_write(OUTPUT_WAVE_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            printf("Test failed\n");
            exit(2);
        }
        /*endif*/
    }
    /*endif*/

    total_audio_time = 0;

    faxtester_set_transmit_on_idle(&state, true);
    faxtester_set_real_time_frame_handler(&state, faxtester_real_time_frame_handler, NULL);
    faxtester_set_front_end_step_complete_handler(&state, faxtester_front_end_step_complete_handler, NULL);
    faxtester_set_front_end_step_timeout_handler(&state, faxtester_front_end_step_timeout_handler, NULL);

    fax = fax_init(NULL, false);
    fax_prepare();
    next_tx_file[0] = '\0';

    while (next_step(s) == 0)
        ;
    /*endwhile*/
    for (;;)
    {
        len = fax_tx(fax, amp, SAMPLES_PER_CHUNK);
        faxtester_rx(s, amp, len);
        if (log_audio)
        {
            for (i = 0;  i < len;  i++)
                out_amp[2*i + 0] = amp[i];
            /*endfor*/
        }
        /*endif*/

        total_audio_time += SAMPLES_PER_CHUNK;

        logging = t30_get_logging_state(fax_get_t30_state(fax));
        span_log_bump_samples(logging, len);
#if 0
        span_log_bump_samples(&fax.modems.v27ter_rx.logging, len);
        span_log_bump_samples(&fax.modems.v29_rx.logging, len);
        span_log_bump_samples(&fax.modems.v17_rx.logging, len);
#endif
        logging = fax_get_logging_state(fax);
        span_log_bump_samples(logging, len);

        span_log_bump_samples(&s->logging, len);

        len = faxtester_tx(s, amp, SAMPLES_PER_CHUNK);
        if (fax_rx(fax, amp, len))
            break;
        /*endif*/
        if (log_audio)
        {
            for (i = 0;  i < len;  i++)
                out_amp[2*i + 1] = amp[i];
            /*endfor*/
            if (sf_writef_short(out_handle, out_amp, SAMPLES_PER_CHUNK) != SAMPLES_PER_CHUNK)
                break;
            /*endif*/
        }
        /*endif*/
        if (test_for_call_clear  &&  !far_end_cleared_call)
        {
            call_clear_timer += len;
            if (!t30_call_active(fax_get_t30_state(fax)))
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Far end cleared after %dms (limits %dms to %dms)\n", call_clear_timer/8, timein_x, timeout_x);
                if (call_clear_timer/8 < timein_x  ||  call_clear_timer/8 > timeout_x)
                {
                    printf("Test failed\n");
                    exit(2);
                }
                span_log(&s->logging, SPAN_LOG_FLOW, "Clear time OK\n");
                far_end_cleared_call = true;
                test_for_call_clear = false;
                while (next_step(s) == 0)
                    ;
                /*endwhile*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    if (log_audio)
    {
        if (sf_close_telephony(out_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            printf("Test failed\n");
            exit(2);
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

static int parse_config(faxtester_state_t *s, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur)
{
    xmlChar *x;
    xmlChar *y;

    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "path") == 0)
        {
            if ((x = xmlGetProp(cur, (const xmlChar *) "type"))
                &&
                (y = xmlGetProp(cur, (const xmlChar *) "value")))
            {
                if (strcasecmp((const char *) x, "IMAGE") == 0)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Found '%s' '%s'\n", (char *) x, (char *) y);
                    strcpy(image_path, (const char *) y);
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int parse_test_group(faxtester_state_t *s, xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr cur, const char *test)
{
    xmlChar *x;

    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "test") == 0)
        {
            if ((x = xmlGetProp(cur, (const xmlChar *) "name")))
            {
                if (xmlStrcmp(x, (const xmlChar *) test) == 0)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Found '%s'\n", (char *) x);
                    s->cur = cur->xmlChildrenNode;
                    return 0;
                }
                /*endif*/
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int get_test_set(faxtester_state_t *s, const char *test_file, const char *test)
{
    xmlParserCtxtPtr ctxt;
    xmlNsPtr ns;
    xmlNodePtr cur;

    ns = NULL;
    xmlKeepBlanksDefault(0);
    xmlCleanupParser();

    if ((ctxt = xmlNewParserCtxt()) == NULL)
    {
        fprintf(stderr, "Failed to allocate parser context\n");
        printf("Test failed\n");
        exit(2);
    }
    /* parse the file, activating the DTD validation option */
    if ((s->doc = xmlCtxtReadFile(ctxt, test_file, NULL, XML_PARSE_XINCLUDE | XML_PARSE_DTDVALID)) == NULL)
    {
        fprintf(stderr, "Failed to read the XML document\n");
        printf("Test failed\n");
        exit(2);
    }
    if (ctxt->valid == 0)
    {
        fprintf(stderr, "Failed to validate the XML document\n");
    	xmlFreeDoc(s->doc);
        xmlFreeParserCtxt(ctxt);
        printf("Test failed\n");
        exit(2);
    }
    xmlFreeParserCtxt(ctxt);

    /* Check the document is of the right kind */
    if ((cur = xmlDocGetRootElement(s->doc)) == NULL)
    {
        xmlFreeDoc(s->doc);
        fprintf(stderr, "Empty document\n");
        printf("Test failed\n");
        exit(2);
    }
    /*endif*/
    if (xmlStrcmp(cur->name, (const xmlChar *) "fax-tests"))
    {
        xmlFreeDoc(s->doc);
        fprintf(stderr, "Document of the wrong type, root node != fax-tests");
        printf("Test failed\n");
        exit(2);
    }
    /*endif*/
    cur = cur->xmlChildrenNode;
    while (cur  &&  xmlIsBlankNode(cur))
        cur = cur->next;
    /*endwhile*/
    if (cur == NULL)
    {
        printf("Test failed\n");
        exit(2);
    }
    /*endif*/
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "config") == 0)
        {
            parse_config(s, s->doc, ns, cur->xmlChildrenNode);
        }
        /*endif*/
        if (xmlStrcmp(cur->name, (const xmlChar *) "test-group") == 0)
        {
            if (parse_test_group(s, s->doc, ns, cur->xmlChildrenNode, test) == 0)
            {
                /* We found the test we want, so run it. */
                exchange(s);
                break;
            }
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    xmlFreeDoc(s->doc);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    const char *xml_file_name;
    const char *test_name;
    int opt;

#if 0
    string_test();
#endif

    xml_file_name = "../spandsp/tsb85.xml";
    test_name = "MRGN01";
    log_audio = false;
    while ((opt = getopt(argc, argv, "lx:")) != -1)
    {
        switch (opt)
        {
        case 'l':
            log_audio = true;
            break;
        case 'x':
            xml_file_name = optarg;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    argc -= optind;
    argv += optind;
    if (argc > 0)
        test_name = argv[0];

    strcpy(image_path, ".");
    faxtester_init(&state, true);
    memset(&expected_rx_info, 0, sizeof(expected_rx_info));
    span_log_set_level(&state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&state.logging, "B");
    get_test_set(&state, xml_file_name, test_name);
    faxtester_release(&state);
    printf("Done\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
