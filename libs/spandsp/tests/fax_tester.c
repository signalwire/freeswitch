/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_tester.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#include "fax_utils.h"
#include "fax_tester.h"

#define HDLC_FRAMING_OK_THRESHOLD       5

extern const char *output_tiff_file_name;

struct xml_node_parms_s
{
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
};

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

static void timer_update(faxtester_state_t *s, int len)
{
    s->timer += len;
    if (s->timer > s->timeout)
    {
        s->timeout = 0x7FFFFFFFFFFFFFFFLL;
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX tester step timed out\n");
        printf("Test failed\n");
        exit(2);
    }
}
/*- End of function --------------------------------------------------------*/

static void front_end_step_complete(faxtester_state_t *s)
{
    while (faxtester_next_step(s) == 0)
        ;
    /*endwhile*/
}
/*- End of function --------------------------------------------------------*/

static int faxtester_phase_b_handler(void *user_data, int result)
{
    int ch;
    int status;
    faxtester_state_t *s;
    const char *u;

    s = (faxtester_state_t *) user_data;
    ch = s->far_tag;
    status = T30_ERR_OK;
    if ((u = t30_get_rx_ident(s->far_t30)))
    {
        printf("%c: Phase B: remote ident '%s'\n", ch, u);
        if (s->expected_rx_info.ident[0]  &&  strcmp(s->expected_rx_info.ident, u))
        {
            printf("%c: Phase B: remote ident incorrect! - expected '%s'\n", ch, s->expected_rx_info.ident);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    else
    {
        if (s->expected_rx_info.ident[0])
        {
            printf("%c: Phase B: remote ident missing!\n", ch);
            status = T30_ERR_IDENT_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sub_address(s->far_t30)))
    {
        printf("%c: Phase B: remote sub-address '%s'\n", ch, u);
        if (s->expected_rx_info.sub_address[0]  &&  strcmp(s->expected_rx_info.sub_address, u))
        {
            printf("%c: Phase B: remote sub-address incorrect! - expected '%s'\n", ch, s->expected_rx_info.sub_address);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    else
    {
        if (s->expected_rx_info.sub_address[0])
        {
            printf("%c: Phase B: remote sub-address missing!\n", ch);
            status = T30_ERR_SUB_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_polled_sub_address(s->far_t30)))
    {
        printf("%c: Phase B: remote polled sub-address '%s'\n", ch, u);
        if (s->expected_rx_info.polled_sub_address[0]  &&  strcmp(s->expected_rx_info.polled_sub_address, u))
        {
            printf("%c: Phase B: remote polled sub-address incorrect! - expected '%s'\n", ch, s->expected_rx_info.polled_sub_address);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    else
    {
        if (s->expected_rx_info.polled_sub_address[0])
        {
            printf("%c: Phase B: remote polled sub-address missing!\n", ch);
            status = T30_ERR_PSA_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_selective_polling_address(s->far_t30)))
    {
        printf("%c: Phase B: remote selective polling address '%s'\n", ch, u);
        if (s->expected_rx_info.selective_polling_address[0]  &&  strcmp(s->expected_rx_info.selective_polling_address, u))
        {
            printf("%c: Phase B: remote selective polling address incorrect! - expected '%s'\n", ch, s->expected_rx_info.selective_polling_address);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    else
    {
        if (s->expected_rx_info.selective_polling_address[0])
        {
            printf("%c: Phase B: remote selective polling address missing!\n", ch);
            status = T30_ERR_SEP_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_sender_ident(s->far_t30)))
    {
        printf("%c: Phase B: remote sender ident '%s'\n", ch, u);
        if (s->expected_rx_info.sender_ident[0]  &&  strcmp(s->expected_rx_info.sender_ident, u))
        {
            printf("%c: Phase B: remote sender ident incorrect! - expected '%s'\n", ch, s->expected_rx_info.sender_ident);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    else
    {
        if (s->expected_rx_info.sender_ident[0])
        {
            printf("%c: Phase B: remote sender ident missing!\n", ch);
            status = T30_ERR_SID_UNACCEPTABLE;
        }
    }
    if ((u = t30_get_rx_password(s->far_t30)))
    {
        printf("%c: Phase B: remote password '%s'\n", ch, u);
        if (s->expected_rx_info.password[0]  &&  strcmp(s->expected_rx_info.password, u))
        {
            printf("%c: Phase B: remote password incorrect! - expected '%s'\n", ch, s->expected_rx_info.password);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    else
    {
        if (s->expected_rx_info.password[0])
        {
            printf("%c: Phase B: remote password missing!\n", ch);
            status = T30_ERR_PWD_UNACCEPTABLE;
        }
    }
    printf("%c: Phase B handler on channel %c - (0x%X) %s\n", ch, ch, result, t30_frametype(result));
    return status;
}
/*- End of function --------------------------------------------------------*/

static int faxtester_phase_d_handler(void *user_data, int result)
{
    int i;
    int ch;
    faxtester_state_t *s;
    char tag[20];

    s = (faxtester_state_t *) user_data;
    ch = s->far_tag;
    i = 0;
    snprintf(tag, sizeof(tag), "%c: Phase D", ch);
    printf("%c: Phase D handler on channel %c - (0x%X) %s\n", ch, ch, result, t30_frametype(result));
    fax_log_page_transfer_statistics(s->far_t30, tag);
    fax_log_tx_parameters(s->far_t30, tag);
    fax_log_rx_parameters(s->far_t30, tag);

    if (s->use_receiver_not_ready)
        t30_set_receiver_not_ready(s->far_t30, 3);

    if (s->test_local_interrupt)
    {
        if (i == 0)
        {
            printf("%c: Initiating interrupt request\n", ch);
            t30_local_interrupt_request(s->far_t30, true);
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
                t30_local_interrupt_request(s->far_t30, true);
                break;
            case T30_PIN:
                break;
            }
        }
    }
    return T30_ERR_OK;
}
/*- End of function --------------------------------------------------------*/

static void faxtester_phase_e_handler(void *user_data, int result)
{
    int ch;
    faxtester_state_t *s;
    char tag[20];

    s = (faxtester_state_t *) user_data;
    ch = s->far_tag;
    snprintf(tag, sizeof(tag), "%c: Phase E", ch);
    printf("%c: Phase E handler on channel %c - (%d) %s\n", ch, ch, result, t30_completion_code_to_str(result));
    fax_log_final_transfer_statistics(s->far_t30, tag);
    fax_log_tx_parameters(s->far_t30, tag);
    fax_log_rx_parameters(s->far_t30, tag);
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

static int faxtester_document_handler(void *user_data, int event)
{
    int ch;
    faxtester_state_t *s;
    t30_state_t *t;

    s = (faxtester_state_t *) user_data;
    ch = s->far_tag;
    t = s->far_t30;
    fprintf(stderr, "%c: Document handler on channel %c - event %d\n", ch, ch, event);
    if (s->next_tx_file[0])
    {
        t30_set_tx_file(t, s->next_tx_file, -1, -1);
        s->next_tx_file[0] = '\0';
        return true;
    }
    return false;
}
/*- End of function --------------------------------------------------------*/

static void faxtester_real_time_frame_handler(faxtester_state_t *s,
                                              int direction,
                                              const uint8_t *msg,
                                              int len)
{
    if (msg == NULL)
    {
        while (faxtester_next_step(s) == 0)
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
        if (direction  &&  msg[1] == s->awaited[1])
        {
            if ((s->awaited_len >= 0  &&  len != abs(s->awaited_len))
                ||
                (s->awaited_len < 0  &&  len < abs(s->awaited_len))
                ||
                memcmp(msg, s->awaited, abs(s->awaited_len)) != 0)
            {
                span_log_buf(&s->logging, SPAN_LOG_FLOW, "Expected", s->awaited, abs(s->awaited_len));
                span_log_buf(&s->logging, SPAN_LOG_FLOW, "Received", msg, len);
                printf("Test failed\n");
                exit(2);
            }
        }
        if (msg[1] == s->awaited[1])
        {
            while (faxtester_next_step(s) == 0)
                ;
            /*endwhile*/
        }
    }
}
/*- End of function --------------------------------------------------------*/

void faxtester_send_hdlc_flags(faxtester_state_t *s, int flags)
{
    hdlc_tx_flags(&s->modems.hdlc_tx, flags);
}
/*- End of function --------------------------------------------------------*/

void faxtester_send_hdlc_msg(faxtester_state_t *s, const uint8_t *msg, int len, int crc_ok)
{
    hdlc_tx_frame(&s->modems.hdlc_tx, msg, len);
    if (!crc_ok)
        hdlc_tx_corrupt_frame(&s->modems.hdlc_tx);
}
/*- End of function --------------------------------------------------------*/

static void hdlc_underflow_handler(void *user_data)
{
    faxtester_state_t *s;
    uint8_t buf[400];

    s = (faxtester_state_t *) user_data;

    if (s->image_buffer)
    {
        /* We are sending an ECM image */
        if (s->image_ptr < s->image_len)
        {
            buf[0] = 0xFF;
            buf[1] = 0x03;
            buf[2] = 0x06;
            buf[3] = s->image_ptr/s->ecm_frame_size;
            memcpy(&buf[4], &s->image_buffer[s->image_ptr], s->ecm_frame_size);
            hdlc_tx_frame(&s->modems.hdlc_tx, buf, 4 + s->ecm_frame_size);
            if (s->corrupt_crc >= 0  &&  s->corrupt_crc == s->image_ptr/s->ecm_frame_size)
                hdlc_tx_corrupt_frame(&s->modems.hdlc_tx);
            s->image_ptr += s->ecm_frame_size;
            return;
        }
        /* The actual image is over. We are sending the final RCP frames. */
        if (s->image_bit_ptr > 2)
        {
            s->image_bit_ptr--;
            buf[0] = 0xFF;
            buf[1] = 0x03;
            buf[2] = 0x86;
            hdlc_tx_frame(&s->modems.hdlc_tx, buf, 3);
            return;
        }
        /* All done. */
        s->image_buffer = NULL;
    }
    front_end_step_complete(s);
}
/*- End of function --------------------------------------------------------*/

static void modem_tx_status(void *user_data, int status)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    printf("Tx status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_SHUTDOWN_COMPLETE:
        front_end_step_complete(s);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void tone_detected(void *user_data, int tone, int level, int delay)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "%s (%d) declared (%ddBm0)\n",
             modem_connect_tone_to_str(tone),
             tone,
             level);
    if (tone != MODEM_CONNECT_TONES_NONE)
    {
        s->tone_on_time = s->timer;
    }
    else
    {
        span_log(&s->logging,
                 SPAN_LOG_FLOW,
                 "Tone was on for %fs\n",
                 (float) (s->timer - s->tone_on_time)/SAMPLE_RATE + 0.55);
    }
    s->tone_state = tone;
    if (tone == MODEM_CONNECT_TONES_NONE)
        front_end_step_complete(s);
}
/*- End of function --------------------------------------------------------*/

static int non_ecm_get_bit(void *user_data)
{
    faxtester_state_t *s;
    int bit;

    s = (faxtester_state_t *) user_data;
    if (s->image_bit_ptr == 0)
    {
        if (s->image_ptr >= s->image_len)
        {
            s->image_buffer = NULL;
            return SIG_STATUS_END_OF_DATA;
        }
        s->image_bit_ptr = 8;
        s->image_ptr++;
    }
    s->image_bit_ptr--;
    bit = (s->image_buffer[s->image_ptr] >> (7 - s->image_bit_ptr)) & 0x01;
    //printf("Rx bit - %d\n", bit);
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void faxtester_set_ecm_image_buffer(faxtester_state_t *s, int block, int frame_size, int crc_hit)
{
    s->image_ptr = 256*frame_size*block;
    if (s->image_len > s->image_ptr + 256*frame_size)
        s->image_len = s->image_ptr + 256*frame_size;

    s->ecm_frame_size = frame_size;
    s->image_bit_ptr = 8;
    s->corrupt_crc = crc_hit;
    s->image_buffer = s->image;

    /* Send the first frame */
    hdlc_underflow_handler(s);
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_rx_status(void *user_data, int status)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM carrier status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_TRAINING_FAILED:
        s->modems.rx_trained = false;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->modems.rx_trained = true;
        break;
    case SIG_STATUS_CARRIER_UP:
        s->modems.rx_signal_present = true;
        break;
    case SIG_STATUS_CARRIER_DOWN:
        if (s->modems.rx_trained)
            faxtester_real_time_frame_handler(s, true, NULL, 0);
        s->modems.rx_signal_present = false;
        s->modems.rx_trained = false;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void non_ecm_put_bit(void *user_data, int bit)
{
    if (bit < 0)
    {
        non_ecm_rx_status(user_data, bit);
        return;
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_rx_status(void *user_data, int status)
{
    faxtester_state_t *s;

    s = (faxtester_state_t *) user_data;
    span_log(&s->logging, SPAN_LOG_FLOW, "HDLC carrier status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_TRAINING_FAILED:
        s->modems.rx_trained = false;
        break;
    case SIG_STATUS_TRAINING_SUCCEEDED:
        /* The modem is now trained */
        s->modems.rx_trained = true;
        break;
    case SIG_STATUS_CARRIER_UP:
        s->modems.rx_signal_present = true;
        break;
    case SIG_STATUS_CARRIER_DOWN:
        s->modems.rx_signal_present = false;
        s->modems.rx_trained = false;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static void hdlc_accept(void *user_data, const uint8_t *msg, int len, int ok)
{
    faxtester_state_t *s;

    if (len < 0)
    {
        hdlc_rx_status(user_data, len);
        return;
    }
    s = (faxtester_state_t *) user_data;
    faxtester_real_time_frame_handler(s, true, msg, len);
}
/*- End of function --------------------------------------------------------*/

int faxtester_rx(faxtester_state_t *s, int16_t *amp, int len)
{
    int i;

    for (i = 0;  i < len;  i++)
        amp[i] = dc_restore(&s->modems.dc_restore, amp[i]);
    if (s->modems.rx_handler)
        s->modems.rx_handler(s->modems.rx_user_data, amp, len);
    timer_update(s, len);
    if (s->wait_for_silence)
    {
        if (!s->modems.rx_signal_present)
        {
            s->wait_for_silence = false;
            front_end_step_complete(s);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_tx(faxtester_state_t *s, int16_t *amp, int max_len)
{
    int len;

    len = 0;
    if (s->transmit)
    {
        while ((len += s->modems.tx_handler(s->modems.tx_user_data, amp + len, max_len - len)) < max_len)
        {
            /* Allow for a change of tx handler within a block */
            front_end_step_complete(s);
            if (!s->transmit)
            {
                if (s->modems.transmit_on_idle)
                {
                    /* Pad to the requested length with silence */
                    memset(amp + len, 0, (max_len - len)*sizeof(int16_t));
                    len = max_len;
                }
                break;
            }
        }
    }
    else
    {
        if (s->modems.transmit_on_idle)
        {
            /* Pad to the requested length with silence */
            memset(amp, 0, max_len*sizeof(int16_t));
            len = max_len;
        }
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

void faxtest_set_rx_silence(faxtester_state_t *s)
{
    s->wait_for_silence = true;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_rx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    faxtester_state_t *s;
    fax_modems_state_t *t;

    s = (faxtester_state_t *) user_data;
    t = &s->modems;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set rx type %d\n", type);
    if (s->current_rx_type == type)
        return;
    s->current_rx_type = type;
    if (use_hdlc)
        hdlc_rx_init(&t->hdlc_rx, false, false, HDLC_FRAMING_OK_THRESHOLD, hdlc_accept, s);
    switch (type)
    {
    case T30_MODEM_CED:
        fax_modems_start_slow_modem(t, FAX_MODEM_CED_TONE_RX);
        s->tone_state = MODEM_CONNECT_TONES_NONE;
        break;
    case T30_MODEM_CNG:
        fax_modems_start_slow_modem(t, FAX_MODEM_CNG_TONE_RX);
        s->tone_state = MODEM_CONNECT_TONES_NONE;
        break;
    case T30_MODEM_V21:
        if (s->flush_handler)
            s->flush_handler(s, s->flush_user_data, 3);
        fax_modems_start_slow_modem(t, FAX_MODEM_V21_RX);
        break;
    case T30_MODEM_V27TER:
        fax_modems_start_fast_modem(t, FAX_MODEM_V27TER_RX, bit_rate, short_train, use_hdlc);
        break;
    case T30_MODEM_V29:
        fax_modems_start_fast_modem(t, FAX_MODEM_V29_RX, bit_rate, short_train, use_hdlc);
        break;
    case T30_MODEM_V17:
        fax_modems_start_fast_modem(t, FAX_MODEM_V17_RX, bit_rate, short_train, use_hdlc);
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
    default:
        fax_modems_set_rx_handler(t, (span_rx_handler_t) &span_dummy_rx, s, NULL, s);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_tx_type(void *user_data, int type, int bit_rate, int short_train, int use_hdlc)
{
    faxtester_state_t *s;
    get_bit_func_t get_bit_func;
    void *get_bit_user_data;
    fax_modems_state_t *t;
    int tone;

    s = (faxtester_state_t *) user_data;
    t = &s->modems;
    span_log(&s->logging, SPAN_LOG_FLOW, "Set tx type %d\n", type);
    if (s->current_tx_type == type)
        return;
    if (use_hdlc)
    {
        get_bit_func = (get_bit_func_t) hdlc_tx_get_bit;
        get_bit_user_data = (void *) &t->hdlc_tx;
    }
    else
    {
        get_bit_func = non_ecm_get_bit;
        get_bit_user_data = (void *) s;
    }
    switch (type)
    {
    case T30_MODEM_PAUSE:
        silence_gen_alter(&t->silence_gen, ms_to_samples(short_train));
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        s->transmit = true;
        break;
    case T30_MODEM_CED:
    case T30_MODEM_CNG:
        tone = (type == T30_MODEM_CED)  ?  FAX_MODEM_CED_TONE_TX  :  FAX_MODEM_CNG_TONE_TX;
        fax_modems_start_slow_modem(t, tone);
        s->transmit = true;
        break;
    case T30_MODEM_V21:
        fax_modems_start_slow_modem(t, FAX_MODEM_V21_TX);
        fsk_tx_set_modem_status_handler(&t->v21_tx, modem_tx_status, (void *) s);
        s->transmit = true;
        break;
    case T30_MODEM_V27TER:
        fax_modems_set_get_bit(t, get_bit_func, get_bit_user_data);
        fax_modems_start_fast_modem(t, FAX_MODEM_V27TER_TX, bit_rate, short_train, use_hdlc);
        v27ter_tx_set_modem_status_handler(&t->fast_modems.v27ter_tx, modem_tx_status, (void *) s);
        /* For any fast modem, set 200ms of preamble flags */
        hdlc_tx_flags(&t->hdlc_tx, bit_rate/(8*5));
        s->transmit = true;
        break;
    case T30_MODEM_V29:
        fax_modems_set_get_bit(t, get_bit_func, get_bit_user_data);
        fax_modems_start_fast_modem(t, FAX_MODEM_V29_TX, bit_rate, short_train, use_hdlc);
        v29_tx_set_modem_status_handler(&t->fast_modems.v29_tx, modem_tx_status, (void *) s);
        /* For any fast modem, set 200ms of preamble flags */
        hdlc_tx_flags(&t->hdlc_tx, bit_rate/(8*5));
        s->transmit = true;
        break;
    case T30_MODEM_V17:
        fax_modems_set_get_bit(t, get_bit_func, get_bit_user_data);
        fax_modems_start_fast_modem(t, FAX_MODEM_V17_TX, bit_rate, short_train, use_hdlc);
        v17_tx_set_modem_status_handler(&t->fast_modems.v17_tx, modem_tx_status, (void *) s);
        /* For any fast modem, set 200ms of preamble flags */
        hdlc_tx_flags(&t->hdlc_tx, bit_rate/(8*5));
        s->transmit = true;
        break;
    case T30_MODEM_DONE:
        span_log(&s->logging, SPAN_LOG_FLOW, "FAX exchange complete\n");
        /* Fall through */
    default:
        silence_gen_alter(&t->silence_gen, 0);
        fax_modems_set_tx_handler(t, (span_tx_handler_t) &silence_gen, &t->silence_gen);
        s->transmit = false;
        break;
    }
    s->current_tx_type = type;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_timeout(faxtester_state_t *s, int timeout)
{
    if (timeout >= 0)
        s->timeout = s->timer + timeout*SAMPLE_RATE/1000;
    else
        s->timeout = 0x7FFFFFFFFFFFFFFFLL;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_transmit_on_idle(faxtester_state_t *s, int transmit_on_idle)
{
    s->modems.transmit_on_idle = transmit_on_idle;
}
/*- End of function --------------------------------------------------------*/

void faxtester_set_tep_mode(faxtester_state_t *s, int use_tep)
{
    fax_modems_set_tep_mode(&s->modems, use_tep);
}
/*- End of function --------------------------------------------------------*/

static void corrupt_image(faxtester_state_t *s, const char *bad_rows)
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
    for (i = 0;  i < s->image_len;  i++)
    {
        bits ^= (s->image[i] << 11);
        bitsx ^= (s->image[i] << 11);
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
        s->image[i] = (bitsx >> 3) & 0xFF;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "%d rows found. %d corrupted\n", row, x);
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
        /* If we find ... we allow arbitrary additional info beyond this point in the message */
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

void faxtester_set_flush_handler(faxtester_state_t *s, faxtester_flush_handler_t handler, void *user_data)
{
    s->flush_handler = handler;
    s->flush_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

static void fax_prepare(faxtester_state_t *s)
{
    if (s->far_fax)
    {
        fax_set_transmit_on_idle(s->far_fax, true);
        fax_set_tep_mode(s->far_fax, true);
    }
#if 0
    t30_set_tx_ident(s->far_t30, "1234567890");
    t30_set_tx_sub_address(s->far_t30, "Sub-address");
    t30_set_tx_sender_ident(s->far_t30, "Sender ID");
    t30_set_tx_password(s->far_t30, "Password");
    t30_set_tx_polled_sub_address(s->far_t30, "Polled sub-address");
    t30_set_tx_selective_polling_address(s->far_t30, "Sel polling address");
#endif
    t30_set_tx_nsf(s->far_t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp NSF\x00", 16);
    //t30_set_tx_nss(s->far_t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp NSS\x00", 16);
    t30_set_tx_nsc(s->far_t30, (const uint8_t *) "\x50\x00\x00\x00Spandsp NSC\x00", 16);
    t30_set_ecm_capability(s->far_t30, true);
    t30_set_supported_t30_features(s->far_t30,
                                   T30_SUPPORT_IDENTIFICATION
                                 | T30_SUPPORT_SELECTIVE_POLLING
                                 | T30_SUPPORT_SUB_ADDRESSING);
    t30_set_supported_image_sizes(s->far_t30,
                                  T4_SUPPORT_WIDTH_215MM
                                | T4_SUPPORT_WIDTH_255MM
                                | T4_SUPPORT_WIDTH_303MM
                                | T4_SUPPORT_LENGTH_US_LETTER
                                | T4_SUPPORT_LENGTH_US_LEGAL
                                | T4_SUPPORT_LENGTH_UNLIMITED);
    t30_set_supported_bilevel_resolutions(s->far_t30,
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
    t30_set_supported_colour_resolutions(s->far_t30, 0);
    t30_set_supported_modems(s->far_t30, T30_SUPPORT_V27TER | T30_SUPPORT_V29 | T30_SUPPORT_V17);
    t30_set_supported_compressions(s->far_t30, T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6);
    t30_set_phase_b_handler(s->far_t30, faxtester_phase_b_handler, (void *) s);
    t30_set_phase_d_handler(s->far_t30, faxtester_phase_d_handler, (void *) s);
    t30_set_phase_e_handler(s->far_t30, faxtester_phase_e_handler, (void *) s);
    t30_set_real_time_frame_handler(s->far_t30, t30_real_time_frame_handler, (void *) s);
    t30_set_document_handler(s->far_t30, faxtester_document_handler, (void *) s);
}
/*- End of function --------------------------------------------------------*/

static void get_node_parms(struct xml_node_parms_s *parms, xmlNodePtr node)
{
    parms->dir = xmlGetProp(node, (const xmlChar *) "dir");
    parms->type = xmlGetProp(node, (const xmlChar *) "type");
    parms->modem = xmlGetProp(node, (const xmlChar *) "modem");
    parms->value = xmlGetProp(node, (const xmlChar *) "value");
    parms->tag = xmlGetProp(node, (const xmlChar *) "tag");
    parms->bad_rows = xmlGetProp(node, (const xmlChar *) "bad_rows");
    parms->crc_error = xmlGetProp(node, (const xmlChar *) "crc_error");
    parms->pattern = xmlGetProp(node, (const xmlChar *) "pattern");
    parms->timein = xmlGetProp(node, (const xmlChar *) "timein");
    parms->timeout = xmlGetProp(node, (const xmlChar *) "timeout");
    parms->min_bits = xmlGetProp(node, (const xmlChar *) "min_bits");
    parms->frame_size = xmlGetProp(node, (const xmlChar *) "frame_size");
    parms->block = xmlGetProp(node, (const xmlChar *) "block");
    parms->compression = xmlGetProp(node, (const xmlChar *) "compression");
}
/*- End of function --------------------------------------------------------*/

static void free_node_parms(struct xml_node_parms_s *parms)
{
    if (parms->dir)
        xmlFree(parms->dir);
    if (parms->type)
        xmlFree(parms->type);
    if (parms->modem)
        xmlFree(parms->modem);
    if (parms->value)
        xmlFree(parms->value);
    if (parms->tag)
        xmlFree(parms->tag);
    if (parms->bad_rows)
        xmlFree(parms->bad_rows);
    if (parms->crc_error)
        xmlFree(parms->crc_error);
    if (parms->pattern)
        xmlFree(parms->pattern);
    if (parms->timein)
        xmlFree(parms->timein);
    if (parms->timeout)
        xmlFree(parms->timeout);
    if (parms->min_bits)
        xmlFree(parms->min_bits);
    if (parms->frame_size)
        xmlFree(parms->frame_size);
    if (parms->block)
        xmlFree(parms->block);
    if (parms->compression)
        xmlFree(parms->compression);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) faxtester_next_step(faxtester_state_t *s)
{
    int delay;
    int flags;
    struct xml_node_parms_s parms;
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
    xmlChar *min;
    xmlChar *max;
    t4_tx_state_t t4_tx_state;
    t30_stats_t t30_stats;

    s->test_for_call_clear = false;
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
    for (;;)
    {
        if (s->cur == NULL)
        {
            if (s->repeat_parent == NULL)
            {
                /* Finished */
                printf("Test passed\n");
                exit(0);
            }
            if (++s->repeat_count > s->repeat_max)
            {
                /* Finished */
                printf("Too many repeats\n");
                printf("Test failed\n");
                exit(0);
            }
            if (s->repeat_count < s->repeat_min)
            {
                s->cur = s->repeat_start;
            }
            else
            {
                s->cur = s->repeat_parent->next;
                s->repeat_parent = NULL;
            }
        }
        if (xmlStrcmp(s->cur->name, (const xmlChar *) "step") == 0)
        {
            break;
        }
        if (s->repeat_parent == NULL  &&  xmlStrcmp(s->cur->name, (const xmlChar *) "repeat") == 0)
        {
            min = xmlGetProp(s->cur, (const xmlChar *) "min");
            max = xmlGetProp(s->cur, (const xmlChar *) "max");
            s->repeat_min = min  ?  atoi((const char *) min)  :  0;
            s->repeat_max = max  ?  atoi((const char *) max)  :  INT_MAX;
            s->repeat_count = 0;
            if (min)
                xmlFree(min);
            if (max)
                xmlFree(max);
            if (s->repeat_min > 0)
            {
                s->repeat_parent = s->cur;
                s->repeat_start =
                s->cur = s->cur->xmlChildrenNode;
                continue;
            }
        }
        s->cur = s->cur->next;
    }

    get_node_parms(&parms, s->cur);

    s->cur = s->cur->next;

    span_log(&s->logging,
             SPAN_LOG_FLOW,
             "Dir - %s, type - %s, modem - %s, value - %s, timein - %s, timeout - %s, tag - %s\n",
             (parms.dir)  ?  (const char *) parms.dir  :  " ",
             (parms.type)  ?  (const char *) parms.type  :  "",
             (parms.modem)  ?  (const char *) parms.modem  :  "",
             (parms.value)  ?  (const char *) parms.value  :  "",
             (parms.timein)  ?  (const char *) parms.timein  :  "",
             (parms.timeout)  ?  (const char *) parms.timeout  :  "",
             (parms.tag)  ?  (const char *) parms.tag  :  "");
    if (parms.type == NULL)
    {
        free_node_parms(&parms);
        return 1;
    }
    s->timein_x = (parms.timein)  ?  atoi((const char *) parms.timein)  :  -1;
    s->timeout_x = (parms.timeout)  ?  atoi((const char *) parms.timeout)  :  -1;

    if (parms.dir  &&  strcasecmp((const char *) parms.dir, "R") == 0)
    {
        /* Receive always has a timeout applied. */
        if (s->timeout_x < 0)
            s->timeout_x = 7000;
        faxtester_set_timeout(s, s->timeout_x);
        if (parms.modem)
        {
            hdlc = (strcasecmp((const char *) parms.type, "PREAMBLE") == 0);
            short_train = (strcasecmp((const char *) parms.type, "TCF") != 0);
            faxtester_set_tx_type(s, T30_MODEM_NONE, 0, false, false);
            if (strcasecmp((const char *) parms.modem, "V.21") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V21, 300, false, true);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/14400") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 14400, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/12000") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 12000, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/9600") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 9600, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/7200") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V17, 7200, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.29/9600") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V29, 9600, false, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.29/7200") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V29, 7200, false, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.27ter/4800") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V27TER, 4800, false, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.27ter/2400") == 0)
            {
                faxtester_set_rx_type(s, T30_MODEM_V27TER, 2400, false, hdlc);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised modem\n");
            }
        }

        if (strcasecmp((const char *) parms.type, "SET") == 0)
        {
            if (strcasecmp((const char *) parms.tag, "IDENT") == 0)
                strcpy(s->expected_rx_info.ident, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "SUB") == 0)
                strcpy(s->expected_rx_info.sub_address, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "SEP") == 0)
                strcpy(s->expected_rx_info.selective_polling_address, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "PSA") == 0)
                strcpy(s->expected_rx_info.polled_sub_address, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "SID") == 0)
                strcpy(s->expected_rx_info.sender_ident, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "PWD") == 0)
                strcpy(s->expected_rx_info.password, (const char *) parms.value);
            free_node_parms(&parms);
            return 0;
        }
        else if (strcasecmp((const char *) parms.type, "CNG") == 0)
        {
            /* Look for CNG */
            faxtester_set_rx_type(s, T30_MODEM_CNG, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_NONE, 0, false, false);
        }
        else if (strcasecmp((const char *) parms.type, "CED") == 0)
        {
            /* Look for CED */
            faxtester_set_rx_type(s, T30_MODEM_CED, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_NONE, 0, false, false);
        }
        else if (strcasecmp((const char *) parms.type, "HDLC") == 0)
        {
            i = string_to_msg(buf, mask, (const char *) parms.value);
            bit_reverse(s->awaited, buf, abs(i));
            s->awaited_len = i;
        }
        else if (strcasecmp((const char *) parms.type, "TCF") == 0)
        {
        }
        else if (strcasecmp((const char *) parms.type, "MSG") == 0)
        {
        }
        else if (strcasecmp((const char *) parms.type, "PP") == 0)
        {
        }
        else if (strcasecmp((const char *) parms.type, "SILENCE") == 0)
        {
            faxtest_set_rx_silence(s);
        }
        else if (strcasecmp((const char *) parms.type, "CLEAR") == 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Far end should drop the call\n");
            s->test_for_call_clear = true;
            s->call_clear_timer = 0;
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised type '%s'\n", (const char *) parms.type);
            free_node_parms(&parms);
            return 0;
        }
    }
    else
    {
        faxtester_set_timeout(s, s->timeout_x);
        if (parms.modem)
        {
            hdlc = (strcasecmp((const char *) parms.type, "PREAMBLE") == 0);
            short_train = (strcasecmp((const char *) parms.type, "TCF") != 0);
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            if (strcasecmp((const char *) parms.modem, "V.21") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V21, 300, false, true);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/14400") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 14400, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/12000") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 12000, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/9600") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 9600, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.17/7200") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V17, 7200, short_train, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.29/9600") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V29, 9600, false, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.29/7200") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V29, 7200, false, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.27ter/4800") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V27TER, 4800, false, hdlc);
            }
            else if (strcasecmp((const char *) parms.modem, "V.27ter/2400") == 0)
            {
                faxtester_set_tx_type(s, T30_MODEM_V27TER, 2400, false, hdlc);
            }
            else
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised modem\n");
            }
        }

        if (strcasecmp((const char *) parms.type, "SET") == 0)
        {
            if (strcasecmp((const char *) parms.tag, "IDENT") == 0)
                t30_set_tx_ident(s->far_t30, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "SUB") == 0)
                t30_set_tx_sub_address(s->far_t30, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "SEP") == 0)
                t30_set_tx_selective_polling_address(s->far_t30, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "PSA") == 0)
                t30_set_tx_polled_sub_address(s->far_t30, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "SID") == 0)
                t30_set_tx_sender_ident(s->far_t30, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "PWD") == 0)
                t30_set_tx_password(s->far_t30, (const char *) parms.value);
            else if (strcasecmp((const char *) parms.tag, "RXFILE") == 0)
            {
                if (parms.value)
                    t30_set_rx_file(s->far_t30, (const char *) parms.value, -1);
                else
                    t30_set_rx_file(s->far_t30, output_tiff_file_name, -1);
            }
            else if (strcasecmp((const char *) parms.tag, "TXFILE") == 0)
            {
                sprintf(s->next_tx_file, "%s/%s", s->image_path, (const char *) parms.value);
                printf("Push '%s'\n", s->next_tx_file);
            }
            free_node_parms(&parms);
            return 0;
        }
        else if (strcasecmp((const char *) parms.type, "CALL") == 0)
        {
            if (s->far_fax)
                fax_restart(s->far_fax, false);
            else
                t38_terminal_restart(s->far_t38, false);
            fax_prepare(s);
            s->next_tx_file[0] = '\0';
            t30_set_rx_file(s->far_t30, output_tiff_file_name, -1);
            /* Avoid libtiff 3.8.2 and earlier bug on complex 2D lines. */
            t30_set_supported_output_compressions(s->far_t30, T4_COMPRESSION_T4_1D);
            if (parms.value)
            {
                sprintf(path, "%s/%s", s->image_path, (const char *) parms.value);
                t30_set_tx_file(s->far_t30, path, -1, -1);
            }
            free_node_parms(&parms);
            return 0;
        }
        else if (strcasecmp((const char *) parms.type, "ANSWER") == 0)
        {
            if (s->far_fax)
                fax_restart(s->far_fax, true);
            else
                t38_terminal_restart(s->far_t38, true);
            fax_prepare(s);
            s->next_tx_file[0] = '\0';
            /* Avoid libtiff 3.8.2 and earlier bug on complex 2D lines. */
            t30_set_supported_output_compressions(s->far_t30, T4_COMPRESSION_T4_1D);
            if (parms.value)
            {
                sprintf(path, "%s/%s", s->image_path, (const char *) parms.value);
                t30_set_tx_file(s->far_t30, path, -1, -1);
            }
            free_node_parms(&parms);
            return 0;
        }
        else if (strcasecmp((const char *) parms.type, "CNG") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_CNG, 0, false, false);
        }
        else if (strcasecmp((const char *) parms.type, "CED") == 0)
        {
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_CED, 0, false, false);
        }
        else if (strcasecmp((const char *) parms.type, "WAIT") == 0)
        {
            delay = (parms.value)  ?  atoi((const char *) parms.value)  :  1;
            faxtester_set_rx_type(s, T30_MODEM_NONE, 0, false, false);
            faxtester_set_tx_type(s, T30_MODEM_PAUSE, 0, delay, false);
        }
        else if (strcasecmp((const char *) parms.type, "PREAMBLE") == 0)
        {
            flags = (parms.value)  ?  atoi((const char *) parms.value)  :  37;
            faxtester_send_hdlc_flags(s, flags);
        }
        else if (strcasecmp((const char *) parms.type, "POSTAMBLE") == 0)
        {
            flags = (parms.value)  ?  atoi((const char *) parms.value)  :  5;
            faxtester_send_hdlc_flags(s, flags);
        }
        else if (strcasecmp((const char *) parms.type, "HDLC") == 0)
        {
            i = string_to_msg(buf, mask, (const char *) parms.value);
            bit_reverse(buf, buf, abs(i));
            if (parms.crc_error  &&  strcasecmp((const char *) parms.crc_error, "0") == 0)
                faxtester_send_hdlc_msg(s, buf, abs(i), false);
            else
                faxtester_send_hdlc_msg(s, buf, abs(i), true);
        }
        else if (strcasecmp((const char *) parms.type, "TCF") == 0)
        {
            i = (parms.value)  ?  atoi((const char *) parms.value)  :  450;
            if (parms.pattern)
            {
                /* TODO: implement proper patterns */
                j = atoi((const char *) parms.pattern);
                memset(s->image, 0x55, j);
                if (i > j)
                    memset(s->image + j, 0, i - j);
            }
            else
            {
                memset(s->image, 0, i);
            }
            s->image_ptr = 0;
            s->image_bit_ptr = 8;
            s->image_buffer = s->image;
            s->image_len = i;
        }
        else if (strcasecmp((const char *) parms.type, "MSG") == 0)
        {
            /* A non-ECM page */
            min_row_bits = (parms.min_bits)  ?  atoi((const char *) parms.min_bits)  :  0;
            sprintf(path, "%s/%s", s->image_path, (const char *) parms.value);
            if (t4_tx_init(&t4_tx_state, path, -1, -1) == NULL)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to init T.4 send\n");
                printf("Test failed\n");
                exit(2);
            }
            t4_tx_set_header_info(&t4_tx_state, NULL);
            compression_type = T4_COMPRESSION_T4_1D;
            if (parms.compression)
            {
                if (strcasecmp((const char *) parms.compression, "T.4 1D") == 0)
                    compression_type = T4_COMPRESSION_T4_1D;
                else if (strcasecmp((const char *) parms.compression, "T.4 2D") == 0)
                    compression_type = T4_COMPRESSION_T4_2D;
                else if (strcasecmp((const char *) parms.compression, "T.6") == 0)
                    compression_type = T4_COMPRESSION_T6;
                else if (strcasecmp((const char *) parms.compression, "T.85") == 0)
                    compression_type = T4_COMPRESSION_T85;
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
            s->image_len = t4_tx_get(&t4_tx_state, s->image, sizeof(s->image));
            if (parms.bad_rows)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "We need to corrupt the image\n");
                corrupt_image(s, (const char *) parms.bad_rows);
            }
            t4_tx_release(&t4_tx_state);
            span_log(&s->logging, SPAN_LOG_FLOW, "Non-ECM image is %d bytes (min row bits %d)\n", s->image_len, min_row_bits);
            s->image_ptr = 0;
            s->image_bit_ptr = 8;
            s->image_buffer = s->image;
        }
        else if (strcasecmp((const char *) parms.type, "PP") == 0)
        {
            min_row_bits = (parms.min_bits)  ?  atoi((const char *) parms.min_bits)  :  0;
            ecm_block = (parms.block)  ?  atoi((const char *) parms.block)  :  0;
            ecm_frame_size = (parms.frame_size)  ?  atoi((const char *) parms.frame_size)  :  64;
            i = (parms.crc_error)  ?  atoi((const char *) parms.crc_error)  :  -1;
            sprintf(path, "%s/%s", s->image_path, (const char *) parms.value);
            if (t4_tx_init(&t4_tx_state, path, -1, -1) == NULL)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to init T.4 send\n");
                printf("Test failed\n");
                exit(2);
            }
            t4_tx_set_header_info(&t4_tx_state, NULL);
            compression_type = T4_COMPRESSION_T4_1D;
            if (parms.compression)
            {
                if (strcasecmp((const char *) parms.compression, "T.4 1D") == 0)
                    compression_type = T4_COMPRESSION_T4_1D;
                else if (strcasecmp((const char *) parms.compression, "T.4 2D") == 0)
                    compression_type = T4_COMPRESSION_T4_2D;
                else if (strcasecmp((const char *) parms.compression, "T.6") == 0)
                    compression_type = T4_COMPRESSION_T6;
                else if (strcasecmp((const char *) parms.compression, "T.85") == 0)
                    compression_type = T4_COMPRESSION_T85;
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
            s->image_len = t4_tx_get(&t4_tx_state, s->image, sizeof(s->image));
            if (parms.bad_rows)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "We need to corrupt the image\n");
                corrupt_image(s, (const char *) parms.bad_rows);
            }
            /*endif*/
            t4_tx_release(&t4_tx_state);
            span_log(&s->logging, SPAN_LOG_FLOW, "ECM image is %d bytes (min row bits %d)\n", s->image_len, min_row_bits);
            faxtester_set_ecm_image_buffer(s, ecm_block, ecm_frame_size, i);
        }
        else if (strcasecmp((const char *) parms.type, "CLEAR") == 0)
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Time to drop the call\n");
            t30_terminate(s->far_t30);
            free_node_parms(&parms);
            return 0;
        }
        else if (strcasecmp((const char *) parms.type, "STATUS") == 0)
        {
            if (parms.value)
            {
                for (i = 0;  t30_status[i].code >= 0;  i++)
                {
                    if (strcmp(t30_status[i].tag, (const char *) parms.value) == 0)
                        break;
                }
                if (t30_status[i].code >= 0)
                    delay = t30_status[i].code;
                else
                    delay = atoi((const char *) parms.value);
                t30_get_transfer_statistics(s->far_t30, &t30_stats);
                if (delay == t30_stats.current_status)
                    span_log(&s->logging, SPAN_LOG_FLOW, "Expected status (%s) found\n", t30_status[i].tag);
                else
                    span_log(&s->logging, SPAN_LOG_FLOW, "Expected status %s, but found %s (%d)\n", t30_status[i].tag, t30_status[t30_stats.current_status].tag, t30_stats.current_status);
                if (delay != t30_stats.current_status)
                {
                    printf("Test failed\n");
                    exit(2);
                }
            }
            free_node_parms(&parms);
            return 0;
        }
        else
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Unrecognised type '%s'\n", (const char *) parms.type);
            free_node_parms(&parms);
            return 0;
        }
        /*endif*/
    }
    /*endif*/
    free_node_parms(&parms);
    return 1;
}
/*- End of function --------------------------------------------------------*/

static int parse_config(faxtester_state_t *s, xmlNodePtr cur)
{
    xmlChar *x;
    xmlChar *y;

    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "path") == 0)
        {
            x = NULL;
            y = NULL;
            if ((x = xmlGetProp(cur, (const xmlChar *) "type"))
                &&
                (y = xmlGetProp(cur, (const xmlChar *) "value")))
            {
                if (strcasecmp((const char *) x, "IMAGE") == 0)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Found '%s' '%s'\n", (char *) x, (char *) y);
                    strcpy(s->image_path, (const char *) y);
                }
                /*endif*/
            }
            /*endif*/
            if (x)
                xmlFree(x);
            /*endif*/
            if (y)
                xmlFree(y);
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    return -1;
}
/*- End of function --------------------------------------------------------*/

