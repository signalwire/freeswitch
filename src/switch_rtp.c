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
 * switch_rtp.c -- RTP
 *
 */

#include <switch.h>
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#include <datatypes.h>
#include <srtp.h>

#define MAX_KEY_LEN      64
#define rtp_header_len 12
#define RTP_MAX_BUF_LEN 16384
#define RTP_START_PORT 16384
#define RTP_END_PORT 32768

static switch_port_t NEXT_PORT = RTP_START_PORT;
static switch_mutex_t *port_lock = NULL;

typedef srtp_hdr_t rtp_hdr_t;

typedef struct {
  srtp_hdr_t header;        
  char body[RTP_MAX_BUF_LEN];  
} rtp_msg_t;

struct switch_rtp {
	switch_socket_t *sock;

	switch_sockaddr_t *local_addr;
	rtp_msg_t send_msg;
	srtp_ctx_t *send_ctx;

	switch_sockaddr_t *remote_addr;
	rtp_msg_t recv_msg;
	srtp_ctx_t *recv_ctx;
	
	uint16_t seq;
	uint32_t payload;
	
	switch_rtp_invalid_handler invalid_handler;
	void *private_data;

	uint32_t ts;
	uint32_t flags;
	switch_memory_pool *pool;
	switch_sockaddr_t *from_addr;

	char *ice_user;
	char *user_ice;
	switch_time_t last_stun;
	uint8_t stuncount;
};

static int global_init = 0;

static switch_status ice_out(switch_rtp *rtp_session)
{

	assert(rtp_session != NULL);
	assert(rtp_session->ice_user != NULL);

	if (rtp_session->stuncount == 0) {
		uint8_t buf[256] = {0};
		switch_stun_packet_t *packet;
		unsigned int elapsed;
		switch_size_t bytes;

		if (rtp_session->last_stun) {
			elapsed = (unsigned int)((switch_time_now() - rtp_session->last_stun) / 1000);

			if (elapsed > 10000) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "No stun for a long time (PUNT!)\n");
				return SWITCH_STATUS_FALSE;
			}
		}
		
		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
		switch_stun_packet_attribute_add_username(packet, rtp_session->ice_user, 32);
		bytes = switch_stun_packet_length(packet);
		switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void *)packet, &bytes);
		rtp_session->stuncount = 25;
	} else {
		rtp_session->stuncount--;
	}
	return SWITCH_STATUS_SUCCESS;
}

static void handle_ice(switch_rtp *rtp_session, void *data, switch_size_t len)
{
	switch_stun_packet_t *packet;
	switch_stun_packet_attribute_t *attr;
	char username[33] = {0};
	unsigned char buf[512] = {0};

	memcpy(buf, data, len);
	packet = switch_stun_packet_parse(buf, sizeof(buf));
	rtp_session->last_stun = switch_time_now();
	
	switch_stun_packet_first_attribute(packet, attr);

	do {
		switch(attr->type) {
		case SWITCH_STUN_ATTR_MAPPED_ADDRESS:
			if (attr->type) {
				char ip[16];
				uint16_t port;
				switch_stun_packet_attribute_get_mapped_address(attr, ip, &port);
			}
			break;
		case SWITCH_STUN_ATTR_USERNAME:
			if(attr->type) {
				switch_stun_packet_attribute_get_username(attr, username, 32);
			}
			break;
		}
	} while (switch_stun_packet_next_attribute(attr));


	if (packet->header.type == SWITCH_STUN_BINDING_REQUEST && !strcmp(rtp_session->user_ice, username)) {
		uint8_t buf[512];
		switch_stun_packet_t *rpacket;
		char *remote_ip;
		switch_size_t bytes;

		memset(buf, 0, sizeof(buf));
		rpacket = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, packet->header.id, buf);
		switch_stun_packet_attribute_add_username(rpacket, username, 32);
		switch_sockaddr_ip_get(&remote_ip, rtp_session->from_addr);
		switch_stun_packet_attribute_add_binded_address(rpacket, remote_ip, rtp_session->from_addr->port);
		bytes = switch_stun_packet_length(rpacket);
		switch_socket_sendto(rtp_session->sock, rtp_session->from_addr, 0, (void*)rpacket, &bytes);
	}
}


SWITCH_DECLARE(void) switch_rtp_init(switch_memory_pool *pool)
{
	if (global_init) {
		return;
	}

  srtp_init();
  switch_mutex_init(&port_lock, SWITCH_MUTEX_NESTED, pool);
  global_init = 1;
}

SWITCH_DECLARE(switch_port_t) switch_rtp_request_port(void)
{
	switch_port_t port;

	switch_mutex_lock(port_lock);
	port = NEXT_PORT;
	NEXT_PORT += 2;
	if (port > RTP_END_PORT) {
		port = RTP_START_PORT;
	}
	switch_mutex_unlock(port_lock);
	return port;
}

