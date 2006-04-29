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


struct switch_rtp_vad_data {
	switch_core_session *session;
	switch_codec vad_codec;
	switch_codec *read_codec;
	uint32_t bg_level;
	uint32_t bg_count;
	uint32_t bg_len;
	uint32_t diff_level;
	uint8_t hangunder;
	uint8_t hangunder_hits;
	uint8_t hangover;
	uint8_t hangover_hits;
	uint8_t cng_freq;
	uint8_t cng_count;
	switch_vad_flag_t flags;
	uint32_t ts;
	uint8_t start;
	uint8_t start_count;
	uint8_t scan_freq;
	time_t next_scan;
};

struct switch_rtp {
	switch_socket_t *sock;

	switch_sockaddr_t *local_addr;
	rtp_msg_t send_msg;
	srtp_ctx_t *send_ctx;

	switch_sockaddr_t *remote_addr;
	rtp_msg_t recv_msg;
	srtp_ctx_t *recv_ctx;
	
	uint16_t seq;
	switch_payload_t payload;
	
	switch_rtp_invalid_handler invalid_handler;
	void *private_data;

	uint32_t ts;
	uint32_t flags;
	switch_memory_pool_t *pool;
	switch_sockaddr_t *from_addr;

	char *ice_user;
	char *user_ice;
	switch_time_t last_stun;
	uint32_t packet_size;
	switch_time_t last_read;
	switch_time_t next_read;
	uint32_t ms_per_packet;
	uint32_t remote_port;
	uint8_t stuncount;
	switch_buffer *packet_buffer;
	struct switch_rtp_vad_data vad_data;
};

static int global_init = 0;

static switch_status ice_out(switch_rtp_t *rtp_session)
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

			if (elapsed > 30000) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No stun for a long time (PUNT!)\n");
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

