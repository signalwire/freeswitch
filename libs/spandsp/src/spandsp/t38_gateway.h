/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t38_gateway.h - A T.38, less the packet exchange part
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005, 2006, 2007 Steve Underwood
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
 * $Id: t38_gateway.h,v 1.56 2008/08/14 14:06:05 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_T38_GATEWAY_H_)
#define _SPANDSP_T38_GATEWAY_H_

/*! \page t38_gateway_page T.38 real time FAX over IP PSTN gateway
\section t38_gateway_page_sec_1 What does it do?

The T.38 gateway facility provides a robust interface between T.38 IP packet streams and
and 8k samples/second audio streams. It provides the buffering and flow control features needed
to maximum the tolerance of jitter and packet loss on the IP network.

\section t38_gateway_page_sec_2 How does it work?
*/

#define T38_RX_BUF_LEN          2048
#define T38_TX_HDLC_BUFS        256
/* Make sure the HDLC frame buffers are big enough for ECM frames. */
#define T38_MAX_HDLC_LEN        260

typedef struct t38_gateway_state_s t38_gateway_state_t;

/*!
    T.30 real time frame handler.
    \brief T.30 real time frame handler.
    \param s The T.30 context.
    \param user_data An opaque pointer.
    \param direction TRUE for incoming, FALSE for outgoing.
    \param msg The HDLC message.
    \param len The length of the message.
*/
typedef void (t38_gateway_real_time_frame_handler_t)(t38_gateway_state_t *s,
                                                     void *user_data,
                                                     int direction,
                                                     const uint8_t *msg,
                                                     int len);

/*!
    T.38 gateway T.38 side channel descriptor.
*/
typedef struct
{
    /*! Core T.38 IFP support */
    t38_core_state_t t38;

    /*! \brief TRUE if the NSF, NSC, and NSS are to be suppressed by altering
               their contents to something the far end will not recognise. */
    int suppress_nsx_len[2];
    /*! \brief TRUE if we need to corrupt the HDLC frame in progress, so the receiver cannot
               interpret it. The two values are for the two directions. */
    int corrupt_current_frame[2];

    /*! \brief the current class of field being received - i.e. none, non-ECM or HDLC */
    int current_rx_field_class;
    /*! \brief The T.38 indicator currently in use */
    int in_progress_rx_indicator;

    /*! \brief The current T.38 data type being sent. */
    int current_tx_data_type;
} t38_gateway_t38_state_t;

/*!
    T.38 gateway audio side channel descriptor.
*/
typedef struct
{
    fax_modems_state_t modems;
    /*! \brief The current receive signal handler. Actual receiving hop between this
               and a dummy receive routine. */
    span_rx_handler_t *base_rx_handler;
} t38_gateway_audio_state_t;

typedef struct
{
    /*! \brief non-ECM and HDLC modem receive data buffer. */
    uint8_t data[T38_RX_BUF_LEN];
    /*! \brief Current pointer into the data buffer. */
    int data_ptr;
    /*! \brief The current octet being received as non-ECM data. */
    unsigned int bit_stream;
    /*! \brief The number of bits taken from the modem for the current scan row. This
               is used during non-ECM transmission will fill bit removal to see that
               T.38 packet transmissions do not stretch too far apart. */
    int bits_absorbed;
    /*! \brief The current bit number in the current non-ECM octet. */
    int bit_no;
    /*! \brief Progressively calculated CRC for HDLC messages received from a modem. */
    uint16_t crc;
    /*! \brief TRUE if non-ECM fill bits are to be stripped when sending image data. */
    int fill_bit_removal;
    /*! \brief The number of octets to send in each image packet (non-ECM or ECM) at the current
               rate and the current specified packet interval. */
    int octets_per_data_packet;

    int in_bits;
    int out_octets;
} t38_gateway_to_t38_state_t;