SWITCH_DECLARE(switch_rtp *)switch_rtp_new(char *rx_ip,
										   switch_port_t rx_port,
										   char *tx_ip,
										   switch_port_t tx_port,
										   int payload,
										   switch_rtp_flag_t flags,
										   const char **err,
										   switch_memory_pool *pool)
{
	switch_socket_t *sock;
	switch_rtp *rtp_session = NULL;
	switch_sockaddr_t *rx_addr;
	switch_sockaddr_t *tx_addr;
	srtp_policy_t policy;
	char key[MAX_KEY_LEN];
	uint32_t ssrc = rand() & 0xffff;


	if (switch_sockaddr_info_get(&rx_addr, rx_ip, SWITCH_UNSPEC, rx_port, 0, pool) != SWITCH_STATUS_SUCCESS) {
		*err = "RX Address Error!";
		return NULL;
	}

	if (switch_sockaddr_info_get(&tx_addr, tx_ip, SWITCH_UNSPEC, tx_port, 0, pool) != SWITCH_STATUS_SUCCESS) {
		*err = "TX Address Error!";
		return NULL;
	}

	if (switch_socket_create(&sock, AF_INET, SOCK_DGRAM, 0, pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Socket Error!";
		return NULL;
	}

	if (switch_socket_bind(sock, rx_addr) != SWITCH_STATUS_SUCCESS) {
		*err = "Bind Error!";
		return NULL;
	}

	if (!(rtp_session = switch_core_alloc(pool, sizeof(*rtp_session)))) {
		*err = "Memory Error!";
		return NULL;
	}

	rtp_session->sock = sock;
	rtp_session->local_addr = rx_addr;
	rtp_session->remote_addr = tx_addr;
	rtp_session->pool = pool;
	switch_sockaddr_info_get(&rtp_session->from_addr, NULL, SWITCH_UNSPEC, 0, 0, rtp_session->pool);
	
	if switch_test_flag(rtp_session, SWITCH_RTP_NOBLOCK) {
		switch_socket_opt_set(rtp_session->sock, APR_SO_NONBLOCK, TRUE);
	}

    policy.key                 = (uint8_t *)key;
    policy.ssrc.type           = ssrc_specific;
    policy.ssrc.value          = ssrc;
    policy.rtp.cipher_type     = NULL_CIPHER;
    policy.rtp.cipher_key_len  = 0; 
    policy.rtp.auth_type       = NULL_AUTH;
    policy.rtp.auth_key_len    = 0;
    policy.rtp.auth_tag_len    = 0;
    policy.rtp.sec_serv        = sec_serv_none;   
    policy.rtcp.cipher_type    = NULL_CIPHER;
    policy.rtcp.cipher_key_len = 0; 
    policy.rtcp.auth_type      = NULL_AUTH;
    policy.rtcp.auth_key_len   = 0;
    policy.rtcp.auth_tag_len   = 0;
    policy.rtcp.sec_serv       = sec_serv_none;   
    policy.next                = NULL;

	rtp_session->send_msg.header.ssrc    = htonl(ssrc);
	rtp_session->send_msg.header.ts      = 0;
	rtp_session->send_msg.header.seq     = (uint16_t) rand();
	rtp_session->send_msg.header.m       = 0;
	rtp_session->send_msg.header.pt      = (uint8_t)htonl(payload);
	rtp_session->send_msg.header.version = 2;
	rtp_session->send_msg.header.p       = 0;
	rtp_session->send_msg.header.x       = 0;
	rtp_session->send_msg.header.cc      = 0;

	rtp_session->recv_msg.header.ssrc    = htonl(ssrc);
	rtp_session->recv_msg.header.ts      = 0;
	rtp_session->recv_msg.header.seq     = 0;
	rtp_session->recv_msg.header.m       = 0;
	rtp_session->recv_msg.header.pt      = (uint8_t)htonl(payload);
	rtp_session->recv_msg.header.version = 2;
	rtp_session->recv_msg.header.p       = 0;
	rtp_session->recv_msg.header.x       = 0;
	rtp_session->recv_msg.header.cc      = 0;

	rtp_session->seq = rtp_session->send_msg.header.seq;
	rtp_session->payload = payload;
	srtp_create(&rtp_session->recv_ctx, &policy);
	srtp_create(&rtp_session->send_ctx, &policy);

	return rtp_session;
}

SWITCH_DECLARE(switch_status) switch_rtp_activate_ice(switch_rtp *rtp_session, char *login, char *rlogin)
{
	char ice_user[80];
	char user_ice[80];

	snprintf(ice_user, sizeof(ice_user), "%s%s", login, rlogin);
	snprintf(user_ice, sizeof(user_ice), "%s%s", rlogin, login);
	rtp_session->ice_user = switch_core_strdup(rtp_session->pool, ice_user);
	rtp_session->user_ice = switch_core_strdup(rtp_session->pool, user_ice);

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_rtp_kill_socket(switch_rtp *rtp_session)
{
	apr_socket_shutdown(rtp_session->sock, APR_SHUTDOWN_READWRITE);
	switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_IO);
}


SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp **rtp_session)
{
	switch_rtp_kill_socket(*rtp_session);
	switch_socket_close((*rtp_session)->sock);
	*rtp_session = NULL;
	return;
}