static void handle_ice(switch_rtp_t *rtp_session, void *data, switch_size_t len)
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

	//printf("[%s] [%s] [%s]\n", rtp_session->user_ice, username, !strcmp(rtp_session->user_ice, username) ? "yes" : "no");
	if ((packet->header.type == SWITCH_STUN_BINDING_REQUEST)  && !strcmp(rtp_session->user_ice, username)) {
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


SWITCH_DECLARE(void) switch_rtp_init(switch_memory_pool_t *pool)
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


SWITCH_DECLARE(switch_status) switch_rtp_set_local_address(switch_rtp_t *rtp_session, char *host, switch_port_t port, const char **err)
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

SWITCH_DECLARE(switch_status) switch_rtp_set_remote_address(switch_rtp_t *rtp_session, char *host, switch_port_t port, const char **err)
{
	*err = "Success";

	if (switch_sockaddr_info_get(&rtp_session->remote_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Remote Address Error!";
		return SWITCH_STATUS_FALSE;
	}

	rtp_session->remote_port = port;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_rtp_create(switch_rtp_t **new_rtp_session,
												switch_payload_t payload,
												uint32_t packet_size,
												uint32_t ms_per_packet,
												switch_rtp_flag_t flags, 
												char *crypto_key,
												const char **err,
												switch_memory_pool_t *pool)
{
	switch_rtp_t *rtp_session = NULL;
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
	rtp_session->flags = flags;

	/* for from address on recvfrom calls */
	switch_sockaddr_info_get(&rtp_session->from_addr, NULL, SWITCH_UNSPEC, 0, 0, rtp_session->pool);
	
	memset(&policy, 0, sizeof(policy));
	if (crypto_key) {
		int len;

		switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE);
		crypto_policy_set_rtp_default(&policy.rtp);
		crypto_policy_set_rtcp_default(&policy.rtcp);
		policy.ssrc.type  = ssrc_any_inbound;
		policy.ssrc.value = ssrc;
		policy.key  = (uint8_t *) key;
		policy.next = NULL;
		policy.rtp.sec_serv = sec_serv_conf_and_auth;
		policy.rtcp.sec_serv = sec_serv_none;

		/*
		 * read key from hexadecimal on command line into an octet string
		 */
		len = hex_string_to_octet_string(key, crypto_key, MASTER_KEY_LEN*2);
    
		/* check that hex string is the right length */
		if (len < MASTER_KEY_LEN*2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
								  "error: too few digits in key/salt "
								  "(should be %d hexadecimal digits, found %d)\n",
								  MASTER_KEY_LEN*2, len);
			*err = "Crypt Error";
			return SWITCH_STATUS_FALSE;
		} 
		if (strlen(crypto_key) > MASTER_KEY_LEN*2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
								  "error: too many digits in key/salt "
								  "(should be %d hexadecimal digits, found %u)\n",
								  MASTER_KEY_LEN*2, (unsigned)strlen(crypto_key));
			*err = "Crypt Error";
			return SWITCH_STATUS_FALSE;
		}
    
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "set master key/salt to %s/", octet_string_hex_string(key, 16));
		//switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "%s\n", octet_string_hex_string(key+16, 14));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activating Secure RTP!\n");
	}

	rtp_session->send_msg.header.ssrc    = htonl(ssrc);
	rtp_session->send_msg.header.ts      = 0;
	rtp_session->send_msg.header.seq     = (uint16_t) rand();
	rtp_session->send_msg.header.m       = 0;
	rtp_session->send_msg.header.pt      = (switch_payload_t)htonl(payload);
	rtp_session->send_msg.header.version = 2;
	rtp_session->send_msg.header.p       = 0;
	rtp_session->send_msg.header.x       = 0;
	rtp_session->send_msg.header.cc      = 0;

	rtp_session->recv_msg.header.ssrc    = htonl(ssrc);
	rtp_session->recv_msg.header.ts      = 0;
	rtp_session->recv_msg.header.seq     = 0;
	rtp_session->recv_msg.header.m       = 0;
	rtp_session->recv_msg.header.pt      = (switch_payload_t)htonl(payload);
	rtp_session->recv_msg.header.version = 2;
	rtp_session->recv_msg.header.p       = 0;
	rtp_session->recv_msg.header.x       = 0;
	rtp_session->recv_msg.header.cc      = 0;

	rtp_session->seq = rtp_session->send_msg.header.seq;
	rtp_session->payload = payload;
	rtp_session->ms_per_packet = ms_per_packet;
	rtp_session->packet_size = packet_size;
	rtp_session->next_read = switch_time_now() + rtp_session->ms_per_packet;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
		err_status_t stat;

		if ((stat = srtp_create(&rtp_session->recv_ctx, &policy))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocating srtp [%d]\n", stat);
			*err = "Crypt Error";
			return SWITCH_STATUS_FALSE;
		}
		if ((stat = srtp_create(&rtp_session->send_ctx, &policy))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocating srtp [%d]\n", stat);
			*err = "Crypt Error";
			return SWITCH_STATUS_FALSE;
		}
	}

	*new_rtp_session = rtp_session;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_rtp_t *)switch_rtp_new(char *rx_host,
										   switch_port_t rx_port,
										   char *tx_host,
										   switch_port_t tx_port,
										   switch_payload_t payload,
										   uint32_t packet_size,
										   uint32_t ms_per_packet,
										   switch_rtp_flag_t flags,
										   char *crypto_key,
										   const char **err,
										   switch_memory_pool_t *pool) 
{
	switch_rtp_t *rtp_session;

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


SWITCH_DECLARE(switch_status) switch_rtp_activate_ice(switch_rtp_t *rtp_session, char *login, char *rlogin)
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

SWITCH_DECLARE(void) switch_rtp_kill_socket(switch_rtp_t *rtp_session)
{
	apr_socket_shutdown(rtp_session->sock, APR_SHUTDOWN_READWRITE);
	switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_IO);
}


SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp_t **rtp_session)
{
	switch_rtp_kill_socket(*rtp_session);
	switch_socket_close((*rtp_session)->sock);

	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_VAD)) {
		switch_rtp_disable_vad(*rtp_session);
	}
	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_SECURE)) {
		srtp_dealloc((*rtp_session)->recv_ctx);
		srtp_dealloc((*rtp_session)->send_ctx);
	}

	*rtp_session = NULL;
	return;
}

SWITCH_DECLARE(switch_socket_t *)switch_rtp_get_rtp_socket(switch_rtp_t *rtp_session)
{
	return rtp_session->sock;
}

