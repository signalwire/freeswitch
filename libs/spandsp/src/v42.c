/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v42.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004, 2011 Steve Underwood
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

/* THIS IS A WORK IN PROGRESS. IT IS NOT FINISHED. */

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
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/async.h"
#include "spandsp/hdlc.h"
#include "spandsp/v42.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/hdlc.h"
#include "spandsp/private/v42.h"

/* Detection phase timer */
#define T_400                           750
/* Acknowledgement timer - 1 second between SABME's */
#define T_401                           1000
/* Replay delay timer (optional) */
#define T_402                           1000
/* Inactivity timer (optional). No default - use 10 seconds with no packets */
#define T_403                           10000

#define LAPM_DLCI_DTE_TO_DTE            0
#define LAPM_DLCI_LAYER2_MANAGEMENT     63

#define elements(a) (sizeof(a)/sizeof((a)[0]))

/* LAPM definitions */

#define LAPM_FRAMETYPE_MASK             0x03

enum
{
    LAPM_FRAMETYPE_I = 0x00,
    LAPM_FRAMETYPE_I_ALT = 0x02,
    LAPM_FRAMETYPE_S = 0x01,
    LAPM_FRAMETYPE_U = 0x03
};

/* Supervisory headers */
enum
{
    LAPM_S_RR = 0x00,       /* cr */
    LAPM_S_RNR = 0x04,      /* cr */
    LAPM_S_REJ = 0x08,      /* cr */
    LAPM_S_SREJ = 0x0C      /* cr */
};

#define LAPM_S_PF                       0x01

/* Unnumbered headers */
enum
{
    LAPM_U_UI = 0x00,       /* cr */
    LAPM_U_DM = 0x0C,       /*  r */
    LAPM_U_DISC = 0x40,     /* c  */
    LAPM_U_UA = 0x60,       /*  r */
    LAPM_U_SABME = 0x6C,    /* c  */
    LAPM_U_FRMR = 0x84,     /*  r */
    LAPM_U_XID = 0xAC,      /* cr */
    LAPM_U_TEST = 0xE0      /* c  */
};

#define LAPM_U_PF                       0x10

/* XID sub-field definitions */
#define FI_GENERAL                      0x82
#define GI_PARAM_NEGOTIATION            0x80
#define GI_PRIVATE_NEGOTIATION          0xF0
#define GI_USER_DATA                    0xFF

/* Param negotiation (Table 11a/V.42) */
enum
{
    PI_HDLC_OPTIONAL_FUNCTIONS = 0x03,
    PI_TX_INFO_MAXSIZE = 0x05,
    PI_RX_INFO_MAXSIZE = 0x06,
    PI_TX_WINDOW_SIZE = 0x07,
    PI_RX_WINDOW_SIZE = 0x08
};

/* Private param negotiation (Table 11b/V.42) */
enum
{
    PI_PARAMETER_SET_ID = 0x00,
    PI_V42BIS_COMPRESSION_REQUEST = 0x01,
    PI_V42BIS_NUM_CODEWORDS = 0x02,
    PI_V42BIS_MAX_STRING_LENGTH = 0x03
};

#define LAPM_DLCI_DTE_TO_DTE            0
#define LAPM_DLCI_LAYER2_MANAGEMENT     63

/* Type definitions */
enum
{
    LAPM_DETECT = 0,
    LAPM_IDLE = 1,
    LAPM_ESTABLISH = 2,
    LAPM_DATA = 3,
    LAPM_RELEASE = 4,
    LAPM_SIGNAL = 5,
    LAPM_SETPARM = 6,
    LAPM_TEST = 7,
    LAPM_V42_UNSUPPORTED = 8
};

/* Prototypes */
static int lapm_connect(v42_state_t *ss);
static int lapm_disconnect(v42_state_t *s);
static void reset_lapm(v42_state_t *s);
static void lapm_hdlc_underflow(void *user_data);

static int lapm_config(v42_state_t *ss);

SPAN_DECLARE(const char *) lapm_status_to_str(int status)
{
    switch (status)
    {
    case LAPM_DETECT:
        return "LAPM_DETECT";
    case LAPM_IDLE:
        return "LAPM_IDLE";
    case LAPM_ESTABLISH:
        return "LAPM_ESTABLISH";
    case LAPM_DATA:
        return "LAPM_DATA";
    case LAPM_RELEASE:
        return "LAPM_RELEASE";
    case LAPM_SIGNAL:
        return "LAPM_SIGNAL";
    case LAPM_SETPARM:
        return "LAPM_SETPARM";
    case LAPM_TEST:
        return "LAPM_TEST";
    case LAPM_V42_UNSUPPORTED:
        return "LAPM_V42_UNSUPPORTED";
    }
    /*endswitch*/
    return "???";
}
/*- End of function --------------------------------------------------------*/

static void report_rx_status_change(v42_state_t *s, int status)
{
    if (s->lapm.status_handler)
        s->lapm.status_handler(s->lapm.status_user_data, status);
    else if (s->lapm.iframe_put)
        s->lapm.iframe_put(s->lapm.iframe_put_user_data, NULL, status);
}
/*- End of function --------------------------------------------------------*/

static inline uint32_t pack_value(const uint8_t *buf, int len)
{
    uint32_t val;

    val = 0;
    while (len--)
    {
        val <<= 8;
        val |= *buf++;
    }
    return val;
}
/*- End of function --------------------------------------------------------*/

static inline v42_frame_t *get_next_free_ctrl_frame(lapm_state_t *s)
{
    v42_frame_t *f;
    int ctrl_put_next;

    if ((ctrl_put_next = s->ctrl_put + 1) >= V42_CTRL_FRAMES)
        ctrl_put_next = 0;
    if (ctrl_put_next == s->ctrl_get)
        return NULL;
    f = &s->ctrl_buf[s->ctrl_put];
    s->ctrl_put = ctrl_put_next;
    return f;
}
/*- End of function --------------------------------------------------------*/

