/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/t31.h - A T.31 compatible class 1 FAX modem interface.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_T31_H_)
#define _SPANDSP_PRIVATE_T31_H_

#define T31_TX_BUF_LEN          (4096)
#define T31_TX_BUF_HIGH_TIDE    (4096 - 1024)
#define T31_TX_BUF_LOW_TIDE     (1024)

#define T31_MAX_HDLC_LEN        284
/*! The maximum length of an HDLC frame buffer. This must be big enough for ECM frames. */
#define T31_T38_MAX_HDLC_LEN    260
/*! The number of HDLC transmit buffers */
#define T31_TX_HDLC_BUFS        256

/*!
    T.31 T.38 HDLC buffer.
*/
typedef struct
{
    /*! \brief HDLC message buffers. */
    uint8_t buf[T31_MAX_HDLC_LEN];
    /*! \brief HDLC message lengths. */
    int16_t len;
} t31_hdlc_buf_t;

/*!
    T.31 T.38 HDLC state.
*/
typedef struct
{
    /*! \brief HDLC message buffers. */
    t31_hdlc_buf_t buf[T31_TX_HDLC_BUFS];
    /*! \brief HDLC buffer number for input. */
    int in;
    /*! \brief HDLC buffer number for output. */
    int out;
} t31_hdlc_state_t;

/*!
    Analogue FAX front end channel descriptor. This defines the state of a single working
    instance of an analogue line FAX front end.
*/
typedef struct
{
    fax_modems_state_t modems;
    v8_state_t v8;

    /*! The transmit signal handler to be used when the current one has finished sending. */
    span_tx_handler_t next_tx_handler;
    void *next_tx_user_data;

    /*! \brief No of data bits in current_byte. */
    int bit_no;
    /*! \brief The current data byte in progress. */
    int current_byte;

    /*! \brief Rx power meter, used to detect silence. */
    power_meter_t rx_power;
    /*! \brief Last sample, used for an elementary HPF for the power meter. */
    int16_t last_sample;
    /*! \brief The current silence threshold. */
    int32_t silence_threshold_power;

    /*! \brief Samples of silence heard */
    int silence_heard;
} t31_audio_front_end_state_t;

/*!
    Analogue FAX front end channel descriptor. This defines the state of a single working
    instance of an analogue line FAX front end.
*/
typedef struct
{
    /*! \brief Internet Aware FAX mode bit mask. */
    int iaf;
    /*! \brief Required time between T.38 transmissions, in us. */
    int us_per_tx_chunk;
    /*! \brief Bit fields controlling the way data is packed into chunked for transmission. */
    int chunking_modes;

    /*! \brief Core T.38 IFP support */
    t38_core_state_t t38;

    /*! \brief The current transmit step being timed */
    int timed_step;

    /*! \brief True is there has been some T.38 data missed */
    bool rx_data_missing;

    /*! \brief The number of octets to send in each image packet (non-ECM or ECM) at the current
               rate and the current specified packet interval. */
    int octets_per_data_packet;

    /*! \brief An HDLC context used when sending HDLC messages to the terminal port
               as if it were non-ECM data (ECM mode support). */
    hdlc_tx_state_t hdlc_tx_non_ecm;
    /*! \brief An HDLC context used when receiving HDLC messages from the terminal port.
               as if it were non-ECM data (ECM mode support). */
    hdlc_rx_state_t hdlc_rx_non_ecm;

    struct
    {
        uint8_t buf[T31_T38_MAX_HDLC_LEN];
        int len;
    } hdlc_rx;

    struct
    {
        /*! \brief The number of extra bits in a fully stuffed version of the
                   contents of the HDLC transmit buffer. This is needed to accurately
                   estimate the playout time for this frame, through an analogue modem. */
        int extra_bits;
    } hdlc_tx;

    t31_hdlc_state_t hdlc_from_t31;

    /*! \brief True if we are using ECM mode. This is used to select HDLC faking, necessary
               with clunky class 1 modems. */
    bool ecm_mode;

    /*! \brief Counter for trailing non-ECM bytes, used to flush out the far end's modem. */
    int non_ecm_trailer_bytes;

    /*! \brief The next queued tramsit indicator */
    int next_tx_indicator;
    /*! \brief The current T.38 data type being transmitted */
    int current_tx_data_type;

    /*! \brief The current operating mode of the receiver. */
    int current_rx_type;
    /*! \brief The current operating mode of the transmitter. */
    int current_tx_type;

    /*! \brief Current transmission bit rate. */
    int tx_bit_rate;
    /*! \brief A "sample" count, used to time events. */
    int32_t samples;
    /*! \brief The value for samples at the next transmission point. */
    int32_t next_tx_samples;
    /*! \brief The current transmit timeout. */
    int32_t timeout_tx_samples;
    /*! \brief The current receive timeout. */
    int32_t timeout_rx_samples;
} t31_t38_front_end_state_t;

/*!
    T.31 descriptor. This defines the working state for a single instance of
    a T.31 FAX modem.
*/
struct t31_state_s
{
    at_state_t at_state;
    t31_modem_control_handler_t modem_control_handler;
    void *modem_control_user_data;

    t31_audio_front_end_state_t audio;
    t31_t38_front_end_state_t t38_fe;
    /*! True if working in T.38 mode. */
    bool t38_mode;

    /*! HDLC buffer, for composing an HDLC frame from the computer to the channel. */
    struct
    {
        /*! \brief The HDLC transmit buffer. */
        uint8_t buf[T31_MAX_HDLC_LEN];
        int len;
        int ptr;
        /*! \brief True when the end of HDLC data from the computer has been detected. */
        bool final;
    } hdlc_tx;
    /*! Buffer for data from the computer to the channel. */
    struct
    {
        /*! \brief The transmit buffer. */
        uint8_t buf[T31_TX_BUF_LEN];
        /*! \brief The number of bytes stored in the transmit buffer. */
        int in_bytes;
        /*! \brief The number of bytes sent from the transmit buffer. */
        int out_bytes;
        /*! \brief True if the flow of real data has started. */
        bool data_started;
        /*! \brief True if holding up further data into the buffer, for flow control. */
        bool holding;
        /*! \brief True when the end of non-ECM data from the computer has been detected. */
        bool final;
    } non_ecm_tx;

    /*! True if DLE prefix just used */
    bool dled;

    /*! \brief Samples of silence awaited, as specified in a "wait for silence" command */
    int silence_awaited;

    /*! \brief The current bit rate for the FAX fast message transfer modem. */
    int bit_rate;
    /*! \brief True if a valid HDLC frame has been received in the current reception period. */
    bool rx_frame_received;

    /*! \brief Samples elapsed in the current call */
    int64_t call_samples;
    int64_t dte_data_timeout;

    /*! \brief The currently queued modem type. */
    int modem;
    /*! \brief True when short training mode has been selected by the computer. */
    bool short_train;
    queue_state_t *rx_queue;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#endif
/*- End of file ------------------------------------------------------------*/