SWITCH_DECLARE(void) switch_rtp_set_default_packet_size(switch_rtp_t *rtp_session, uint16_t packet_size)
{
	rtp_session->packet_size = packet_size;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_default_packet_size(switch_rtp_t *rtp_session)
{
	return rtp_session->packet_size;
}

SWITCH_DECLARE(void) switch_rtp_set_default_payload(switch_rtp_t *rtp_session, switch_payload_t payload)
{
	rtp_session->payload = payload;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_default_payload(switch_rtp_t *rtp_session)
{
	return rtp_session->payload;
}

SWITCH_DECLARE(void) switch_rtp_set_invald_handler(switch_rtp_t *rtp_session, switch_rtp_invalid_handler on_invalid)
{
	rtp_session->invalid_handler = on_invalid;
}

SWITCH_DECLARE(void) switch_rtp_set_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags) 
{
	
	switch_set_flag(rtp_session, flags);

}

SWITCH_DECLARE(uint8_t) switch_rtp_test_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags) 
{
	
	return (uint8_t) switch_test_flag(rtp_session, flags);
	
}

SWITCH_DECLARE(void) switch_rtp_clear_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags) 
{
	
	switch_clear_flag(rtp_session, flags);

}

static int rtp_common_read(switch_rtp_t *rtp_session, switch_payload_t *payload_type, switch_frame_flag *flags)
{
	switch_size_t bytes;
	switch_status status;

	for(;;) {
		bytes = sizeof(rtp_msg_t);	
		status = switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock, 0, (void *)&rtp_session->recv_msg, &bytes);
		
		
		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
			return -1;
		}

		if (bytes < 0) {
			return (int)bytes;
		} else if (bytes > 0 && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
			int sbytes = (int)bytes;
			err_status_t stat;

			stat = srtp_unprotect(rtp_session->recv_ctx, &rtp_session->recv_msg.header, &sbytes);
			if (stat) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						"error: srtp unprotection failed with code %d%s\n", stat,
						stat == err_status_replay_fail ? " (replay check failed)" :
						stat == err_status_auth_fail ? " (auth check failed)" : "");
				return -1;
			}
			bytes = sbytes;
		}

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
			if ((switch_time_now() - rtp_session->next_read) > 1000) {
				/* We're late! We're Late!*/
				memset(&rtp_session->recv_msg, 0, SWITCH_RTP_CNG_PAYLOAD);
				rtp_session->recv_msg.header.pt = SWITCH_RTP_CNG_PAYLOAD;
				*flags |= SFF_CNG;
				/* Set the next waypoint and return a CNG frame */
				if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_TIMER_RECLOCK)) {
					rtp_session->next_read = switch_time_now() + rtp_session->ms_per_packet;
				} else {
					rtp_session->next_read += rtp_session->ms_per_packet;
				}
				*payload_type = SWITCH_RTP_CNG_PAYLOAD;
				return SWITCH_RTP_CNG_PAYLOAD;
			}
		
			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK) && status == SWITCH_STATUS_BREAK) {
				switch_yield(1000);
				continue;
			}
		}		

		if (status == SWITCH_STATUS_BREAK || bytes == 0) {
			return 0;
		}

		if (rtp_session->recv_msg.header.version == 2) {
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_GOOGLEHACK) && rtp_session->recv_msg.header.pt == 102) {
				rtp_session->recv_msg.header.pt = 97;
			}
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ) && rtp_session->from_addr->port && 
				(rtp_session->from_addr->port != rtp_session->remote_port)) {
				uint32_t old = rtp_session->remote_port;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Auto Changing port from %u to %u\n", old, rtp_session->from_addr->port);
				rtp_session->remote_port = rtp_session->from_addr->port;
			}
		} else {
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

	return (int) bytes;
}

SWITCH_DECLARE(switch_status) switch_rtp_read(switch_rtp_t *rtp_session, void *data, uint32_t *datalen, switch_payload_t *payload_type, switch_frame_flag *flags)
{

	int bytes = rtp_common_read(rtp_session, payload_type, flags);
	
	if (bytes < 0) {
		*datalen = 0;
		return SWITCH_STATUS_GENERR;
	} else {
		bytes -= rtp_header_len;
	}

	*datalen = bytes;

	memcpy(data, rtp_session->recv_msg.body, bytes);

	return SWITCH_STATUS_SUCCESS;	
}