typedef struct
{
    /*! \brief HDLC message buffers. */
    uint8_t buf[T38_TX_HDLC_BUFS][T38_MAX_HDLC_LEN];
    /*! \brief HDLC message lengths. */
    int len[T38_TX_HDLC_BUFS];
    /*! \brief HDLC message status flags. */
    int flags[T38_TX_HDLC_BUFS];
    /*! \brief HDLC buffer contents. */
    int contents[T38_TX_HDLC_BUFS];
    /*! \brief HDLC buffer number for input. */
    int in;
    /*! \brief HDLC buffer number for output. */
    int out;
} t38_gateway_hdlc_state_t;

/*!
    T.38 gateway core descriptor.
*/
typedef struct
{
    /*! \brief A bit mask of the currently supported modem types. */
    int supported_modems;
    /*! \brief TRUE if ECM FAX mode is allowed through the gateway. */
    int ecm_allowed;

    /*! \brief TRUE if in image data modem is to use short training. This usually
               follows image_data_mode, but in ECM mode T.30 defines recovery
               conditions in which long training is used for image data. */
    int short_train;
    /*! \brief TRUE if in image data mode, as opposed to TCF mode. */
    int image_data_mode;
    /*! \brief The minimum permitted bits per FAX scan line row. */
    int min_row_bits;

    /*! \brief TRUE if we should count the next MCF as a page end, else FALSE */
    int count_page_on_mcf;
    /*! \brief The number of pages for which a confirm (MCF) message was returned. */
    int pages_confirmed;

    /*! \brief TRUE if we are in error correcting (ECM) mode */
    int ecm_mode;
    /*! \brief The current bit rate for the fast modem. */
    int fast_bit_rate;
    /*! \brief The current fast modem type. */
    int fast_modem;
    /*! \brief The type of fast receive modem currently active, which may be T38_NONE */
    int fast_rx_active;

    /*! \brief TRUE if between DCS and TCF, and we want the fast image modem to
               start in the T.38 data at a predictable time from the end of the
               V.21 signal. */
    int tcf_mode_predictable_modem_start;

    /*! \brief The number of samples until the next timeout event */
    int samples_to_timeout;

    /*! Buffer for HDLC and non-ECM data going to the T.38 channel */
    t38_gateway_to_t38_state_t to_t38;
    /*! Buffer for data going to an HDLC modem. */
    t38_gateway_hdlc_state_t hdlc_to_modem;
    /*! Buffer for data going to a non-ECM mode modem. */
    t38_non_ecm_buffer_state_t non_ecm_to_modem;

    /*! \brief A pointer to a callback routine to be called when frames are
        exchanged. */
    t38_gateway_real_time_frame_handler_t *real_time_frame_handler;
    /*! \brief An opaque pointer supplied in real time frame callbacks. */
    void *real_time_frame_user_data;
} t38_gateway_core_state_t;

/*!
    T.38 gateway state.
*/
struct t38_gateway_state_s
{
    t38_gateway_t38_state_t t38x;
    t38_gateway_audio_state_t audio;
    t38_gateway_core_state_t core;

    /*! \brief Error and flow logging control */
    logging_state_t logging;
};

typedef struct
{
    /*! \brief The current bit rate for image transfer. */
    int bit_rate;
    /*! \brief TRUE if error correcting mode is used. */
    int error_correcting_mode;
    /*! \brief The number of pages transferred so far. */
    int pages_transferred;
} t38_stats_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! \brief Initialise a gateway mode T.38 context.
    \param s The T.38 context.
    \param tx_packet_handler A callback routine to encapsulate and transmit T.38 packets.
    \param tx_packet_user_data An opaque pointer passed to the tx_packet_handler routine.
    \return A pointer to the termination mode T.38 context, or NULL if there was a problem. */
t38_gateway_state_t *t38_gateway_init(t38_gateway_state_t *s,
                                      t38_tx_packet_handler_t *tx_packet_handler,
                                      void *tx_packet_user_data);

/*! Free a gateway mode T.38 context.
    \brief Free a T.38 context.
    \param s The T.38 context.
    \return 0 for OK, else -1. */
int t38_gateway_free(t38_gateway_state_t *s);

