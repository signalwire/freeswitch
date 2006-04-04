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




typedef void (*switch_rtp_invalid_handler)(switch_rtp *rtp_session,
										   switch_socket_t *sock,
										   void *data,
										   switch_size_t datalen,
										   switch_sockaddr_t *from_addr);


SWITCH_DECLARE(switch_rtp *)switch_rtp_new(char *rx_ip,
						   switch_port_t rx_port,
						   char *tx_ip,
						   switch_port_t tx_port,
						   int payload,
						   switch_rtp_flag_t flags,
						   const char **err,
						   switch_memory_pool *pool);

SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp **rtp_session);
SWITCH_DECLARE(switch_socket_t *)switch_rtp_get_rtp_socket(switch_rtp *rtp_session);
SWITCH_DECLARE(void) switch_rtp_set_invald_handler(switch_rtp *rtp_session, switch_rtp_invalid_handler on_invalid);
SWITCH_DECLARE(int) switch_rtp_read(switch_rtp *rtp_session, void *data, uint32_t datalen, int *payload_type);
SWITCH_DECLARE(int) switch_rtp_zerocopy_read(switch_rtp *rtp_session, void **data, int *payload_type);
SWITCH_DECLARE(int) switch_rtp_write(switch_rtp *rtp_session, void *data, int datalen, uint32_t ts);
SWITCH_DECLARE(int) switch_rtp_write_payload(switch_rtp *rtp_session, void *data, int datalen, uint8_t payload, uint32_t ts, uint16_t mseq);
SWITCH_DECLARE(uint32_t) switch_rtp_start(switch_rtp *rtp_session);
SWITCH_DECLARE(uint32_t) switch_rtp_get_ssrc(switch_rtp *rtp_session);
SWITCH_DECLARE(void) switch_rtp_killread(switch_rtp *rtp_session);
SWITCH_DECLARE(void) switch_rtp_set_private(switch_rtp *rtp_session, void *private_data);
SWITCH_DECLARE(void *)switch_rtp_get_private(switch_rtp *rtp_session);

#ifdef __cplusplus
}
#endif

#endif