SWITCH_DECLARE(switch_status) switch_rtp_zerocopy_read_frame(switch_rtp_t *rtp_session, switch_frame *frame)
{
	int bytes = rtp_common_read(rtp_session, &frame->payload, &frame->flags);

	frame->data = rtp_session->recv_msg.body;
	frame->packet = &rtp_session->recv_msg;
	frame->packetlen = bytes;
	frame->source = __FILE__;
	frame->flags |= SFF_RAW_RTP;

	if (bytes < 0) {
		frame->datalen = 0;
		return SWITCH_STATUS_GENERR;
	} else if (bytes == 0) {
		frame->datalen = 0;
		return SWITCH_STATUS_BREAK;
	} else {
		bytes -= rtp_header_len;
	}

	frame->datalen = bytes;
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status) switch_rtp_zerocopy_read(switch_rtp_t *rtp_session, void **data, uint32_t *datalen, switch_payload_t *payload_type, switch_frame_flag *flags)
{

	int bytes = rtp_common_read(rtp_session, payload_type, flags);
	*data = rtp_session->recv_msg.body;

	if (bytes < 0) {
		*datalen = 0;
		return SWITCH_STATUS_GENERR;
	} else {
		bytes -= rtp_header_len;
	}

	*datalen = bytes;
	return SWITCH_STATUS_SUCCESS;
}

