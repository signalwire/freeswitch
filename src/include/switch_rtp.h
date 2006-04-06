/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_channel.h -- Media Channel Interface
 *
 */
/** 
 * @file switch_rtp.h
 * @brief RTP
 * 
 */

#ifndef SWITCH_RTP_H
#define SWITCH_RTP_H

#ifdef __cplusplus
extern "C" {
#ifdef _FORMATBUG
}
#endif
#endif


///\defgroup rtp RTP (RealTime Transport Protocol)
///\ingroup core1
///\{
typedef void (*switch_rtp_invalid_handler)(switch_rtp *rtp_session,
										   switch_socket_t *sock,
										   void *data,
										   switch_size_t datalen,
										   switch_sockaddr_t *from_addr);

/*! 
  \brief Initilize the RTP System
  \param pool the memory pool to use for long term allocations
  \note Generally called by the core_init
*/
SWITCH_DECLARE(void) switch_rtp_init(switch_memory_pool *pool);

/*! 
  \brief Request a new port to be used for media
  \return the new port to use
*/
SWITCH_DECLARE(switch_port_t) switch_rtp_request_port(void);

/*! 
  \brief prepare a new RTP session handle
  \param rx_ip the local address
  \param rx_port the local port
  \param tx_ip the remote address
  \param tx_port the remote port
  \param payload the IANA payload number
  \param flags flags to control behaviour
  \param err a pointer to resolve error messages
  \param pool a memory pool to use for the session
  \return the new RTP session or NULL on failure
*/
SWITCH_DECLARE(switch_rtp *)switch_rtp_new(char *rx_ip,
						   switch_port_t rx_port,
						   char *tx_ip,
						   switch_port_t tx_port,
						   int payload,
						   switch_rtp_flag_t flags,
						   const char **err,
						   switch_memory_pool *pool);
/*! 
  \brief Kill the socket on an existing RTP session
  \param rtp_session an RTP session to kill the socket of
*/
SWITCH_DECLARE(void) switch_rtp_kill_socket(switch_rtp *rtp_session);

/*! 
  \brief Destroy an RTP session
  \param rtp_session an RTP session to destroy
*/
SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp **rtp_session);

/*! 
  \brief Acvite ICE on an RTP session
  \return SWITCH_STATUS_SUCCESS
*/
SWITCH_DECLARE(switch_status) switch_rtp_activate_ice(switch_rtp *rtp_session, char *login, char *rlogin);

/*! 
  \brief Retrieve the socket from an existing RTP session
  \param rtp_session the RTP session to retrieve the socket from
  \return the socket from the RTP session
*/
SWITCH_DECLARE(switch_socket_t *)switch_rtp_get_rtp_socket(switch_rtp *rtp_session);

/*! 
  \brief Activate a given RTP session
  \param rtp_session the RTP session to activate
  \return 0
*/
SWITCH_DECLARE(uint32_t) switch_rtp_start(switch_rtp *rtp_session);

/*! 
  \brief Set a callback function to execute when an invalid RTP packet is encountered
  \param rtp_session the RTP session
  \param on_invalid the function to set
  \return 
*/
SWITCH_DECLARE(void) switch_rtp_set_invald_handler(switch_rtp *rtp_session, switch_rtp_invalid_handler on_invalid);

/*! 
  \brief Read data from a given RTP session
  \param rtp_session the RTP session to read from
  \param data the data to read
  \param datalen the length of the data
  \param payload_type the IANA payload of the packet
  \return the number of bytes read
*/
SWITCH_DECLARE(int) switch_rtp_read(switch_rtp *rtp_session, void *data, uint32_t datalen, int *payload_type);

/*! 
  \brief Read data from a given RTP session without copying
  \param rtp_session the RTP session to read from
  \param data a pointer to point directly to the RTP read buffer
  \param payload_type the IANA payload of the packet
  \return the number of bytes read
*/
SWITCH_DECLARE(int) switch_rtp_zerocopy_read(switch_rtp *rtp_session, void **data, int *payload_type);

/*! 
  \brief Write data to a given RTP session
  \param rtp_session the RTP session to write to
  \param data data to write
  \param datalen the size of the data
  \param ts then number of bytes to increment the timestamp by
  \return the number of bytes written
*/
SWITCH_DECLARE(int) switch_rtp_write(switch_rtp *rtp_session, void *data, int datalen, uint32_t ts);

/*! 
  \brief Write data with a specified payload and sequence number to a given RTP session
  \param rtp_session the RTP session to write to
  \param data data to write
  \param datalen the size of the data
  \param payload the IANA payload number
  \param ts then number of bytes to increment the timestamp by
  \param mseq the specific sequence number to use
  \return the number of bytes written
*/
SWITCH_DECLARE(int) switch_rtp_write_payload(switch_rtp *rtp_session, void *data, int datalen, uint8_t payload, uint32_t ts, uint16_t mseq);

/*! 
  \brief Retrieve the SSRC from a given RTP session
  \param rtp_session the RTP session to retrieve from
  \return the SSRC
*/
SWITCH_DECLARE(uint32_t) switch_rtp_get_ssrc(switch_rtp *rtp_session);

/*! 
  \brief Associate an arbitrary data pointer with and RTP session
  \param rtp_session the RTP session to assign the pointer to
  \param private_data the private data to assign
*/
SWITCH_DECLARE(void) switch_rtp_set_private(switch_rtp *rtp_session, void *private_data);

/*! 
  \brief Retrieve the private data from a given RTP session
  \param rtp_session the RTP session to retrieve the data from
  \return the pointer to the private data
*/
SWITCH_DECLARE(void *)switch_rtp_get_private(switch_rtp *rtp_session);

/*!
  \}
*/

#ifdef __cplusplus
}
#endif

#endif