static int parse_test_group(faxtester_state_t *s, xmlNodePtr cur, const char *test)
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
                    xmlFree(x);
                    return 0;
                }
                /*endif*/
                xmlFree(x);
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
    xmlNodePtr cur;

    if ((ctxt = xmlNewParserCtxt()) == NULL)
    {
        fprintf(stderr, "Failed to allocate XML parser context\n");
        return -1;
    }
    /* parse the file, activating the DTD validation option */
    if ((s->doc = xmlCtxtReadFile(ctxt, test_file, NULL, XML_PARSE_XINCLUDE | XML_PARSE_DTDVALID)) == NULL)
    {
        fprintf(stderr, "Failed to read the XML document\n");
        return -1;
    }
    if (ctxt->valid == 0)
    {
        fprintf(stderr, "Failed to validate the XML document\n");
    	xmlFreeDoc(s->doc);
        s->doc = NULL;
        xmlFreeParserCtxt(ctxt);
        return -1;
    }
    xmlFreeParserCtxt(ctxt);

    /* Check the document is of the right kind */
    if ((cur = xmlDocGetRootElement(s->doc)) == NULL)
    {
        xmlFreeDoc(s->doc);
        s->doc = NULL;
        fprintf(stderr, "Empty document\n");
        return -1;
    }
    /*endif*/
    if (xmlStrcmp(cur->name, (const xmlChar *) "fax-tests"))
    {
        xmlFreeDoc(s->doc);
        s->doc = NULL;
        fprintf(stderr, "Document of the wrong type, root node != fax-tests\n");
        return -1;
    }
    /*endif*/
    cur = cur->xmlChildrenNode;
    while (cur  &&  xmlIsBlankNode(cur))
        cur = cur->next;
    /*endwhile*/
    if (cur == NULL)
    {
        fprintf(stderr, "XML test not found\n");
        return -1;
    }
    /*endif*/
    xmlCleanupParser();
    while (cur)
    {
        if (xmlStrcmp(cur->name, (const xmlChar *) "config") == 0)
            parse_config(s, cur->xmlChildrenNode);
        /*endif*/
        if (xmlStrcmp(cur->name, (const xmlChar *) "test-group") == 0)
        {
            if (parse_test_group(s, cur->xmlChildrenNode, test) == 0)
                return 0;
            /*endif*/
        }
        /*endif*/
        cur = cur->next;
    }
    /*endwhile*/
    fprintf(stderr, "XML test not found\n");
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) faxtester_get_logging_state(faxtester_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

faxtester_state_t *faxtester_init(faxtester_state_t *s, const char *test_file, const char *test)
{
    if (s == NULL)
    {
        if ((s = (faxtester_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    /*endif*/

    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "TST");
    fax_modems_init(&s->modems,
                    false,
                    hdlc_accept,
                    hdlc_underflow_handler,
                    non_ecm_put_bit,
                    t38_non_ecm_buffer_get_bit,
                    tone_detected,
                    s);
    fax_modems_set_tep_mode(&s->modems, false);
    fax_modems_set_rx_active(&s->modems, true);
    faxtester_set_timeout(s, -1);
    s->timein_x = -1;
    s->timeout_x = -1;
    faxtester_set_tx_type(s, T30_MODEM_NONE, 0, false, false);
    strcpy(s->image_path, ".");
    s->next_tx_file[0] = '\0';
    if (get_test_set(s, test_file, test) < 0)
    {
        /* TODO: free the state, if it was allocated. */
        return NULL;
    }
    /*endif*/
    memset(&s->expected_rx_info, 0, sizeof(s->expected_rx_info));
    return s;
}
/*- End of function --------------------------------------------------------*/

int faxtester_release(faxtester_state_t *s)
{
    if (s->doc)
    {
        xmlFreeDoc(s->doc);
        s->doc = NULL;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int faxtester_free(faxtester_state_t *s)
{
    faxtester_release(s);
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