SWITCH_DECLARE(switch_socket_t *)switch_rtp_get_rtp_socket(switch_rtp *rtp_session)
{
	return rtp_session->sock;
}

SWITCH_DECLARE(void) switch_rtp_set_invald_handler(switch_rtp *rtp_session, switch_rtp_invalid_handler on_invalid)
{
	rtp_session->invalid_handler = on_invalid;
}

SWITCH_DECLARE(int) switch_rtp_read(switch_rtp *rtp_session, void *data, uint32_t datalen, int *payload_type)
{
	switch_size_t bytes;

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
		return -1;
	}
	bytes = sizeof(rtp_msg_t);

	switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock, 0, (void *)&rtp_session->recv_msg, &bytes);
	
	if (bytes <= 0) {
		return 0;
	}
	
	if (rtp_session->recv_msg.header.version != 2) {
		if (rtp_session->recv_msg.header.version == 0 && rtp_session->ice_user) {
			handle_ice(rtp_session, (void *) &rtp_session->recv_msg, bytes);
		}	

		if (rtp_session->invalid_handler) {
			rtp_session->invalid_handler(rtp_session, rtp_session->sock, (void *) &rtp_session->recv_msg, bytes, rtp_session->from_addr);
		}
		return 0;
	}
	memcpy(data, rtp_session->recv_msg.body, bytes);
	*payload_type = rtp_session->recv_msg.header.pt;
	return (int)(bytes - rtp_header_len);
}

SWITCH_DECLARE(int) switch_rtp_zerocopy_read(switch_rtp *rtp_session, void **data, int *payload_type)
{
	switch_size_t bytes;

	*data = NULL;
	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
		return -1;
	}

	bytes = sizeof(rtp_msg_t);
	switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock, 0, (void *)&rtp_session->recv_msg, &bytes);

	if (bytes <= 0) {
		return 0;
	}

	if (rtp_session->recv_msg.header.version != 2) {
		if (rtp_session->recv_msg.header.version == 0 && rtp_session->ice_user) {
			handle_ice(rtp_session, (void *) &rtp_session->recv_msg, bytes);
		}	

		if (rtp_session->invalid_handler) {
			rtp_session->invalid_handler(rtp_session, rtp_session->sock, (void *) &rtp_session->recv_msg, bytes, rtp_session->from_addr);
		}
		return 0;
	}
	*payload_type = rtp_session->recv_msg.header.pt;
	*data = rtp_session->recv_msg.body;

	return (int)(bytes - rtp_header_len);
}

SWITCH_DECLARE(int) switch_rtp_write(switch_rtp *rtp_session, void *data, int datalen, uint32_t ts)
{
	switch_size_t bytes;

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
		return -1;
	}

	rtp_session->ts += ts;
	rtp_session->seq = ntohs(rtp_session->seq) + 1;
	rtp_session->seq = htons(rtp_session->seq);
	rtp_session->send_msg.header.seq = rtp_session->seq;
	rtp_session->send_msg.header.ts = htonl(rtp_session->ts);
	rtp_session->payload = htonl(rtp_session->payload);

	memcpy(rtp_session->send_msg.body, data, datalen);

	bytes = datalen + rtp_header_len;
	switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void*)&rtp_session->send_msg, &bytes);
	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
	}

	return (int)bytes;
}

SWITCH_DECLARE(int) switch_rtp_write_payload(switch_rtp *rtp_session, void *data, int datalen, uint8_t payload, uint32_t ts, uint16_t mseq)
{
	switch_size_t bytes;

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
		return -1;
	}
	rtp_session->ts += ts;
	rtp_session->send_msg.header.seq = htons(mseq);
	rtp_session->send_msg.header.ts = htonl(rtp_session->ts);
	rtp_session->send_msg.header.pt = (uint8_t)htonl(payload);

	memcpy(rtp_session->send_msg.body, data, datalen);
	bytes = datalen + rtp_header_len;
	switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void*)&rtp_session->send_msg, &bytes);

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
	}
	
	return (int)bytes;
}

SWITCH_DECLARE(uint32_t) switch_rtp_start(switch_rtp *rtp_session)
{
	switch_set_flag(rtp_session, SWITCH_RTP_FLAG_IO);
	return 0;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_ssrc(switch_rtp *rtp_session)
{
	return rtp_session->send_msg.header.ssrc;
}

SWITCH_DECLARE(void) switch_rtp_set_private(switch_rtp *rtp_session, void *private_data)
{
	rtp_session->private_data = private_data;
}

SWITCH_DECLARE(void *)switch_rtp_get_private(switch_rtp *rtp_session)
{
	return rtp_session->private_data;
}