/*! Process a block of received FAX audio samples.
    \brief Process a block of received FAX audio samples.
    \param s The T.38 context.
    \param amp The audio sample buffer.
    \param len The number of samples in the buffer.
    \return The number of samples unprocessed. */
int t38_gateway_rx(t38_gateway_state_t *s, int16_t amp[], int len);

/*! Generate a block of FAX audio samples.
    \brief Generate a block of FAX audio samples.
    \param s The T.38 context.
    \param amp The audio sample buffer.
    \param max_len The number of samples to be generated.
    \return The number of samples actually generated.
*/
int t38_gateway_tx(t38_gateway_state_t *s, int16_t amp[], int max_len);

/*! Control whether error correcting mode (ECM) is allowed.
    \brief Control whether error correcting mode (ECM) is allowed.
    \param s The T.38 context.
    \param ecm_allowed TRUE is ECM is to be allowed.
*/
void t38_gateway_set_ecm_capability(t38_gateway_state_t *s, int ecm_allowed);

/*! Select whether silent audio will be sent when transmit is idle.
    \brief Select whether silent audio will be sent when transmit is idle.
    \param s The T.38 context.
    \param transmit_on_idle TRUE if silent audio should be output when the FAX transmitter is
           idle. FALSE to transmit zero length audio when the FAX transmitter is idle. The default
           behaviour is FALSE.
*/
void t38_gateway_set_transmit_on_idle(t38_gateway_state_t *s, int transmit_on_idle);

/*! Specify which modem types are supported by a T.30 context.
    \brief Specify supported modems.
    \param s The T.38 context.
    \param supported_modems Bit field list of the supported modems.
*/
void t38_gateway_set_supported_modems(t38_gateway_state_t *s, int supported_modems);

/*! Select whether NSC, NSF, and NSS should be suppressed. It selected, the contents of
    these messages are forced to zero for all octets beyond the message type. This makes
    them look like manufacturer specific messages, from a manufacturer which does not exist.
    \brief Select whether NSC, NSF, and NSS should be suppressed.
    \param s The T.38 context.
    \param from_t38 A string of bytes to overwrite the header of any NSC, NSF, and NSS
           frames passing through the gateway from T.38 the the modem.
    \param from_t38_len The length of the overwrite string.
    \param from_modem A string of bytes to overwrite the header of any NSC, NSF, and NSS
           frames passing through the gateway from the modem to T.38.
    \param from_modem_len The length of the overwrite string.
*/
void t38_gateway_set_nsx_suppression(t38_gateway_state_t *s,
                                     const uint8_t *from_t38,
                                     int from_t38_len,
                                     const uint8_t *from_modem,
                                     int from_modem_len);

/*! Select whether talker echo protection tone will be sent for the image modems.
    \brief Select whether TEP will be sent for the image modems.
    \param s The T.38 context.
    \param use_tep TRUE if TEP should be sent.
*/
void t38_gateway_set_tep_mode(t38_gateway_state_t *s, int use_tep);

/*! Select whether non-ECM fill bits are to be removed during transmission.
    \brief Select whether non-ECM fill bits are to be removed during transmission.
    \param s The T.38 context.
    \param remove TRUE if fill bits are to be removed.
*/
void t38_gateway_set_fill_bit_removal(t38_gateway_state_t *s, int remove);

/*! Get the current transfer statistics for the current T.38 session.
    \brief Get the current transfer statistics.
    \param s The T.38 context.
    \param t A pointer to a buffer for the statistics. */
void t38_gateway_get_transfer_statistics(t38_gateway_state_t *s, t38_stats_t *t);

/*! Set a callback function for T.30 frame exchange monitoring. This is called from the heart
    of the signal processing, so don't take too long in the handler routine.
    \brief Set a callback function for T.30 frame exchange monitoring.
    \param s The T.30 context.
    \param handler The callback function.
    \param user_data An opaque pointer passed to the callback function. */
void t38_gateway_set_real_time_frame_handler(t38_gateway_state_t *s,
                                             t38_gateway_real_time_frame_handler_t *handler,
                                             void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
