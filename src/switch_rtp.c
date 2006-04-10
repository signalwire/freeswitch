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
#define RTP_START_PORT 16384
#define RTP_END_PORT 32768
#define SWITCH_RTP_CNG_PAYLOAD 13
#define MAX_KEY_LEN      64
#define MASTER_KEY_LEN   30

static switch_port_t NEXT_PORT = RTP_START_PORT;
static switch_mutex_t *port_lock = NULL;

typedef srtp_hdr_t rtp_hdr_t;

typedef struct {
	srtp_hdr_t header;        
	char body[SWITCH_RTP_MAX_BUF_LEN];  
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
	uint8_t payload;
	
	switch_rtp_invalid_handler invalid_handler;
	void *private_data;

	uint32_t ts;
	uint32_t flags;
	switch_memory_pool *pool;
	switch_sockaddr_t *from_addr;

	char *ice_user;
	char *user_ice;
	switch_time_t last_stun;
	uint32_t packet_size;
	switch_time_t last_read;
	switch_time_t next_read;
	uint32_t ms_per_packet;
	uint8_t stuncount;
	switch_buffer *packet_buffer;
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


SWITCH_DECLARE(switch_status) switch_rtp_set_local_address(switch_rtp *rtp_session, char *host, switch_port_t port, const char **err)
{
	*err = "Success";

	if (switch_sockaddr_info_get(&rtp_session->local_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Local Address Error!";
		return SWITCH_STATUS_FALSE;
	}

	if (rtp_session->sock) {
		switch_socket_close(rtp_session->sock);
		rtp_session->sock = NULL;
	}
	
	if (switch_socket_create(&rtp_session->sock, AF_INET, SOCK_DGRAM, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Socket Error!";
		return SWITCH_STATUS_SOCKERR;
	}

	if (switch_socket_bind(rtp_session->sock, rtp_session->local_addr) != SWITCH_STATUS_SUCCESS) {
		*err = "Bind Error!";
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK)) {
		switch_socket_opt_set(rtp_session->sock, APR_SO_NONBLOCK, TRUE);
	}
	switch_set_flag(rtp_session, SWITCH_RTP_FLAG_IO);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_rtp_set_remote_address(switch_rtp *rtp_session, char *host, switch_port_t port, const char **err)
{
	*err = "Success";

	if (switch_sockaddr_info_get(&rtp_session->remote_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Remote Address Error!";
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_rtp_create(switch_rtp **new_rtp_session,
												uint8_t payload,
												uint32_t packet_size,
												uint32_t ms_per_packet,
												switch_rtp_flag_t flags, 
												char *crypto_key,
												const char **err,
												switch_memory_pool *pool)
{
	switch_rtp *rtp_session = NULL;
	srtp_policy_t policy;
	char key[MAX_KEY_LEN];
	uint32_t ssrc = rand() & 0xffff;

	*new_rtp_session = NULL;
	
	if (packet_size > SWITCH_RTP_MAX_BUF_LEN) {
		*err = "Packet Size Too Large!";
		return SWITCH_STATUS_FALSE;
	}

	if (!(rtp_session = switch_core_alloc(pool, sizeof(*rtp_session)))) {
		*err = "Memory Error!";
		return SWITCH_STATUS_MEMERR;
	}

	rtp_session->pool = pool;

	/* for from address on recvfrom calls */
	switch_sockaddr_info_get(&rtp_session->from_addr, NULL, SWITCH_UNSPEC, 0, 0, rtp_session->pool);
	
	memset(&policy, 0, sizeof(policy));
	if (crypto_key) {
		int len;

		switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE);
		crypto_policy_set_rtp_default(&policy.rtp);
		crypto_policy_set_rtcp_default(&policy.rtcp);
		policy.ssrc.type  = ssrc_specific;
		policy.ssrc.value = ssrc;
		policy.key  = (uint8_t *) key;
		policy.next = NULL;
		policy.rtp.sec_serv = sec_serv_conf_and_auth;
		policy.rtcp.sec_serv = sec_serv_conf_and_auth;

		/*
		 * read key from hexadecimal on command line into an octet string
		 */
		len = hex_string_to_octet_string(key, crypto_key, MASTER_KEY_LEN*2);
    
		/* check that hex string is the right length */
		if (len < MASTER_KEY_LEN*2) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, 
								  "error: too few digits in key/salt "
								  "(should be %d hexadecimal digits, found %d)\n",
								  MASTER_KEY_LEN*2, len);
			return SWITCH_STATUS_FALSE;
		} 
		if (strlen(crypto_key) > MASTER_KEY_LEN*2) {
			switch_console_printf(SWITCH_CHANNEL_CONSOLE, 
								  "error: too many digits in key/salt "
								  "(should be %d hexadecimal digits, found %u)\n",
								  MASTER_KEY_LEN*2, (unsigned)strlen(crypto_key));
			return SWITCH_STATUS_FALSE;
		}
    
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "set master key/salt to %s/", octet_string_hex_string(key, 16));
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "%s\n", octet_string_hex_string(key+16, 14));
	} else {
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
	}
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
	rtp_session->ms_per_packet = ms_per_packet;
	rtp_session->packet_size = packet_size;
	rtp_session->next_read = switch_time_now() + rtp_session->ms_per_packet;
	srtp_create(&rtp_session->recv_ctx, &policy);
	srtp_create(&rtp_session->send_ctx, &policy);

	*new_rtp_session = rtp_session;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_rtp *)switch_rtp_new(char *rx_host,
										   switch_port_t rx_port,
										   char *tx_host,
										   switch_port_t tx_port,
										   uint8_t payload,
										   uint32_t packet_size,
										   uint32_t ms_per_packet,
										   switch_rtp_flag_t flags,
										   char *crypto_key,
										   const char **err,
										   switch_memory_pool *pool) 
{
	switch_rtp *rtp_session;

	if (switch_rtp_create(&rtp_session, payload, packet_size, ms_per_packet, flags, crypto_key, err, pool) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	if (switch_rtp_set_remote_address(rtp_session, tx_host, tx_port, err) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	if (switch_rtp_set_local_address(rtp_session, rx_host, rx_port, err) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

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

SWITCH_DECLARE(void) switch_rtp_set_default_packet_size(switch_rtp *rtp_session, uint16_t packet_size)
{
	rtp_session->packet_size = packet_size;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_default_packet_size(switch_rtp *rtp_session)
{
	return rtp_session->packet_size;
}

SWITCH_DECLARE(void) switch_rtp_set_default_payload(switch_rtp *rtp_session, uint8_t payload)
{
	rtp_session->payload = payload;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_default_payload(switch_rtp *rtp_session)
{
	return rtp_session->payload;
}

SWITCH_DECLARE(void) switch_rtp_set_invald_handler(switch_rtp *rtp_session, switch_rtp_invalid_handler on_invalid)
{
	rtp_session->invalid_handler = on_invalid;
}


static int rtp_common_read(switch_rtp *rtp_session, void *data, int *payload_type, switch_frame_flag *flags)
{
	switch_size_t bytes;
	switch_status status;


	for(;;) {
		bytes = sizeof(rtp_msg_t);	
		status = switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock, 0, (void *)&rtp_session->recv_msg, &bytes);

		if (bytes < 0) {
			return bytes;
		}	

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
			int sbytes = (int)bytes;
			err_status_t stat;

			stat = srtp_unprotect(rtp_session->recv_ctx, &rtp_session->recv_msg, &sbytes);
			if (stat) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE,
						"error: srtp unprotection failed with code %d%s\n", stat,
						stat == err_status_replay_fail ? " (replay check failed)" :
						stat == err_status_auth_fail ? " (auth check failed)" : "");
				return -1;
			}
			bytes = sbytes;
		}

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
				return -1;
			}

			if ((switch_time_now() - rtp_session->next_read) > 1000) {
				/* We're late! We're Late!*/
				memset(&rtp_session->recv_msg, 0, 13);
				rtp_session->recv_msg.header.pt = SWITCH_RTP_CNG_PAYLOAD;
				*flags |= SFF_CNG;
				/* RE-Sync the clock and return a CNG frame */
				rtp_session->next_read = switch_time_now() + rtp_session->ms_per_packet;
				return 13;
			}
		
			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK) && status == SWITCH_STATUS_BREAK) {
				switch_yield(1000);
				continue;
			}
		}		

		if (status == SWITCH_STATUS_BREAK || bytes == 0) {
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

		break;
	}

	rtp_session->last_read = switch_time_now();
	rtp_session->next_read += rtp_session->ms_per_packet;
	*payload_type = rtp_session->recv_msg.header.pt;

	if (*payload_type == SWITCH_RTP_CNG_PAYLOAD) {
		*flags |= SFF_CNG;
	}

	return (int)(bytes - rtp_header_len);
}

