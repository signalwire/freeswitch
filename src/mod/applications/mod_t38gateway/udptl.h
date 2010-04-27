/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2009, Steve Underwood <steveu@coppice.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Contributor(s):
 * 
 * Steve Underwood <steveu@coppice.org>
 *
 * udptl.h -- UDPTL handling for T.38
 *
 */

#if !defined(FREESWITCH_UDPTL_H)
#define FREESWITCH_UDPTL_H

#define LOCAL_FAX_MAX_DATAGRAM      400
#define LOCAL_FAX_MAX_FEC_PACKETS   5

#define UDPTL_BUF_MASK              15

typedef int (udptl_rx_packet_handler_t) (void *user_data, const uint8_t msg[], int len, int seq_no);

typedef struct {
	int buf_len;
	uint8_t buf[LOCAL_FAX_MAX_DATAGRAM];
} udptl_fec_tx_buffer_t;

typedef struct {
	int buf_len;
	uint8_t buf[LOCAL_FAX_MAX_DATAGRAM];
	int fec_len[LOCAL_FAX_MAX_FEC_PACKETS];
	uint8_t fec[LOCAL_FAX_MAX_FEC_PACKETS][LOCAL_FAX_MAX_DATAGRAM];
	int fec_span;
	int fec_entries;
} udptl_fec_rx_buffer_t;

struct udptl_state_s {
	udptl_rx_packet_handler_t *rx_packet_handler;
	void *user_data;

	/*! This option indicates the error correction scheme used in transmitted UDPTL
	   packets. */
	int error_correction_scheme;

	/*! This option indicates the number of error correction entries transmitted in
	   UDPTL packets. */
	int error_correction_entries;

	/*! This option indicates the span of the error correction entries in transmitted
	   UDPTL packets (FEC only). */
	int error_correction_span;

	/*! This option indicates the maximum size of a datagram that can be accepted by
	   the remote device. */
	int far_max_datagram_size;

	/*! This option indicates the maximum size of a datagram that we are prepared to
	   accept. */
	int local_max_datagram_size;

	int verbose;

	int tx_seq_no;
	int rx_seq_no;
	int rx_expected_seq_no;

	udptl_fec_tx_buffer_t tx[UDPTL_BUF_MASK + 1];
	udptl_fec_rx_buffer_t rx[UDPTL_BUF_MASK + 1];
};

enum {
	UDPTL_ERROR_CORRECTION_NONE,
	UDPTL_ERROR_CORRECTION_FEC,
	UDPTL_ERROR_CORRECTION_REDUNDANCY
};

typedef struct udptl_state_s udptl_state_t;

#if defined(__cplusplus)
extern "C" {
#endif

/*! \brief Process an arriving UDPTL packet.
    \param s The UDPTL context.
    \param buf The UDPTL packet buffer.
    \param len The length of the packet.
    \return 0 for OK. */
	int udptl_rx_packet(udptl_state_t *s, const uint8_t buf[], int len);

/*! \brief Construct a UDPTL packet, ready for transmission.
    \param s The UDPTL context.
    \param buf The UDPTL packet buffer.
    \param msg The primary packet.
    \param len The length of the primary packet.
    \return The length of the constructed UDPTL packet. */
	int udptl_build_packet(udptl_state_t *s, uint8_t buf[], const uint8_t msg[], int msg_len);

/*! \brief Change the error correction settings of a UDPTL context.
    \param s The UDPTL context.
    \param ec_scheme One of the optional error correction schemes.
    \param span The packet span over which error correction should be applied.
    \param entries The number of error correction entries to include in packets.
    \return 0 for OK. */
	int udptl_set_error_correction(udptl_state_t *s, int ec_scheme, int span, int entries);

/*! \brief Check the error correction settings of a UDPTL context.
    \param s The UDPTL context.
    \param ec_scheme One of the optional error correction schemes.
    \param span The packet span over which error correction is being applied.
    \param entries The number of error correction being included in packets.
    \return 0 for OK. */
	int udptl_get_error_correction(udptl_state_t *s, int *ec_scheme, int *span, int *entries);

	int udptl_set_local_max_datagram(udptl_state_t *s, int max_datagram);

	int udptl_get_local_max_datagram(udptl_state_t *s);

	int udptl_set_far_max_datagram(udptl_state_t *s, int max_datagram);

	int udptl_get_far_max_datagram(udptl_state_t *s);

/*! \brief Initialise a UDPTL context.
    \param s The UDPTL context.
    \param ec_scheme One of the optional error correction schemes.
    \param span The packet span over which error correction should be applied.
    \param entries The number of error correction entries to include in packets.
    \param rx_packet_handler The callback function, used to report arriving IFP packets.
    \param user_data An opaque pointer supplied to rx_packet_handler.
    \return A pointer to the UDPTL context, or NULL if there was a problem. */
	udptl_state_t *udptl_init(udptl_state_t *s, int ec_scheme, int span, int entries, udptl_rx_packet_handler_t rx_packet_handler, void *user_data);

/*! \brief Release a UDPTL context.
    \param s The UDPTL context.
    \return 0 for OK. */
	int udptl_release(udptl_state_t *s);

#if defined(__cplusplus)
}
#endif
#endif
/*- End of file ------------------------------------------------------------*/