static int rtp_common_write(switch_rtp_t *rtp_session, void *data, uint32_t datalen, uint8_t m, switch_payload_t payload, switch_frame_flag *flags)
{
	switch_size_t bytes;
	uint8_t packetize = (rtp_session->packet_size > datalen && (payload == rtp_session->payload)) ? 1 : 0;
	uint8_t fwd = (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) && (*flags & SFF_RAW_RTP)) ? 1 : 0;
	rtp_msg_t *send_msg;
	uint8_t send = 1;

	if (fwd) {
		bytes = datalen;
		send_msg = (rtp_msg_t *) data;
	} else {
		send_msg = &rtp_session->send_msg;
		send_msg->header.pt = payload;
		send_msg->header.m = m ? 1 : 0;
		if (packetize) {
			if (!rtp_session->packet_buffer) {
				if (switch_buffer_create(rtp_session->pool, &rtp_session->packet_buffer, rtp_session->packet_size * 2) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Buffer memory error\n");
					return -1;
				}
			}
			switch_buffer_write(rtp_session->packet_buffer, data, datalen);
			if (switch_buffer_inuse(rtp_session->packet_buffer) >= rtp_session->packet_size) {
				switch_buffer_read(rtp_session->packet_buffer, send_msg->body, rtp_session->packet_size);
				datalen = rtp_session->packet_size;
			} else {
				return datalen;
			}
		} else {
			memcpy(send_msg->body, data, datalen);
		}
		bytes = datalen + rtp_header_len;	
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
		int sbytes = (int)bytes;
		err_status_t stat;

		stat = srtp_protect(rtp_session->send_ctx, &send_msg->header, &sbytes);
		if (stat) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error: srtp unprotection failed with code %d\n", stat);
		}

		bytes = sbytes;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_GOOGLEHACK) && rtp_session->send_msg.header.pt == 97) {
		rtp_session->recv_msg.header.pt = 102;
	}
	
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VAD)) {
		int16_t decoded[SWITCH_RECCOMMENDED_BUFFER_SIZE/sizeof(int16_t)];
		uint32_t rate;
		uint32_t flags;
		uint32_t len = sizeof(decoded);
		time_t now = time(NULL);
		send = 0;

		if (rtp_session->vad_data.scan_freq && rtp_session->vad_data.next_scan <= now) {
			rtp_session->vad_data.bg_count = rtp_session->vad_data.bg_level = 0;
			rtp_session->vad_data.next_scan = now + rtp_session->vad_data.scan_freq;
			//printf("RESCAN\n");
		}

		if (switch_core_codec_decode(&rtp_session->vad_data.vad_codec,
									 rtp_session->vad_data.read_codec,
									 data,
									 datalen,
									 rtp_session->vad_data.read_codec->implementation->samples_per_second,
									 decoded,
									 &len,
									 &rate,
									 &flags) == SWITCH_STATUS_SUCCESS) {

			uint32_t energy = 0;
			uint32_t x, y = 0, z = len / sizeof(int16_t);
			uint32_t score = 0;
			
			if (z) {
				for (x = 0; x < z; x++) {
					energy += abs(decoded[y]);
					y += rtp_session->vad_data.read_codec->implementation->number_of_channels;
				}
				
				if (++rtp_session->vad_data.start_count < rtp_session->vad_data.start) {
					send = 1;
				} else {
					score = energy / z;
					if (score && (rtp_session->vad_data.bg_count < rtp_session->vad_data.bg_len)) {
						rtp_session->vad_data.bg_level += score;
						if (++rtp_session->vad_data.bg_count == rtp_session->vad_data.bg_len) {
							rtp_session->vad_data.bg_level /= rtp_session->vad_data.bg_len;
							//rtp_session->vad_data.bg_level += (rtp_session->vad_data.bg_level / 3);
							//printf("AVG %u\n", rtp_session->vad_data.bg_level);
						}
						send = 1;
					} else {
						if (score > rtp_session->vad_data.bg_level) {
							uint32_t diff = score - rtp_session->vad_data.bg_level;

							if (rtp_session->vad_data.hangover_hits) {
								rtp_session->vad_data.hangover_hits--;
							}

							if (diff >= rtp_session->vad_data.diff_level || ++rtp_session->vad_data.hangunder_hits >= rtp_session->vad_data.hangunder) {
								switch_set_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING);
								send_msg->header.m = 1;
								rtp_session->vad_data.hangover_hits = rtp_session->vad_data.hangunder_hits = rtp_session->vad_data.cng_count = 0;
								if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_EVENTS_TALK)) {
									switch_event_t *event;
									if (switch_event_create(&event, SWITCH_EVENT_TALK) == SWITCH_STATUS_SUCCESS) {
										switch_channel *channel = switch_core_session_get_channel(rtp_session->vad_data.session);
										switch_channel_event_set_data(channel, event);
										switch_event_fire(&event);
									}
								}
							} 
						} else {
							if (rtp_session->vad_data.hangunder_hits) {
								rtp_session->vad_data.hangunder_hits--;
							}
							if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING)) {
								if (++rtp_session->vad_data.hangover_hits >= rtp_session->vad_data.hangover) {
									switch_clear_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING);
									rtp_session->vad_data.hangover_hits = rtp_session->vad_data.hangunder_hits = rtp_session->vad_data.cng_count = 0;
									if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_EVENTS_NOTALK)) {
										switch_event_t *event;
										if (switch_event_create(&event, SWITCH_EVENT_NOTALK) == SWITCH_STATUS_SUCCESS) {
											switch_channel *channel = switch_core_session_get_channel(rtp_session->vad_data.session);
											switch_channel_event_set_data(channel, event);
											switch_event_fire(&event);
										}					
									}
								}
							}
						}
					}
				}
				
				if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING)) {
					send = 1;
				} else {
					if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_CNG) && ++rtp_session->vad_data.cng_count >= rtp_session->vad_data.cng_freq) {
						rtp_session->send_msg.header.pt = SWITCH_RTP_CNG_PAYLOAD;
						memset(rtp_session->send_msg.body, 255, SWITCH_RTP_CNG_PAYLOAD);
						//rtp_session->send_msg.header.ts = htonl(rtp_session->vad_data.ts);
						//rtp_session->vad_data.ts++;
						bytes = SWITCH_RTP_CNG_PAYLOAD;
						send = 1;
						rtp_session->vad_data.cng_count = 0;
					}
				}
					   
			}
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}

	if (send) {
		switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void*)send_msg, &bytes);
	}

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
	}

	return (int)bytes;

}