SWITCH_DECLARE(int) switch_rtp_read(switch_rtp *rtp_session, void *data, uint32_t datalen, int *payload_type, switch_frame_flag *flags)
{

	int bytes = rtp_common_read(rtp_session, data, payload_type, flags);
	
	if (bytes <= 0) {
		return bytes;
	}
	memcpy(data, rtp_session->recv_msg.body, bytes);
	return bytes;
}

SWITCH_DECLARE(int) switch_rtp_zerocopy_read(switch_rtp *rtp_session, void **data, int *payload_type, switch_frame_flag *flags)
{

	int bytes = rtp_common_read(rtp_session, data, payload_type, flags);
	*data = rtp_session->recv_msg.body;

	if (bytes <= 0) {
		return bytes;
	}

	return bytes;
}

static int rtp_common_write(switch_rtp *rtp_session, void *data, uint32_t datalen, uint8_t payload)
{
	switch_size_t bytes;

	if (rtp_session->packet_size > datalen && (payload == rtp_session->payload)) {
		if (!rtp_session->packet_buffer) {
			if (switch_buffer_create(rtp_session->pool, &rtp_session->packet_buffer, rtp_session->packet_size * 2) != SWITCH_STATUS_SUCCESS) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Buffer memory error\n");
				return -1;
			}
		}
		switch_buffer_write(rtp_session->packet_buffer, data, datalen);
		if (switch_buffer_inuse(rtp_session->packet_buffer) >= rtp_session->packet_size) {
			switch_buffer_read(rtp_session->packet_buffer, rtp_session->send_msg.body, rtp_session->packet_size);
			datalen = rtp_session->packet_size;
		} else {
			return datalen;
		}
	} else {
		memcpy(rtp_session->send_msg.body, data, datalen);
	}
	
	bytes = datalen + rtp_header_len;
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
		int sbytes = (int)bytes;
		srtp_protect(rtp_session->send_ctx, &rtp_session->send_msg, &sbytes);
		bytes = sbytes;
	}

	switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void*)&rtp_session->send_msg, &bytes);

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
	}

	return (int)bytes;

}

SWITCH_DECLARE(int) switch_rtp_write(switch_rtp *rtp_session, void *data, uint32_t datalen, uint32_t ts)
{

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}

	rtp_session->ts += ts;
	rtp_session->seq = ntohs(rtp_session->seq) + 1;
	rtp_session->seq = htons(rtp_session->seq);
	rtp_session->send_msg.header.seq = rtp_session->seq;
	rtp_session->send_msg.header.ts = htonl(rtp_session->ts);
	rtp_session->payload = (uint8_t)htonl(rtp_session->payload);

	return rtp_common_write(rtp_session, data, datalen, rtp_session->payload);

}

SWITCH_DECLARE(int) switch_rtp_write_payload(switch_rtp *rtp_session, void *data, uint16_t datalen, uint8_t payload, uint32_t ts, uint16_t mseq)
{

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}
	rtp_session->ts += ts;
	rtp_session->send_msg.header.seq = htons(mseq);
	rtp_session->send_msg.header.ts = htonl(rtp_session->ts);
	rtp_session->send_msg.header.pt = (uint8_t)htonl(payload);

	return rtp_common_write(rtp_session, data, datalen, payload);
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
