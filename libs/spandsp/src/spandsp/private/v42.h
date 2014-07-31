/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/v42.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2011 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_V42_H_)
#define _SPANDSP_PRIVATE_V42_H_

/*! Max retries (9.2.2) */
#define V42_DEFAULT_N_400               5
/*! Default for max octets in an information field (9.2.3) */
#define V42_DEFAULT_N_401               128
/*! Maximum supported value for max octets in an information field */
#define V42_MAX_N_401                   128
/*! Default window size (k) (9.2.4) */
#define V42_DEFAULT_WINDOW_SIZE_K       15
/*! Maximum supported window size (k) */
#define V42_MAX_WINDOW_SIZE_K           15

/*! The number of info frames to allocate */
#define V42_INFO_FRAMES                 (V42_MAX_WINDOW_SIZE_K + 1)
/*! The number of control frames to allocate */
#define V42_CTRL_FRAMES                 8

typedef struct
{
    /* V.42 LAP.M parameters */
    uint8_t v42_tx_window_size_k;
    uint8_t v42_rx_window_size_k;
    uint16_t v42_tx_n401;
    uint16_t v42_rx_n401;

    /* V.42bis compressor parameters */
    uint8_t comp;
    int comp_dict_size;
    int comp_max_string;
} v42_config_parameters_t;

typedef struct frame_s
{
    int len;
    uint8_t buf[4 + V42_MAX_N_401];
} v42_frame_t;

/*!
    LAP-M descriptor. This defines the working state for a single instance of LAP-M.
*/
typedef struct
{
    get_msg_func_t iframe_get;
    void *iframe_get_user_data;

    put_msg_func_t iframe_put;
    void *iframe_put_user_data;

    modem_status_func_t status_handler;
    void *status_user_data;

    hdlc_rx_state_t hdlc_rx;
    hdlc_tx_state_t hdlc_tx;

    /*! Negotiated values for the window and maximum info sizes */
    uint8_t tx_window_size_k;
    uint8_t rx_window_size_k;
    uint16_t tx_n401;
    uint16_t rx_n401;

    uint8_t cmd_addr;
    uint8_t rsp_addr;
    uint8_t vs;
    uint8_t va;
    uint8_t vr;
    int state;
    int configuring;
    bool local_busy;
    bool far_busy;
    bool rejected;
    int retry_count;

    /* The control frame buffer, and its pointers */
    int ctrl_put;
    int ctrl_get;
    v42_frame_t ctrl_buf[V42_CTRL_FRAMES];

    /* The info frame buffer, and its pointers */
    int info_put;
    int info_get;
    int info_acked;
    v42_frame_t info_buf[V42_INFO_FRAMES];

    void (*packer_process)(v42_state_t *m, int bits);
} lapm_state_t;

/*! V.42 support negotiation parameters */
typedef struct
{
    /*! Stage in negotiating V.42 support */
    int rx_negotiation_step;
    int rxbits;
    int rxstream;
    int rxoks;
    int odp_seen;
    int txbits;
    int txstream;
    int txadps;
} v42_negotiation_t;

/*!
    V.42 descriptor. This defines the working state for a single
    instance of a V.42 error corrector.
*/
struct v42_state_s
{
    /*! True if we are the calling party, otherwise false. */
    bool calling_party;
    /*! True if we should detect whether the far end is V.42 capable. False if we go
        directly to protocol establishment. */
    bool detect;

    /*! The bit rate, used to time events */
    int tx_bit_rate;

    v42_config_parameters_t config;
    v42_negotiation_t neg;
    lapm_state_t lapm;

    int bit_timer;
    void (*bit_timer_func)(v42_state_t *m);

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