static int tx_unnumbered_frame(lapm_state_t *s, uint8_t addr, uint8_t ctrl, uint8_t *info, int len)
{
    v42_frame_t *f;
    uint8_t *buf;

    if ((f = get_next_free_ctrl_frame(s)) == NULL)
        return -1;
    buf = f->buf;
    buf[0] = addr;
    buf[1] = LAPM_FRAMETYPE_U | ctrl;
    f->len = 2;
    if (info  &&  len)
    {
        memcpy(buf + f->len, info, len);
        f->len += len;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tx_supervisory_frame(lapm_state_t *s, uint8_t addr, uint8_t ctrl, uint8_t pf_mask)
{
    v42_frame_t *f;
    uint8_t *buf;

    if ((f = get_next_free_ctrl_frame(s)) == NULL)
        return -1;
    buf = f->buf;
    buf[0] = addr;
    buf[1] = LAPM_FRAMETYPE_S | ctrl;
    buf[2] = (s->vr << 1) | pf_mask;
    f->len = 3;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int set_param(int param, int value, int def)
{
    if ((value < def  &&  param >= def)  ||  (value >= def  &&  param < def))
        return def;
    if ((value < def  &&  param < value)  ||  (value >= def  &&  param > value))
        return value;
    return param;
}
/*- End of function --------------------------------------------------------*/

static int receive_xid(v42_state_t *ss, const uint8_t *frame, int len)
{
    lapm_state_t *s;
    v42_config_parameters_t config;
    const uint8_t *buf;
    uint8_t group_id;
    uint16_t group_len;
    uint32_t param_val;
    uint8_t param_id;
    uint8_t param_len;

    s = &ss->lapm;
    if (frame[2] != FI_GENERAL)
        return -1;
    memset(&config, 0, sizeof(config));
    /* Skip the header octets */
    frame += 3;
    len -= 3;
    while (len > 0)
    {
        group_id = frame[0];
        group_len = frame[1];
        group_len = (group_len << 8) | frame[2];
        frame += 3;
        len -= (3 + group_len);
        if (len < 0)
            break;
        buf = frame;
        frame += group_len;
        switch (group_id)
        {
        case GI_PARAM_NEGOTIATION:
            while (group_len > 0)
            {
                param_id = buf[0];
                param_len = buf[1];
                buf += 2;
                if (group_len < (2 + param_len))
                    break;
                group_len -= (2 + param_len);
                switch (param_id)
                {
                case PI_HDLC_OPTIONAL_FUNCTIONS:
                    /* TODO: param_val is never used right now. */
                    param_val = pack_value(buf, param_len);
                    break;
                case PI_TX_INFO_MAXSIZE:
                    param_val = pack_value(buf, param_len);
                    param_val >>= 3;
                    config.v42_tx_n401 =
                    s->tx_n401 = set_param(s->tx_n401, param_val, ss->config.v42_tx_n401);
                    break;
                case PI_RX_INFO_MAXSIZE:
                    param_val = pack_value(buf, param_len);
                    param_val >>= 3;
                    config.v42_rx_n401 =
                    s->rx_n401 = set_param(s->rx_n401, param_val, ss->config.v42_rx_n401);
                    break;
                case PI_TX_WINDOW_SIZE:
                    param_val = pack_value(buf, param_len);
                    config.v42_tx_window_size_k =
                    s->tx_window_size_k = set_param(s->tx_window_size_k, param_val, ss->config.v42_tx_window_size_k);
                    break;
                case PI_RX_WINDOW_SIZE:
                    param_val = pack_value(buf, param_len);
                    config.v42_rx_window_size_k =
                    s->rx_window_size_k = set_param(s->rx_window_size_k, param_val, ss->config.v42_rx_window_size_k);
                    break;
                default:
                    break;
                }
                buf += param_len;
            }
            break;
        case GI_PRIVATE_NEGOTIATION:
            while (group_len > 0)
            {
                param_id = buf[0];
                param_len = buf[1];
                buf += 2;
                if (group_len < 2 + param_len)
                    break;
                group_len -= (2 + param_len);
                switch (param_id)
                {
                case PI_PARAMETER_SET_ID:
                    /* This might be worth monitoring, but it doesn't serve mnuch other purpose */
                    break;
                case PI_V42BIS_COMPRESSION_REQUEST:
                    config.comp = pack_value(buf, param_len);
                    break;
                case PI_V42BIS_NUM_CODEWORDS:
                    config.comp_dict_size = pack_value(buf, param_len);
                    break;
                case PI_V42BIS_MAX_STRING_LENGTH:
                    config.comp_max_string = pack_value(buf, param_len);
                    break;
                default:
                    break;
                }
                buf += param_len;
            }
            break;
        default:
            break;
        }
    }
    //v42_update_config(ss, &config);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void transmit_xid(v42_state_t *ss, uint8_t addr)
{
    lapm_state_t *s;
    uint8_t *buf;
    int len;
    int group_len;
    uint32_t param_val;
    v42_frame_t *f;

    s = &ss->lapm;
    if ((f = get_next_free_ctrl_frame(s)) == NULL)
        return;

    buf = f->buf;
    len = 0;

    /* Figure 11/V.42 */
    *buf++ = addr;
    *buf++ = LAPM_U_XID | LAPM_FRAMETYPE_U;
    /* Format identifier subfield */
    *buf++ = FI_GENERAL;
    len += 3;

    /* Parameter negotiation group */
    group_len = 20;
    *buf++ = GI_PARAM_NEGOTIATION;
    *buf++ = (group_len >> 8) & 0xFF;
    *buf++ = group_len & 0xFF;
    len += 3;

    /* For conformance with the encoding rules in ISO/IEC 8885, the transmitter of an XID command frame shall
       set bit positions 2, 4, 8, 9, 12 and 16 to 1. (Table 11a/V.42)
       Optional bits are:
         3 Selective retransmission procedure (SREJ frame) single I frame request
        14 Loop-back test procedure (TEST frame)
        17
        Extended FCS procedure (32-bit FCS)
        24 Selective retransmission procedure (SREJ frame) multiple I frame request with span list
           capability. */
    *buf++ = PI_HDLC_OPTIONAL_FUNCTIONS;
    *buf++ = 4;
    *buf++ = 0x8A;  /* Bits 2, 4, and 8 set */
    *buf++ = 0x89;  /* Bits 9, 12, and 16 set */
    *buf++ = 0x00;
    *buf++ = 0x00;

    /* Send the maximum as a number of bits, rather than octets */
    param_val = ss->config.v42_tx_n401 << 3;
    *buf++ = PI_TX_INFO_MAXSIZE;
    *buf++ = 2;
    *buf++ = (param_val >> 8) & 0xFF;
    *buf++ = (param_val & 0xFF);

    /* Send the maximum as a number of bits, rather than octets */
    param_val = ss->config.v42_rx_n401 << 3;
    *buf++ = PI_RX_INFO_MAXSIZE;
    *buf++ = 2;
    *buf++ = (param_val >> 8) & 0xFF;
    *buf++ = (param_val & 0xFF);

    *buf++ = PI_TX_WINDOW_SIZE;
    *buf++ = 1;
    *buf++ = ss->config.v42_tx_window_size_k;

    *buf++ = PI_RX_WINDOW_SIZE;
    *buf++ = 1;
    *buf++ = ss->config.v42_rx_window_size_k;

    len += group_len;

    if (ss->config.comp)
    {
        /* Private parameter negotiation group */
        group_len = 15;
        *buf++ = GI_PRIVATE_NEGOTIATION;
        *buf++ = (group_len >> 8) & 0xFF;
        *buf++ = group_len & 0xFF;
        len += 3;

        /* Private parameter for V.42 (ASCII for V42). V.42 says ".42", but V.42bis says "V42",
           and that seems to be what should be used. */
        *buf++ = PI_PARAMETER_SET_ID;
        *buf++ = 3;
        *buf++ = 'V';
        *buf++ = '4';
        *buf++ = '2';

        /* V.42bis P0
           00 Compression in neither direction (default);
           01 Negotiation initiator-responder direction only;
           10 Negotiation responder-initiator direction only;
           11 Both directions. */
        *buf++ = PI_V42BIS_COMPRESSION_REQUEST;
        *buf++ = 1;
        *buf++ = ss->config.comp;

        /* V.42bis P1 */
        param_val = ss->config.comp_dict_size;
        *buf++ = PI_V42BIS_NUM_CODEWORDS;
        *buf++ = 2;
        *buf++ = (param_val >> 8) & 0xFF;
        *buf++ = param_val & 0xFF;

        /* V.42bis P2 */
        *buf++ = PI_V42BIS_MAX_STRING_LENGTH;
        *buf++ = 1;
        *buf++ = ss->config.comp_max_string;

        len += group_len;
    }

    f->len = len;
}
/*- End of function --------------------------------------------------------*/

static int ms_to_bits(v42_state_t *s, int time)
{
    return ((time*s->tx_bit_rate)/1000);
}
/*- End of function --------------------------------------------------------*/

static void t400_expired(v42_state_t *ss)
{
    /* Give up trying to detect a V.42 capable peer. */
    ss->bit_timer = 0;
    ss->lapm.state = LAPM_V42_UNSUPPORTED;
    report_rx_status_change(ss, ss->lapm.state);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void t400_start(v42_state_t *s)
{
    s->bit_timer = ms_to_bits(s, T_400);
    s->bit_timer_func = t400_expired;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void t400_stop(v42_state_t *s)
{
    s->bit_timer = 0;
}
/*- End of function --------------------------------------------------------*/

static void t401_expired(v42_state_t *ss)
{
    lapm_state_t *s;

    span_log(&ss->logging, SPAN_LOG_FLOW, "T.401 expired\n");
    s = &ss->lapm;
    if (s->retry_count > V42_DEFAULT_N_400)
    {
        s->retry_count = 0;
        switch (s->state)
        {
        case LAPM_ESTABLISH:
        case LAPM_RELEASE:
            s->state = LAPM_IDLE;
            report_rx_status_change(ss, SIG_STATUS_LINK_DISCONNECTED);
            break;
        case LAPM_DATA:
            lapm_disconnect(ss);
            break;
        }
        return ;
    }
    s->retry_count++;
    if (s->configuring)
    {
        transmit_xid(ss, s->cmd_addr);
    }
    else
    {
        switch (s->state)
        {
        case LAPM_ESTABLISH:
            tx_unnumbered_frame(s, s->cmd_addr, LAPM_U_SABME | LAPM_U_PF, NULL, 0);
            break;
        case LAPM_RELEASE:
            tx_unnumbered_frame(s, s->cmd_addr, LAPM_U_DISC | LAPM_U_PF, NULL, 0);
            break;
        case LAPM_DATA:
            tx_supervisory_frame(s, s->cmd_addr, (s->local_busy)  ?  LAPM_S_RNR  :  LAPM_S_RR, 1);
            break;
        }
    }
    ss->bit_timer = ms_to_bits(ss, T_401);
    ss->bit_timer_func = t401_expired;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void t401_start(v42_state_t *s)
{
    s->bit_timer = ms_to_bits(s, T_401);
    s->bit_timer_func = t401_expired;
    s->lapm.retry_count = 0;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void t401_stop(v42_state_t *s)
{
    s->bit_timer = 0;
    s->lapm.retry_count = 0;
}
/*- End of function --------------------------------------------------------*/

static void t403_expired(v42_state_t *ss)
{
    lapm_state_t *s;

    span_log(&ss->logging, SPAN_LOG_FLOW, "T.403 expired\n");
    if (ss->lapm.state != LAPM_DATA)
        return;
    s = &ss->lapm;
    tx_supervisory_frame(s, s->cmd_addr, (ss->lapm.local_busy)  ?  LAPM_S_RNR  :  LAPM_S_RR, 1);
    t401_start(ss);
    ss->lapm.retry_count = 1;
}
/*- End of function --------------------------------------------------------*/

static __inline__ void t401_stop_t403_start(v42_state_t *s)
{
    s->bit_timer = ms_to_bits(s, T_403);
    s->bit_timer_func = t403_expired;
    s->lapm.retry_count = 0;
}
/*- End of function --------------------------------------------------------*/

static void initiate_negotiation_expired(v42_state_t *s)
{
    /* Timer service routine */
    span_log(&s->logging, SPAN_LOG_FLOW, "Start negotiation\n");
    lapm_config(s);
    lapm_hdlc_underflow(s);
}
/*- End of function --------------------------------------------------------*/

static int tx_information_frame(v42_state_t *ss)
{
    lapm_state_t *s;
    v42_frame_t *f;
    uint8_t *buf;
    int n;
    int info_put_next;

    s = &ss->lapm;
    if (s->far_busy  ||  ((s->vs - s->va) & 0x7F) >= s->tx_window_size_k)
        return false;
    if (s->info_get != s->info_put)
        return true;
    if ((info_put_next = s->info_put + 1) >= V42_INFO_FRAMES)
        info_put_next = 0;
    if (info_put_next == s->info_get  ||  info_put_next == s->info_acked)
        return false;
    f = &s->info_buf[s->info_put];
    buf = f->buf;
    if (s->iframe_get == NULL)
        return false;
    n = s->iframe_get(s->iframe_get_user_data, buf + 3, s->tx_n401);
    if (n < 0)
    {
        /* Error */
        report_rx_status_change(ss, SIG_STATUS_LINK_ERROR);
        return false;
    }
    if (n == 0)
        return false;

    f->len = n + 3;
    s->info_put = info_put_next;
    return true;
}
/*- End of function --------------------------------------------------------*/

static void tx_information_rr_rnr_response(v42_state_t *ss, const uint8_t *frame, int len)
{
    lapm_state_t *s;

    s = &ss->lapm;
    /* Respond with information frame, RR, or RNR, as appropriate */
    /* p = 1 may be used for status checking */
    if ((frame[2] & 0x1)  ||  !tx_information_frame(ss))
        tx_supervisory_frame(s, frame[0], (s->local_busy)  ?  LAPM_S_RNR  :  LAPM_S_RR, 1);
}
/*- End of function --------------------------------------------------------*/

static int reject_info(lapm_state_t *s)
{
    uint8_t n;

    /* Reject all non-acked frames */
    if (s->state != LAPM_DATA)
        return 0;
    n = (s->vs - s->va) & 0x7F;
    s->vs = s->va;
    s->info_get = s->info_acked;
    return n;
}
/*- End of function --------------------------------------------------------*/

static int ack_info(v42_state_t *ss, uint8_t nr)
{
    lapm_state_t *s;
    int n;

    s = &ss->lapm;
    /* Check that NR is valid - i.e.  VA <= NR <= VS  &&  VS-VA <= k */
    if (!((((nr - s->va) & 0x7F) + ((s->vs - nr) & 0x7F)) <= s->tx_window_size_k
         &&
         ((s->vs - s->va) & 0x7F) <= s->tx_window_size_k))
    {
        lapm_disconnect(ss);
        return -1;
    }
    n = 0;
    while (s->va != nr  &&  s->info_acked != s->info_get)
    {
        if (++s->info_acked >= V42_INFO_FRAMES)
            s->info_acked = 0;
        s->va = (s->va + 1) & 0x7F;
        n++;
    }
    if (n > 0  &&  s->retry_count == 0)
    {
        t401_stop_t403_start(ss);
        /* 8.4.8 */
        if (((s->vs - s->va) & 0x7F))
            t401_start(ss);
    }
    return n;
}
/*- End of function --------------------------------------------------------*/

static int valid_data_state(v42_state_t *ss)
{
    lapm_state_t *s;

    s = &ss->lapm;
    switch (s->state)
    {
    case LAPM_DETECT:
    case LAPM_IDLE:
        break;
    case LAPM_ESTABLISH:
        reset_lapm(ss);
        s->state = LAPM_DATA;
        report_rx_status_change(ss, SIG_STATUS_LINK_CONNECTED);
        return 1;
    case LAPM_DATA:
        return 1;
    case LAPM_RELEASE:
        reset_lapm(ss);
        s->state = LAPM_IDLE;
        report_rx_status_change(ss, SIG_STATUS_LINK_DISCONNECTED);
        break;
    case LAPM_SIGNAL:
    case LAPM_SETPARM:
    case LAPM_TEST:
    case LAPM_V42_UNSUPPORTED:
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void receive_information_frame(v42_state_t *ss, const uint8_t *frame, int len)
{
    lapm_state_t *s;

    s = &ss->lapm;
    if (!valid_data_state(ss))
        return;
    if (len > s->rx_n401 + 3)
        return;
    /* Ack I frames: NR - 1 */
    ack_info(ss, frame[2] >> 1);
    if (s->local_busy)
    {
        /* 8.4.7 */
        if ((frame[2] & 0x1))
            tx_supervisory_frame(s, s->rsp_addr, LAPM_S_RNR, 1);
        return;
    }
    /* NS sequence error */
    if ((frame[1] >> 1) != s->vr)
    {
        if (!s->rejected)
        {
            tx_supervisory_frame(s, s->rsp_addr, LAPM_S_REJ, (frame[2] & 0x1));
            s->rejected = true;
        }
        return;
    }
    s->rejected = false;

    s->iframe_put(s->iframe_put_user_data, frame + 3, len - 3);
    /* Increment vr */
    s->vr = (s->vr + 1) & 0x7F;
    tx_information_rr_rnr_response(ss, frame, len);
}
/*- End of function --------------------------------------------------------*/

static void rx_supervisory_cmd_frame(v42_state_t *ss, const uint8_t *frame, int len)
{
    lapm_state_t *s;

    s = &ss->lapm;
    /* If l->local_busy each RR,RNR,REJ with p=1 should be replied by RNR with f=1 (8.4.7) */
    switch (frame[1] & 0x0C)
    {
    case LAPM_S_RR:
        s->far_busy = false;
        ack_info(ss, frame[2] >> 1);
        /* If p = 1 may be used for status checking? */
        tx_information_rr_rnr_response(ss, frame, len);
        break;
    case LAPM_S_RNR:
        s->far_busy = true;
        ack_info(ss, frame[2] >> 1);
        /* If p = 1 may be used for status checking? */
        if ((frame[2] & 0x1))
            tx_supervisory_frame(s, s->rsp_addr, (s->local_busy)  ?  LAPM_S_RNR  :  LAPM_S_RR, 1);
        break;
    case LAPM_S_REJ:
        s->far_busy = false;
        ack_info(ss, frame[2] >> 1);
        if (s->retry_count == 0)
        {
            t401_stop_t403_start(ss);
            reject_info(s);
        }
        tx_information_rr_rnr_response(ss, frame, len);
        break;
    case LAPM_S_SREJ:
        /* TODO: */
        return;
    default:
        return;
    }
}
/*- End of function --------------------------------------------------------*/

static void rx_supervisory_rsp_frame(v42_state_t *ss, const uint8_t *frame, int len)
{
    lapm_state_t *s;

    s = &ss->lapm;
    if (s->retry_count == 0  &&  (frame[2] & 0x1))
        return;
    /* Ack I frames <= NR - 1 */
    switch (frame[1] & 0x0C)
    {
    case LAPM_S_RR:
        s->far_busy = false;
        ack_info(ss, frame[2] >> 1);
        if (s->retry_count  &&  (frame[2] & 0x1))
        {
            reject_info(s);
            t401_stop_t403_start(ss);
        }
        break;
    case LAPM_S_RNR:
        s->far_busy = true;
        ack_info(ss, frame[2] >> 1);
        if (s->retry_count  &&  (frame[2] & 0x1))
        {
            reject_info(s);
            t401_stop_t403_start(ss);
        }
        if (s->retry_count == 0)
            t401_start(ss);
        break;
    case LAPM_S_REJ:
        s->far_busy = false;
        ack_info(ss, frame[2] >> 1);
        if (s->retry_count == 0  ||  (frame[2] & 0x1))
        {
            reject_info(s);
            t401_stop_t403_start(ss);
        }
        break;
    case LAPM_S_SREJ:
        /* TODO: */
        return;
    default:
        return;
    }
}
/*- End of function --------------------------------------------------------*/

static int rx_unnumbered_cmd_frame(v42_state_t *ss, const uint8_t *frame, int len)
{
    lapm_state_t *s;

    s = &ss->lapm;
    switch (frame[1] & 0xEC)
    {
    case LAPM_U_SABME:
        /* Discard un-acked I frames. Reset vs, vr, and va. Clear exceptions */
        reset_lapm(ss);
        /* Going to connected state */
        s->state = LAPM_DATA;
        /* Respond UA (or DM on error) */
        // fixme: why may be error and LAPM_U_DM ??
        tx_unnumbered_frame(s, s->rsp_addr, LAPM_U_UA | (frame[1] & 0x10), NULL, 0);
        t401_stop_t403_start(ss);
        report_rx_status_change(ss, SIG_STATUS_LINK_CONNECTED);
        break;
    case LAPM_U_UI:
        /* Break signal */
        /* TODO: */
        break;
    case LAPM_U_DISC:
        /* Respond UA (or DM) */
        if (s->state == LAPM_IDLE)
        {
            tx_unnumbered_frame(s, s->rsp_addr, LAPM_U_DM | LAPM_U_PF, NULL, 0);
        }
        else
        {
            /* Going to disconnected state, discard unacked I frames, reset all. */
            s->state = LAPM_IDLE;
            reset_lapm(ss);
            tx_unnumbered_frame(s, s->rsp_addr, LAPM_U_UA | (frame[1] & 0x10), NULL, 0);
            t401_stop(ss);
            /* TODO: notify CF */
            report_rx_status_change(ss, SIG_STATUS_LINK_DISCONNECTED);
        }
        break;
    case LAPM_U_XID:
        /* Exchange general ID info */
        receive_xid(ss, frame, len);
        transmit_xid(ss, s->rsp_addr);
        break;
    case LAPM_U_TEST:
        /* TODO: */
        break;
    default:
        return -1;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int rx_unnumbered_rsp_frame(v42_state_t *ss, const uint8_t *frame, int len)
{
    lapm_state_t *s;

    s = &ss->lapm;
    switch (frame[1] & 0xEC)
    {
    case LAPM_U_DM:
        switch (s->state)
        {
        case LAPM_IDLE:
            if (!(frame[1] & 0x10))
            {
                /* TODO: notify CF */
                report_rx_status_change(ss, SIG_STATUS_LINK_CONNECTED);
            }
            break;
        case LAPM_ESTABLISH:
        case LAPM_RELEASE:
            if ((frame[1] & 0x10))
            {
                s->state = LAPM_IDLE;
                reset_lapm(ss);
                t401_stop(ss);
                /* TODO: notify CF */
                report_rx_status_change(ss, SIG_STATUS_LINK_DISCONNECTED);
            }
            break;
        case LAPM_DATA:
            if (s->retry_count  ||  !(frame[1] & 0x10))
            {
                s->state = LAPM_IDLE;
                reset_lapm(ss);
                /* TODO: notify CF */
                report_rx_status_change(ss, SIG_STATUS_LINK_DISCONNECTED);
            }
            break;
        default:
            break;
        }
        break;
    case LAPM_U_UI:
        /* TODO: */
        break;
    case LAPM_U_UA:
        switch (s->state)
        {
        case LAPM_ESTABLISH:
            s->state = LAPM_DATA;
            reset_lapm(ss);
            t401_stop_t403_start(ss);
            report_rx_status_change(ss, SIG_STATUS_LINK_CONNECTED);
            break;
        case LAPM_RELEASE:
            s->state = LAPM_IDLE;
            reset_lapm(ss);
            t401_stop(ss);
            report_rx_status_change(ss, SIG_STATUS_LINK_DISCONNECTED);
            break;
        default:
            /* Unsolicited UA */
            /* TODO: */
            break;
        }
        /* Clear all exceptions, busy states (self and peer) */
        /* Reset vars */
        break;
    case LAPM_U_FRMR:
        /* Non-recoverable error */
        /* TODO: */
        break;
    case LAPM_U_XID:
        if (s->configuring)
        {
            receive_xid(ss, frame, len);
            s->configuring = false;
            t401_stop(ss);
            switch (s->state)
            {
            case LAPM_IDLE:
                lapm_connect(ss);
                break;
            case LAPM_DATA:
                s->local_busy = false;
                tx_supervisory_frame(s, s->cmd_addr, LAPM_S_RR, 0);
                break;
            }
        }
        break;
    default:
        break;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void lapm_hdlc_underflow(void *user_data)
{
    lapm_state_t *s;
    v42_state_t *ss;
    v42_frame_t *f;

    ss = (v42_state_t *) user_data;
    s = &ss->lapm;
    if (s->ctrl_get != s->ctrl_put)
    {
        /* Send control frame */
        f = &s->ctrl_buf[s->ctrl_get];
        if (++s->ctrl_get >= V42_CTRL_FRAMES)
            s->ctrl_get = 0;
    }
    else
    {
        if (s->far_busy  ||  s->configuring  ||  s->state != LAPM_DATA)
        {
            hdlc_tx_flags(&s->hdlc_tx, 10);
            return;
        }
        if (s->info_get == s->info_put  &&  !tx_information_frame(ss))
        {
            hdlc_tx_flags(&s->hdlc_tx, 10);
            return;
        }
        /* Send info frame */
        f = &s->info_buf[s->info_get];
        if (++s->info_get >= V42_INFO_FRAMES)
            s->info_get = 0;

        f->buf[0] = s->cmd_addr;
        f->buf[1] = s->vs << 1;
        f->buf[2] = s->vr << 1;
        s->vs = (s->vs + 1) & 0x7F;
        if (ss->bit_timer == 0)
            t401_start(ss);
    }
    hdlc_tx_frame(&s->hdlc_tx, f->buf, f->len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE_NONSTD(void) lapm_receive(void *user_data, const uint8_t *frame, int len, int ok)
{
    lapm_state_t *s;
    v42_state_t *ss;

    ss = (v42_state_t *) user_data;
    s = &ss->lapm;
    if (len < 0)
    {
        span_log(&ss->logging, SPAN_LOG_DEBUG, "V.42 rx status is %s (%d)\n", signal_status_to_str(len), len);
        return;
    }
    if (!ok)
        return;

    switch ((frame[1] & LAPM_FRAMETYPE_MASK))
    {
    case LAPM_FRAMETYPE_I:
    case LAPM_FRAMETYPE_I_ALT:
        receive_information_frame(ss, frame, len);
        break;
    case LAPM_FRAMETYPE_S:
        if (!valid_data_state(ss))
            return;
        if (frame[0] == s->rsp_addr)
            rx_supervisory_cmd_frame(ss, frame, len);
        else
            rx_supervisory_rsp_frame(ss, frame, len);
        break;
    case LAPM_FRAMETYPE_U:
        if (frame[0] == s->rsp_addr)
            rx_unnumbered_cmd_frame(ss, frame, len);
        else
            rx_unnumbered_rsp_frame(ss, frame, len);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

static int lapm_connect(v42_state_t *ss)
{
    lapm_state_t *s;

    s = &ss->lapm;
    if (s->state != LAPM_IDLE)
        return -1;

    /* Negotiate params */
    //transmit_xid(s, s->cmd_addr);

    reset_lapm(ss);
    /* Connect */
    s->state = LAPM_ESTABLISH;
    tx_unnumbered_frame(s, s->cmd_addr, LAPM_U_SABME | LAPM_U_PF, NULL, 0);
    /* Start T401 (and not T403) */
    t401_start(ss);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int lapm_disconnect(v42_state_t *ss)
{
    lapm_state_t *s;

    s = &ss->lapm;
    s->state = LAPM_RELEASE;
    tx_unnumbered_frame(s, s->cmd_addr, LAPM_U_DISC | LAPM_U_PF, NULL, 0);
    t401_start(ss);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int lapm_config(v42_state_t *ss)
{
    lapm_state_t *s;

    s = &ss->lapm;
    s->configuring = true;
    if (s->state == LAPM_DATA)
    {
        s->local_busy = true;
        tx_supervisory_frame(s, s->cmd_addr, LAPM_S_RNR, 1);
    }
    transmit_xid(ss, s->cmd_addr);
    t401_start(ss);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void reset_lapm(v42_state_t *ss)
{
    lapm_state_t *s;

    s = &ss->lapm;
    /* Reset the LAP.M state */
    s->local_busy = false;
    s->far_busy = false;
    s->vs = 0;
    s->va = 0;
    s->vr = 0;
    /* Discard any info frames still queued for transmission */
    s->info_put = 0;
    s->info_acked = 0;
    s->info_get = 0;
    /* Discard any control frames */
    s->ctrl_put = 0;
    s->ctrl_get = 0;

    s->tx_window_size_k = ss->config.v42_tx_window_size_k;
    s->rx_window_size_k = ss->config.v42_rx_window_size_k;
    s->tx_n401 = ss->config.v42_tx_n401;
    s->rx_n401 = ss->config.v42_rx_n401;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42_stop(v42_state_t *ss)
{
    lapm_state_t *s;

    s = &ss->lapm;
    ss->bit_timer = 0;
    s->packer_process = NULL;
    lapm_disconnect(ss);
}
/*- End of function --------------------------------------------------------*/

static void restart_lapm(v42_state_t *s)
{
    if (s->calling_party)
    {
        s->bit_timer = 48*8;
        s->bit_timer_func = initiate_negotiation_expired;
    }
    else
    {
        lapm_hdlc_underflow(s);
    }
    s->lapm.packer_process = NULL;
    s->lapm.state = LAPM_IDLE;
}
/*- End of function --------------------------------------------------------*/

static void negotiation_rx_bit(v42_state_t *s, int new_bit)
{
    /* DC1 with even parity, 8-16 ones, DC1 with odd parity, 8-16 ones */
    /* uint8_t odp = "0100010001 11111111 0100010011 11111111"; */
    /* V.42 OK E , 8-16 ones, C, 8-16 ones */
    /* uint8_t adp_v42 = "0101000101  11111111  0110000101  11111111"; */
    /* V.42 disabled E, 8-16 ones, NULL, 8-16 ones */
    /* uint8_t adp_nov42 = "0101000101  11111111  0000000001  11111111"; */

    /* There may be no negotiation, so we need to process this data through the
       HDLC receiver as well */
    if (new_bit < 0)
    {
        /* Special conditions */
        span_log(&s->logging, SPAN_LOG_DEBUG, "V.42 rx status is %s (%d)\n", signal_status_to_str(new_bit), new_bit);
        return;
    }
    /*endif*/
    new_bit &= 1;
    s->neg.rxstream = (s->neg.rxstream << 1) | new_bit;
    switch (s->neg.rx_negotiation_step)
    {
    case 0:
        /* Look for some ones */
        if (new_bit)
            break;
        /*endif*/
        s->neg.rx_negotiation_step = 1;
        s->neg.rxbits = 0;
        s->neg.rxstream = ~1;
        s->neg.rxoks = 0;
        break;
    case 1:
        /* Look for the first character */
        if (++s->neg.rxbits < 9)
            break;
        /*endif*/
        s->neg.rxstream &= 0x3FF;
        if (s->calling_party  &&  s->neg.rxstream == 0x145)
        {
            s->neg.rx_negotiation_step++;
        }
        else if (!s->calling_party  &&  s->neg.rxstream == 0x111)
        {
            s->neg.rx_negotiation_step++;
        }
        else
        {
            s->neg.rx_negotiation_step = 0;
        }
        /*endif*/
        s->neg.rxbits = 0;
        s->neg.rxstream = ~0;
        break;
    case 2:
        /* Look for 8 to 16 ones */
        s->neg.rxbits++;
        if (new_bit)
            break;
        /*endif*/
        if (s->neg.rxbits >= 8  &&  s->neg.rxbits <= 16)
            s->neg.rx_negotiation_step++;
        else
            s->neg.rx_negotiation_step = 0;
        /*endif*/
        s->neg.rxbits = 0;
        s->neg.rxstream = ~1;
        break;
    case 3:
        /* Look for the second character */
        if (++s->neg.rxbits < 9)
            break;
        /*endif*/
        s->neg.rxstream &= 0x3FF;
        if (s->calling_party  &&  s->neg.rxstream == 0x185)
        {
            s->neg.rx_negotiation_step++;
        }
        else if (s->calling_party  &&  s->neg.rxstream == 0x001)
        {
            s->neg.rx_negotiation_step++;
        }
        else if (!s->calling_party  &&  s->neg.rxstream == 0x113)
        {
            s->neg.rx_negotiation_step++;
        }
        else
        {
            s->neg.rx_negotiation_step = 0;
        }
        /*endif*/
        s->neg.rxbits = 0;
        s->neg.rxstream = ~0;
        break;
    case 4:
        /* Look for 8 to 16 ones */
        s->neg.rxbits++;
        if (new_bit)
            break;
        /*endif*/
        if (s->neg.rxbits >= 8  &&  s->neg.rxbits <= 16)
        {
            if (++s->neg.rxoks >= 2)
            {
                /* HIT - we have found the "V.42 supported" pattern. */
                s->neg.rx_negotiation_step++;
                if (s->calling_party)
                {
                    t400_stop(s);
                    s->lapm.state = LAPM_IDLE;
                    report_rx_status_change(s, s->lapm.state);
                    restart_lapm(s);
                }
                else
                {
                    s->neg.odp_seen = true;
                }
                /*endif*/
                break;
            }
            /*endif*/
            s->neg.rx_negotiation_step = 1;
            s->neg.rxbits = 0;
            s->neg.rxstream = ~1;
        }
        else
        {
            s->neg.rx_negotiation_step = 0;
            s->neg.rxbits = 0;
            s->neg.rxstream = ~0;
        }
        /*endif*/
        break;
    case 5:
        /* Parked */
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static int v42_support_negotiation_tx_bit(v42_state_t *s)
{
    int bit;

    if (s->calling_party)
    {
        if (s->neg.txbits <= 0)
        {
            s->neg.txstream = 0x3FE22;
            s->neg.txbits = 36;
        }
        else if (s->neg.txbits == 18)
        {
            s->neg.txstream = 0x3FF22;
        }
        /*endif*/
        bit = s->neg.txstream & 1;
        s->neg.txstream >>= 1;
        s->neg.txbits--;
    }
    else
    {
        if (s->neg.odp_seen  &&  s->neg.txadps < 10)
        {
            if (s->neg.txbits <= 0)
            {
                if (++s->neg.txadps >= 10)
                {
                    t400_stop(s);
                    s->lapm.state = LAPM_IDLE;
                    report_rx_status_change(s, s->lapm.state);
                    s->neg.txstream = 1;
                    restart_lapm(s);
                }
                else
                {
                    s->neg.txstream = 0x3FE8A;
                    s->neg.txbits = 36;
                }
                /*endif*/
            }
            else if (s->neg.txbits == 18)
            {
                s->neg.txstream = 0x3FE86;
            }
            /*endif*/
            bit = s->neg.txstream & 1;
            s->neg.txstream >>= 1;
            s->neg.txbits--;
        }
        else
        {
            bit = 1;
        }
        /*endif*/
    }
    /*endif*/
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42_rx_bit(void *user_data, int bit)
{
    v42_state_t *s;

    s = (v42_state_t *) user_data;
    if (s->lapm.state == LAPM_DETECT)
        negotiation_rx_bit(s, bit);
    else
        hdlc_rx_put_bit(&s->lapm.hdlc_rx, bit);
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_tx_bit(void *user_data)
{
    v42_state_t *s;
    int bit;

    s = (v42_state_t *) user_data;
    if (s->bit_timer  &&  (--s->bit_timer) <= 0)
    {
        s->bit_timer = 0;
        s->bit_timer_func(s);
    }
    if (s->lapm.state == LAPM_DETECT)
        bit = v42_support_negotiation_tx_bit(s);
    else
        bit = hdlc_tx_get_bit(&s->lapm.hdlc_tx);
    /*endif*/
    return bit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_set_local_busy_status(v42_state_t *s, int busy)
{
    int previous_busy;

    previous_busy = s->lapm.local_busy;
    s->lapm.local_busy = busy;
    return previous_busy;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_get_far_busy_status(v42_state_t *s)
{
    return s->lapm.far_busy;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) v42_get_logging_state(v42_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42_set_status_callback(v42_state_t *s, modem_status_func_t status_handler, void *user_data)
{
    s->lapm.status_handler = status_handler;
    s->lapm.status_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) v42_restart(v42_state_t *s)
{
    hdlc_tx_init(&s->lapm.hdlc_tx, false, 1, true, lapm_hdlc_underflow, s);
    hdlc_rx_init(&s->lapm.hdlc_rx, false, false, 1, lapm_receive, s);

    if (s->detect)
    {
        /* We need to do the V.42 support detection sequence */
        s->neg.txstream = ~0;
        s->neg.txbits = 0;
        s->neg.rxstream = ~0;
        s->neg.rxbits = 0;
        s->neg.rxoks = 0;
        s->neg.txadps = 0;
        s->neg.rx_negotiation_step = 0;
        s->neg.odp_seen = false;
        t400_start(s);
        s->lapm.state = LAPM_DETECT;
    }
    else
    {
        /* Go directly to LAP.M mode */
        s->lapm.state = LAPM_IDLE;
        restart_lapm(s);
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(v42_state_t *) v42_init(v42_state_t *ss,
                                     bool calling_party,
                                     bool detect,
                                     get_msg_func_t iframe_get,
                                     put_msg_func_t iframe_put,
                                     void *user_data)
{
    lapm_state_t *s;

    if (ss == NULL)
    {
        if ((ss = (v42_state_t *) span_alloc(sizeof(*ss))) == NULL)
            return NULL;
    }
    memset(ss, 0, sizeof(*ss));

    s = &ss->lapm;
    ss->calling_party = calling_party;
    ss->detect = detect;
    s->iframe_get = iframe_get;
    s->iframe_get_user_data = user_data;
    s->iframe_put = iframe_put;
    s->iframe_put_user_data = user_data;

    s->state = (ss->detect)  ?  LAPM_DETECT  :  LAPM_IDLE;
    s->local_busy = false;
    s->far_busy = false;

    /* The address octet is:
        Data link connection identifier (0)
        Command/response (0 if answerer, 1 if originator)
        Extended address (1) */
    s->cmd_addr = (LAPM_DLCI_DTE_TO_DTE << 2) | ((ss->calling_party)  ?  0x02  :  0x00) | 0x01;
    s->rsp_addr = (LAPM_DLCI_DTE_TO_DTE << 2) | ((ss->calling_party)  ?  0x00  :  0x02) | 0x01;

    /* Set default values for the LAP.M parameters. These can be modified later. */
    ss->config.v42_tx_window_size_k = V42_DEFAULT_WINDOW_SIZE_K;
    ss->config.v42_rx_window_size_k = V42_DEFAULT_WINDOW_SIZE_K;
    ss->config.v42_tx_n401 = V42_DEFAULT_N_401;
    ss->config.v42_rx_n401 = V42_DEFAULT_N_401;

    /* TODO: This should be part of the V.42bis startup */
    ss->config.comp = 1;
    ss->config.comp_dict_size = 512;
    ss->config.comp_max_string = 6;

    ss->tx_bit_rate = 28800;

    reset_lapm(ss);

    span_log_init(&ss->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&ss->logging, "V.42");
    return ss;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_release(v42_state_t *s)
{
    reset_lapm(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) v42_free(v42_state_t *s)
{
    v42_release(s);
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