SWITCH_DECLARE(switch_status) switch_rtp_disable_vad(switch_rtp_t *rtp_session)
{
	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VAD)) {
		return SWITCH_STATUS_GENERR;
	}
	switch_core_codec_destroy(&rtp_session->vad_data.vad_codec);
	switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_VAD);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status) switch_rtp_enable_vad(switch_rtp_t *rtp_session, switch_core_session *session, switch_codec *codec, switch_vad_flag_t flags)
{
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VAD)) {
		return SWITCH_STATUS_GENERR;
	}
	memset(&rtp_session->vad_data, 0, sizeof(rtp_session->vad_data));
	
	if (switch_core_codec_init(&rtp_session->vad_data.vad_codec,
							   codec->codec_interface->iananame,
							   codec->implementation->samples_per_second,
							   codec->implementation->microseconds_per_frame / 1000,
							   codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL, 
							   rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}

	rtp_session->vad_data.diff_level = 400;
	rtp_session->vad_data.hangunder = 15;
	rtp_session->vad_data.hangover = 40;
	rtp_session->vad_data.bg_len = 5;
	rtp_session->vad_data.bg_count = 5;
	rtp_session->vad_data.bg_level = 300;
	rtp_session->vad_data.read_codec = codec;
	rtp_session->vad_data.session = session;
	rtp_session->vad_data.flags = flags;
	rtp_session->vad_data.cng_freq = 50;
	rtp_session->vad_data.ts = 1;
	rtp_session->vad_data.start = 0;
	rtp_session->vad_data.next_scan = time(NULL);
	rtp_session->vad_data.scan_freq = 0;
	switch_set_flag(rtp_session, SWITCH_RTP_FLAG_VAD);
	switch_set_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_CNG);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_rtp_write(switch_rtp_t *rtp_session, void *data, uint32_t datalen, uint32_t ts, switch_frame_flag *flags)
{

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}

	rtp_session->ts += ts;
	rtp_session->seq = ntohs(rtp_session->seq) + 1;
	rtp_session->seq = htons(rtp_session->seq);
	rtp_session->send_msg.header.seq = rtp_session->seq;
	rtp_session->send_msg.header.ts = htonl(rtp_session->ts);

	return rtp_common_write(rtp_session, data, datalen, 0, rtp_session->payload, flags);

}

SWITCH_DECLARE(int) switch_rtp_write_frame(switch_rtp_t *rtp_session, switch_frame *frame, uint32_t ts)
{
	uint8_t fwd = (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) && switch_test_flag(frame, SFF_RAW_RTP)) ? 1 : 0;
	uint8_t packetize = (rtp_session->packet_size > frame->datalen && (frame->payload == rtp_session->payload)) ? 1 : 0;
	void *data;
	uint32_t len;

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}
	
	if (fwd && !packetize) {
		data = frame->packet;
		len = frame->packetlen;
	} else {
		data = frame->data;
		len = frame->datalen;
		rtp_session->ts += ts;
		rtp_session->seq = ntohs(rtp_session->seq) + 1;
		rtp_session->seq = htons(rtp_session->seq);
		rtp_session->send_msg.header.seq = rtp_session->seq;
		rtp_session->send_msg.header.ts = htonl(rtp_session->ts);
	}

	return rtp_common_write(rtp_session, data, len, 0, rtp_session->payload, &frame->flags);

}

SWITCH_DECLARE(int) switch_rtp_write_manual(switch_rtp_t *rtp_session, void *data, uint16_t datalen, uint8_t m, uint8_t payload, uint32_t ts, uint16_t mseq, switch_frame_flag *flags)
{

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}

	rtp_session->ts += ts;
	rtp_session->send_msg.header.seq = htons(mseq);
	rtp_session->send_msg.header.ts = htonl(rtp_session->ts);

	return rtp_common_write(rtp_session, data, datalen, m, payload, flags);
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_ssrc(switch_rtp_t *rtp_session)
{
	return rtp_session->send_msg.header.ssrc;
}

SWITCH_DECLARE(void) switch_rtp_set_private(switch_rtp_t *rtp_session, void *private_data)
{
	rtp_session->private_data = private_data;
}

SWITCH_DECLARE(void *)switch_rtp_get_private(switch_rtp_t *rtp_session)
{
	return rtp_session->private_data;
}
