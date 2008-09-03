/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t31.h - A T.31 compatible class 1 FAX modem interface.
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
 *
 * $Id: t31.h,v 1.52 2008/07/31 12:55:30 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_T31_H_)
#define _SPANDSP_T31_H_

/*! \page t31_page T.31 Class 1 FAX modem protocol handling
\section t31_page_sec_1 What does it do?
The T.31 class 1 FAX modem modules implements a class 1 interface to the FAX
modems in spandsp.

\section t31_page_sec_2 How does it work?
*/

typedef struct t31_state_s t31_state_t;

typedef int (t31_modem_control_handler_t)(t31_state_t *s, void *user_data, int op, const char *num);

#define T31_TX_BUF_LEN          (4096)
#define T31_TX_BUF_HIGH_TIDE    (4096 - 1024)
#define T31_TX_BUF_LOW_TIDE     (1024)
#define T31_MAX_HDLC_LEN        284
#define T31_T38_MAX_HDLC_LEN    260

/*!
    Analogue FAX front end channel descriptor. This defines the state of a single working
    instance of an analogue line FAX front end.
*/
typedef struct
{
    fax_modems_state_t modems;

    /*! The transmit signal handler to be used when the current one has finished sending. */
    span_tx_handler_t *next_tx_handler;
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
    t38_core_state_t t38;

    /*! \brief The number of octets to send in each image packet (non-ECM or ECM) at the current
               rate and the current specified packet interval. */
    int octets_per_data_packet;

    int timed_step;

    int ms_per_tx_chunk;
    int merge_tx_fields;

    /*! \brief TRUE is there has been some T.38 data missed */
    int missing_data;

    /*! \brief The number of times an indicator packet will be sent. Numbers greater than one
               will increase reliability for UDP transmission. Zero is valid, to suppress all
               indicator packets for TCP transmission. */
    int indicator_tx_count;

    /*! \brief The number of times a data packet which ends transmission will be sent. Numbers
               greater than one will increase reliability for UDP transmission. Zero is not valid. */
    int data_end_tx_count;

    /*! \brief The current T.38 data type being transmitted */
    int current_tx_data_type;
    /*! \brief The next queued tramsit indicator */
    int next_tx_indicator;

    int trailer_bytes;

    struct
    {
        uint8_t buf[T31_T38_MAX_HDLC_LEN];
        int len;
    } hdlc_rx;

    int current_rx_type;
    int current_tx_type;

    int32_t next_tx_samples;
    int32_t timeout_rx_samples;
    /*! \brief A "sample" count, used to time events */
    int32_t samples;
} t31_t38_front_end_state_t;

/*!
    T.31 descriptor. This defines the working state for a single instance of
    a T.31 FAX modem.
*/
struct t31_state_s
{
    at_state_t at_state;
    t31_modem_control_handler_t *modem_control_handler;
    void *modem_control_user_data;

    t31_audio_front_end_state_t audio;
    t31_t38_front_end_state_t t38_fe;
    /*! TRUE if working in T.38 mode. */
    int t38_mode;

    /*! HDLC buffer, for composing an HDLC frame from the computer to the channel. */
    struct
    {
        uint8_t buf[T31_MAX_HDLC_LEN];
        int len;
        int ptr;
        /*! \brief TRUE when the end of HDLC data from the computer has been detected. */
        int final;
    } hdlc_tx;
    /*! Buffer for data from the computer to the channel. */
    struct
    {
        uint8_t data[T31_TX_BUF_LEN];
        /*! \brief The number of bytes stored in transmit buffer. */
        int in_bytes;
        /*! \brief The number of bytes sent from the transmit buffer. */
        int out_bytes;
        /*! \brief TRUE if the flow of real data has started. */
        int data_started;
        /*! \brief TRUE if holding up further data into the buffer, for flow control. */
        int holding;
        /*! \brief TRUE when the end of non-ECM data from the computer has been detected. */
        int final;
    } tx;

    /*! TRUE if DLE prefix just used */
    int dled;

	/*! \brief Samples of silence awaited, as specified in a "wait for silence" command */
    int silence_awaited;

    /*! \brief The current bit rate for the FAX fast message transfer modem. */
    int bit_rate;
    /*! \brief TRUE if a valid HDLC frame has been received in the current reception period. */
    int rx_frame_received;

    /*! \brief Samples elapsed in the current call */
    int64_t call_samples;
    int64_t dte_data_timeout;

    /*! \brief The currently queued modem type. */
    int modem;
    /*! \brief TRUE when short training mode has been selected by the computer. */
    int short_train;
    queue_state_t *rx_queue;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

#if defined(__cplusplus)
extern "C"
{
#endif

void t31_call_event(t31_state_t *s, int event);

int t31_at_rx(t31_state_t *s, const char *t, int len);

/*! Process a block of received T.31 modem audio samples.
    \brief Process a block of received T.31 modem audio samples.
    \param s The T.31 modem context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. */
int t31_rx(t31_state_t *s, int16_t amp[], int len);

/*! Generate a block of T.31 modem audio samples.
    \brief Generate a block of T.31 modem audio samples.
    \param s The T.31 modem context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int t31_tx(t31_state_t *s, int16_t amp[], int max_len);

int t31_t38_send_timeout(t31_state_t *s, int samples);

/*! Select whether silent audio will be sent when transmit is idle.
    \brief Select whether silent audio will be sent when transmit is idle.
    \param s The T.31 modem context.
    \param transmit_on_idle TRUE if silent audio should be output when the transmitter is
           idle. FALSE to transmit zero length audio when the transmitter is idle. The default
           behaviour is FALSE.
*/
void t31_set_transmit_on_idle(t31_state_t *s, int transmit_on_idle);

/*! Select whether TEP mode will be used (or time allowed for it (when transmitting).
    \brief Select whether TEP mode will be used.
    \param s The T.31 modem context.
    \param use_tep TRUE if TEP is to be ised.
*/
void t31_set_tep_mode(t31_state_t *s, int use_tep);

/*! Select whether T.38 data will be paced as it is transmitted.
    \brief Select whether T.38 data will be paced.
    \param s The T.31 modem context.
    \param without_pacing TRUE if data is to be sent as fast as possible. FALSE if it is
           to be paced.
*/
void t31_set_t38_config(t31_state_t *s, int without_pacing);

/*! Initialise a T.31 context. This must be called before the first
    use of the context, to initialise its contents.
    \brief Initialise a T.31 context.
    \param s The T.31 context.
    \param at_tx_handler A callback routine to handle AT interpreter channel output.
    \param at_tx_user_data An opaque pointer passed in called to at_tx_handler.
    \param modem_control_handler A callback routine to handle control of the modem (off-hook, etc).
    \param modem_control_user_data An opaque pointer passed in called to modem_control_handler.
    \param tx_t38_packet_handler ???
    \param tx_t38_packet_user_data ???
    \return A pointer to the T.31 context. */
t31_state_t *t31_init(t31_state_t *s,
                      at_tx_handler_t *at_tx_handler,
                      void *at_tx_user_data,
                      t31_modem_control_handler_t *modem_control_handler,
                      void *modem_control_user_data,
                      t38_tx_packet_handler_t *tx_t38_packet_handler,
                      void *tx_t38_packet_user_data);

/*! Release a T.31 context.
    \brief Release a T.31 context.
    \param s The T.31 context.
    \return 0 for OK */
int t31_release(t31_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
