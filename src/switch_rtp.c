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
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 *
 *
 * switch_rtp.c -- RTP
 *
 */

#include <switch.h>
#include <switch_stun.h>
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#undef VERSION
#undef PACKAGE
#undef inline
#include <datatypes.h>
#include <srtp.h>

#define READ_INC(rtp_session) switch_mutex_lock(rtp_session->read_mutex); rtp_session->reading++
#define READ_DEC(rtp_session)  switch_mutex_unlock(rtp_session->read_mutex); rtp_session->reading--
#define WRITE_INC(rtp_session)  switch_mutex_lock(rtp_session->write_mutex); rtp_session->writing++
#define WRITE_DEC(rtp_session) switch_mutex_unlock(rtp_session->write_mutex); rtp_session->writing--

#include "stfu.h"

#define rtp_header_len 12
#define RTP_START_PORT 16384
#define RTP_END_PORT 32768
#define MASTER_KEY_LEN   30
#define RTP_MAGIC_NUMBER 42
#define MAX_SRTP_ERRS 10

static switch_port_t START_PORT = RTP_START_PORT;
static switch_port_t END_PORT = RTP_END_PORT;
static switch_port_t NEXT_PORT = RTP_START_PORT;
static switch_mutex_t *port_lock = NULL;

typedef srtp_hdr_t rtp_hdr_t;

#ifdef _MSC_VER
#pragma pack(4)
#endif

#ifdef _MSC_VER
#pragma pack()
#endif

static switch_hash_t *alloc_hash = NULL;

typedef struct {
	srtp_hdr_t header;
	char body[SWITCH_RTP_MAX_BUF_LEN];
} rtp_msg_t;

struct switch_rtp_vad_data {
	switch_core_session_t *session;
	switch_codec_t vad_codec;
	switch_codec_t *read_codec;
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

struct switch_rtp_rfc2833_data {
	switch_queue_t *dtmf_queue;
	char out_digit;
	unsigned char out_digit_packet[4];
	unsigned int out_digit_sofar;
	unsigned int out_digit_sub_sofar;
	unsigned int out_digit_dur;
	uint16_t in_digit_seq;
	uint32_t in_digit_ts;
	uint32_t timestamp_dtmf;
	uint16_t last_duration;
	uint32_t flip;
	char first_digit;
	char last_digit;
	switch_queue_t *dtmf_inqueue;
	switch_mutex_t *dtmf_mutex;
};

struct switch_rtp {
	/* 
	 * Two sockets are needed because we might be transcoding protocol families
	 * (e.g. receive over IPv4 and send over IPv6). In case the protocol
	 * families are equal, sock_input == sock_output and only one socket is
	 * used.
	 */
	switch_socket_t *sock_input, *sock_output;

	switch_sockaddr_t *local_addr;
	rtp_msg_t send_msg;

	switch_sockaddr_t *remote_addr;
	rtp_msg_t recv_msg;


	switch_sockaddr_t *remote_stun_addr;

	uint32_t autoadj_window;
	uint32_t autoadj_tally;

	srtp_ctx_t *send_ctx;
	srtp_ctx_t *recv_ctx;
	srtp_policy_t send_policy;
	srtp_policy_t recv_policy;
	uint32_t srtp_errs;

	uint16_t seq;
	uint32_t ssrc;
	uint8_t sending_dtmf;
	switch_payload_t payload;
	switch_payload_t rpayload;
	switch_rtp_invalid_handler_t invalid_handler;
	void *private_data;
	uint32_t ts;
	uint32_t last_write_ts;
	uint32_t last_write_samplecount;
	uint32_t next_write_samplecount;
	uint32_t flags;
	switch_memory_pool_t *pool;
	switch_sockaddr_t *from_addr;
	char *rx_host;
	switch_port_t rx_port;
	char *ice_user;
	char *user_ice;
	char *timer_name;
	char *remote_host_str;
	switch_time_t last_stun;
	uint32_t samples_per_interval;
	uint32_t conf_samples_per_interval;
	uint32_t rsamples_per_interval;
	uint32_t ms_per_packet;
	switch_port_t remote_port;
	uint32_t stuncount;
	uint32_t funny_stun;
	uint32_t default_stuncount;
	struct switch_rtp_vad_data vad_data;
	struct switch_rtp_rfc2833_data dtmf_data;
	switch_payload_t te;
	switch_payload_t cng_pt;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *read_mutex;
	switch_mutex_t *write_mutex;
	switch_timer_t timer;
	uint8_t ready;
	uint8_t cn;
	switch_time_t last_time;
	stfu_instance_t *jb;
	uint32_t max_missed_packets;
	uint32_t missed_count;
	rtp_msg_t write_msg;
	switch_rtp_crypto_key_t *crypto_keys[SWITCH_RTP_CRYPTO_MAX];
	int reading;
	int writing;
	char *stun_ip;
	switch_port_t stun_port;
	int from_auto;
	uint32_t cng_count;
};

static int global_init = 0;
static int rtp_common_write(switch_rtp_t *rtp_session,
							rtp_msg_t *send_msg, void *data, uint32_t datalen, switch_payload_t payload, uint32_t timestamp, switch_frame_flag_t *flags);


static switch_status_t do_stun_ping(switch_rtp_t *rtp_session)
{
	uint8_t buf[256] = { 0 };
	uint8_t *start = buf;
	switch_stun_packet_t *packet;
	//unsigned int elapsed;
	switch_size_t bytes;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(rtp_session != NULL);

	WRITE_INC(rtp_session);

	if (rtp_session->stuncount != 0) {
		rtp_session->stuncount--;
		goto end;
	}

#if 0
	if (rtp_session->last_stun) {
		elapsed = (unsigned int) ((switch_timestamp_now() - rtp_session->last_stun) / 1000);
		
		if (elapsed > 30000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No stun for a long time (PUNT!)\n");
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	}
#endif

	if (rtp_session->funny_stun) {
		*start++ = 0;
		*start++ = 0;
		*start++ = 0x22;
		*start++ = 0x22;
	}

	packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, start);
	bytes = switch_stun_packet_length(packet);

	if (rtp_session->funny_stun) {
		packet = (switch_stun_packet_t *) buf;
		bytes += 4;
	}


	switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_stun_addr, 0, (void *) packet, &bytes);
	rtp_session->stuncount = rtp_session->default_stuncount;
	
  end:
	WRITE_DEC(rtp_session);

	return status;
}

static switch_status_t ice_out(switch_rtp_t *rtp_session)
{
	uint8_t buf[256] = { 0 };
	switch_stun_packet_t *packet;
	unsigned int elapsed;
	switch_size_t bytes;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(rtp_session != NULL);
	switch_assert(rtp_session->ice_user != NULL);

	WRITE_INC(rtp_session);

	if (rtp_session->stuncount != 0) {
		rtp_session->stuncount--;
		goto end;
	}

	if (rtp_session->last_stun) {
		elapsed = (unsigned int) ((switch_timestamp_now() - rtp_session->last_stun) / 1000);

		if (elapsed > 30000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No stun for a long time (PUNT!)\n");
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	}

	packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
	switch_stun_packet_attribute_add_username(packet, rtp_session->ice_user, 32);
	bytes = switch_stun_packet_length(packet);
	switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, (void *) packet, &bytes);
	rtp_session->stuncount = rtp_session->default_stuncount;

  end:
	WRITE_DEC(rtp_session);

	return status;
}


static void handle_stun_ping_reply(switch_rtp_t *rtp_session, void *data, switch_size_t len)
{
	if (!switch_rtp_ready(rtp_session)) {
		return;
	}

	rtp_session->last_stun = switch_timestamp_now();
}

static void handle_ice(switch_rtp_t *rtp_session, void *data, switch_size_t len)
{
	switch_stun_packet_t *packet;
	switch_stun_packet_attribute_t *attr;
	void *end_buf;
	char username[33] = { 0 };
	unsigned char buf[512] = { 0 };
	switch_size_t cpylen = len;

	if (!switch_rtp_ready(rtp_session)) {
		return;
	}

	READ_INC(rtp_session);
	WRITE_INC(rtp_session);

	if (!switch_rtp_ready(rtp_session)) {
		goto end;
	}


	if (cpylen > 512) {
		cpylen = 512;
	}

	memcpy(buf, data, cpylen);
	packet = switch_stun_packet_parse(buf, sizeof(buf));
	if (!packet) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid STUN/ICE packet received\n");
		goto end;
	}
	end_buf = buf + ((sizeof(buf) > packet->header.length) ? packet->header.length : sizeof(buf));

	rtp_session->last_stun = switch_timestamp_now();

	switch_stun_packet_first_attribute(packet, attr);

	do {
		switch (attr->type) {
		case SWITCH_STUN_ATTR_MAPPED_ADDRESS:
			if (attr->type) {
				char ip[16];
				uint16_t port;
				switch_stun_packet_attribute_get_mapped_address(attr, ip, &port);
			}
			break;
		case SWITCH_STUN_ATTR_USERNAME:
			if (attr->type) {
				switch_stun_packet_attribute_get_username(attr, username, 32);
			}
			break;
		}
	} while (switch_stun_packet_next_attribute(attr, end_buf));

	if ((packet->header.type == SWITCH_STUN_BINDING_REQUEST) && !strcmp(rtp_session->user_ice, username)) {
		uint8_t stunbuf[512];
		switch_stun_packet_t *rpacket;
		const char *remote_ip;
		switch_size_t bytes;
		char ipbuf[25];

		memset(stunbuf, 0, sizeof(stunbuf));
		rpacket = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, packet->header.id, stunbuf);
		switch_stun_packet_attribute_add_username(rpacket, username, 32);
		remote_ip = switch_get_addr(ipbuf, sizeof(ipbuf), rtp_session->from_addr);
		switch_stun_packet_attribute_add_binded_address(rpacket, (char *) remote_ip, switch_sockaddr_get_port(rtp_session->from_addr));
		bytes = switch_stun_packet_length(rpacket);
		switch_socket_sendto(rtp_session->sock_output, rtp_session->from_addr, 0, (void *) rpacket, &bytes);
	}

  end:

	READ_DEC(rtp_session);
	WRITE_DEC(rtp_session);
}


SWITCH_DECLARE(void) switch_rtp_init(switch_memory_pool_t *pool)
{
	if (global_init) {
		return;
	}
	switch_core_hash_init(&alloc_hash, pool);
	srtp_init();
	switch_mutex_init(&port_lock, SWITCH_MUTEX_NESTED, pool);
	global_init = 1;
}

SWITCH_DECLARE(void) switch_rtp_get_random(void *buf, uint32_t len)
{
	crypto_get_random(buf, len);
}


SWITCH_DECLARE(void) switch_rtp_shutdown(void)
{
	switch_core_hash_destroy(&alloc_hash);
}

SWITCH_DECLARE(switch_port_t) switch_rtp_set_start_port(switch_port_t port)
{
	if (port) {
		if (port_lock) {
			switch_mutex_lock(port_lock);
		}
		if (NEXT_PORT == START_PORT) {
			NEXT_PORT = port;
		}
		START_PORT = port;
		if (NEXT_PORT < START_PORT) {
			NEXT_PORT = START_PORT;
		}
		if (port_lock) {
			switch_mutex_unlock(port_lock);
		}
	}
	return START_PORT;
}

SWITCH_DECLARE(switch_port_t) switch_rtp_set_end_port(switch_port_t port)
{
	if (port) {
		if (port_lock) {
			switch_mutex_lock(port_lock);
		}
		END_PORT = port;
		if (NEXT_PORT > END_PORT) {
			NEXT_PORT = START_PORT;
		}
		if (port_lock) {
			switch_mutex_unlock(port_lock);
		}
	}
	return END_PORT;
}

SWITCH_DECLARE(void) switch_rtp_release_port(const char *ip, switch_port_t port)
{
	switch_core_port_allocator_t *alloc = NULL;

	if (!ip) {
		return;
	}

	switch_mutex_lock(port_lock);
	if ((alloc = switch_core_hash_find(alloc_hash, ip))) {
		switch_core_port_allocator_free_port(alloc, port);
	}
	switch_mutex_unlock(port_lock);

}

SWITCH_DECLARE(switch_port_t) switch_rtp_request_port(const char *ip)
{
	switch_port_t port = 0;
	switch_core_port_allocator_t *alloc = NULL;

	switch_mutex_lock(port_lock);
	alloc = switch_core_hash_find(alloc_hash, ip);
	if (!alloc) {
		if (switch_core_port_allocator_new(START_PORT, END_PORT, SPF_EVEN, &alloc) != SWITCH_STATUS_SUCCESS) {
			abort();
		}

		switch_core_hash_insert(alloc_hash, ip, alloc);
	}

	if (switch_core_port_allocator_request_port(alloc, &port) != SWITCH_STATUS_SUCCESS) {
		port = 0;
	}

	switch_mutex_unlock(port_lock);
	return port;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_set_local_address(switch_rtp_t *rtp_session, const char *host, switch_port_t port, const char **err)
{
	switch_socket_t *new_sock = NULL, *old_sock = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
#ifndef WIN32
	char o[5] = "TEST", i[5] = "";
	switch_size_t len, ilen = 0;
	int x;
#endif

	if (rtp_session->ready != 1) {
		if (!switch_rtp_ready(rtp_session)) {
			return SWITCH_STATUS_FALSE;
		}

		WRITE_INC(rtp_session);
		READ_INC(rtp_session);

		if (!switch_rtp_ready(rtp_session)) {
			goto done;
		}
	}


	*err = NULL;

	if (switch_strlen_zero(host) || !port) {
		*err = "Address Error";
		goto done;
	}

	if (switch_sockaddr_info_get(&rtp_session->local_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Local Address Error!";
		goto done;
	}

	if (rtp_session->sock_input) {
		switch_rtp_kill_socket(rtp_session);
	}

	if (switch_socket_create(&new_sock, switch_sockaddr_get_family(rtp_session->local_addr), SOCK_DGRAM, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Socket Error!";
		goto done;
	}

	if (switch_socket_opt_set(new_sock, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
		*err = "Socket Error!";
		goto done;
	}

	if (switch_socket_bind(new_sock, rtp_session->local_addr) != SWITCH_STATUS_SUCCESS) {
		*err = "Bind Error!";
		goto done;
	}
#ifndef WIN32
	len = sizeof(i);
	switch_socket_opt_set(new_sock, SWITCH_SO_NONBLOCK, TRUE);

	switch_socket_sendto(new_sock, rtp_session->local_addr, 0, (void *) o, &len);

	x = 0;
	while (!ilen) {
		switch_status_t status;
		ilen = len;
		status = switch_socket_recvfrom(rtp_session->from_addr, new_sock, 0, (void *) i, &ilen);

		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			break;
		}

		if (++x > 1000) {
			break;
		}
		switch_cond_next();
	}
	switch_socket_opt_set(new_sock, SWITCH_SO_NONBLOCK, FALSE);

#endif

	old_sock = rtp_session->sock_input;
	rtp_session->sock_input = new_sock;
	new_sock = NULL;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK)) {
		switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, TRUE);
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
	}

	status = SWITCH_STATUS_SUCCESS;
	*err = "Success";
	switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_IO);

  done:

	if (new_sock) {
		switch_socket_close(new_sock);
	}

	if (old_sock) {
		switch_socket_close(old_sock);
	}

	if (rtp_session->ready != 1) {
		WRITE_DEC(rtp_session);
		READ_DEC(rtp_session);
	}

	return status;
}

SWITCH_DECLARE(void) switch_rtp_set_max_missed_packets(switch_rtp_t *rtp_session, uint32_t max)
{
	rtp_session->max_missed_packets = max;
}

SWITCH_DECLARE(char *) switch_rtp_get_remote_host(switch_rtp_t *rtp_session)
{
	return switch_strlen_zero(rtp_session->remote_host_str) ? "0.0.0.0" : rtp_session->remote_host_str;
}

SWITCH_DECLARE(switch_port_t) switch_rtp_get_remote_port(switch_rtp_t *rtp_session)
{
	return rtp_session->remote_port;
}


SWITCH_DECLARE(switch_status_t) switch_rtp_set_remote_address(switch_rtp_t *rtp_session, const char *host, switch_port_t port, 
															  switch_bool_t change_adv_addr, const char **err)
{
	switch_sockaddr_t *remote_addr;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	*err = "Success";

	if (switch_sockaddr_info_get(&remote_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS || !remote_addr) {
		*err = "Remote Address Error!";
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(rtp_session->write_mutex);

	rtp_session->remote_addr = remote_addr;

	if (change_adv_addr) {
		rtp_session->remote_host_str = switch_core_strdup(rtp_session->pool, host);
		rtp_session->remote_port = port;
	}

	if (rtp_session->sock_input &&
		switch_sockaddr_get_family(rtp_session->remote_addr) ==
		switch_sockaddr_get_family(rtp_session->local_addr)) {
			rtp_session->sock_output = rtp_session->sock_input;
	}
	else {
		if (rtp_session->sock_output &&
			rtp_session->sock_output != rtp_session->sock_input) {
				switch_socket_close(rtp_session->sock_output);
		}
		if ((status = switch_socket_create(&rtp_session->sock_output,
			switch_sockaddr_get_family(rtp_session->remote_addr),
			SOCK_DGRAM, 0, rtp_session->pool)) !=
			SWITCH_STATUS_SUCCESS)
		{
			*err = "Socket Error!";
		}
	}

	switch_mutex_unlock(rtp_session->write_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_add_crypto_key(switch_rtp_t *rtp_session,
														  switch_rtp_crypto_direction_t direction,
														  uint32_t index, switch_rtp_crypto_key_type_t type, unsigned char *key, switch_size_t keylen)
{
	switch_rtp_crypto_key_t *crypto_key;
	srtp_policy_t *policy;
	err_status_t stat;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (direction >= SWITCH_RTP_CRYPTO_MAX || keylen > SWITCH_RTP_MAX_CRYPTO_LEN) {
		return SWITCH_STATUS_FALSE;
	}

	crypto_key = switch_core_alloc(rtp_session->pool, sizeof(*crypto_key));

	if (direction == SWITCH_RTP_CRYPTO_RECV) {
		policy = &rtp_session->recv_policy;
	} else {
		policy = &rtp_session->send_policy;
	}

	crypto_key->type = type;
	crypto_key->index = index;
	memcpy(crypto_key->key, key, keylen);
	crypto_key->next = rtp_session->crypto_keys[direction];
	rtp_session->crypto_keys[direction] = crypto_key;

	memset(policy, 0, sizeof(*policy));

	switch (crypto_key->type) {
	case AES_CM_128_HMAC_SHA1_80:
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy->rtp);
		break;
	case AES_CM_128_HMAC_SHA1_32:
		crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy->rtp);
		break;
	case AES_CM_128_NULL_AUTH:
		crypto_policy_set_aes_cm_128_null_auth(&policy->rtp);
		break;
	default:
		break;
	}

	policy->next = NULL;
	policy->key = (uint8_t *) crypto_key->key;
	crypto_policy_set_rtcp_default(&policy->rtcp);
	policy->rtcp.sec_serv = sec_serv_none;

	policy->rtp.sec_serv = sec_serv_conf_and_auth;
	switch (direction) {
	case SWITCH_RTP_CRYPTO_RECV:
		policy->ssrc.type = ssrc_any_inbound;

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV)) {
			switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV_RESET);
		} else {
			if ((stat = srtp_create(&rtp_session->recv_ctx, policy))) {
				status = SWITCH_STATUS_FALSE;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activating Secure RTP RECV\n");
				switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocating srtp [%d]\n", stat);
				return status;
			}
		}
		break;
	case SWITCH_RTP_CRYPTO_SEND:
		policy->ssrc.type = ssrc_specific;
		policy->ssrc.value = rtp_session->ssrc;

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
			switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET);
		} else {
			if ((stat = srtp_create(&rtp_session->send_ctx, policy))) {
				status = SWITCH_STATUS_FALSE;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activating Secure RTP SEND\n");
				switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error allocating SRTP [%d]\n", stat);
				return status;
			}
		}

		break;
	default:
		abort();
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_change_interval(switch_rtp_t *rtp_session, uint32_t ms_per_packet, uint32_t samples_per_interval)
{
	rtp_session->ms_per_packet = ms_per_packet;
	rtp_session->samples_per_interval = rtp_session->conf_samples_per_interval = samples_per_interval;
	rtp_session->missed_count = 0;

	if (rtp_session->timer_name) {
		if (rtp_session->timer.timer_interface) {
			switch_core_timer_destroy(&rtp_session->timer);
		}
		if (switch_core_timer_init(&rtp_session->timer, 
								   rtp_session->timer_name, ms_per_packet / 1000, samples_per_interval, rtp_session->pool) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "RE-Starting timer [%s] %d bytes per %dms\n", rtp_session->timer_name, samples_per_interval, ms_per_packet);
		} else {
			memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "Problem RE-Starting timer [%s] %d bytes per %dms\n", rtp_session->timer_name, samples_per_interval, ms_per_packet / 1000);
			return SWITCH_STATUS_FALSE;
		}
	}
		
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_create(switch_rtp_t **new_rtp_session,
												  switch_payload_t payload,
												  uint32_t samples_per_interval,
												  uint32_t ms_per_packet,
												  switch_rtp_flag_t flags, char *timer_name, const char **err, switch_memory_pool_t *pool)
{
	switch_rtp_t *rtp_session = NULL;

	*new_rtp_session = NULL;

	if (samples_per_interval > SWITCH_RTP_MAX_BUF_LEN) {
		*err = "Packet Size Too Large!";
		return SWITCH_STATUS_FALSE;
	}

	if (!(rtp_session = switch_core_alloc(pool, sizeof(*rtp_session)))) {
		*err = "Memory Error!";
		return SWITCH_STATUS_MEMERR;
	}

	rtp_session->pool = pool;
	rtp_session->te = 101;


	switch_mutex_init(&rtp_session->flag_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->read_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->write_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->dtmf_data.dtmf_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_queue_create(&rtp_session->dtmf_data.dtmf_queue, 100, rtp_session->pool);
	switch_queue_create(&rtp_session->dtmf_data.dtmf_inqueue, 100, rtp_session->pool);

	switch_rtp_set_flag(rtp_session, flags);

	/* for from address on recvfrom calls */
	switch_sockaddr_info_get(&rtp_session->from_addr, NULL, SWITCH_UNSPEC, 0, 0, pool);


	rtp_session->seq = (uint16_t) rand();
	rtp_session->ssrc = (uint32_t) ((intptr_t) &rtp_session + (uint32_t) switch_timestamp(NULL));
	rtp_session->send_msg.header.ssrc = htonl(rtp_session->ssrc);
	rtp_session->send_msg.header.ts = 0;
	rtp_session->send_msg.header.m = 0;
	rtp_session->send_msg.header.pt = (switch_payload_t) htonl(payload);
	rtp_session->send_msg.header.version = 2;
	rtp_session->send_msg.header.p = 0;
	rtp_session->send_msg.header.x = 0;
	rtp_session->send_msg.header.cc = 0;

	rtp_session->recv_msg.header.ssrc = 0;
	rtp_session->recv_msg.header.ts = 0;
	rtp_session->recv_msg.header.seq = 0;
	rtp_session->recv_msg.header.m = 0;
	rtp_session->recv_msg.header.pt = (switch_payload_t) htonl(payload);
	rtp_session->recv_msg.header.version = 2;
	rtp_session->recv_msg.header.p = 0;
	rtp_session->recv_msg.header.x = 0;
	rtp_session->recv_msg.header.cc = 0;

	rtp_session->payload = payload;
	rtp_session->ms_per_packet = ms_per_packet;
	rtp_session->samples_per_interval = rtp_session->conf_samples_per_interval = samples_per_interval;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) && switch_strlen_zero(timer_name)) {
		timer_name = "soft";
	}

	if (!switch_strlen_zero(timer_name) && !strcasecmp(timer_name, "none")) {
		timer_name = NULL;
	}

	if (!switch_strlen_zero(timer_name)) {
		rtp_session->timer_name = switch_core_strdup(pool, timer_name);
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);

		if (switch_core_timer_init(&rtp_session->timer, timer_name, ms_per_packet / 1000, samples_per_interval, pool) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
							  "Starting timer [%s] %d bytes per %dms\n", timer_name, samples_per_interval, ms_per_packet);
		} else {
			memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error starting timer [%s], async RTP disabled\n", timer_name);
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not using a timer\n");
		switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
	}

	rtp_session->ready = 1;
	*new_rtp_session = rtp_session;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_rtp_t *) switch_rtp_new(const char *rx_host,
											  switch_port_t rx_port,
											  const char *tx_host,
											  switch_port_t tx_port,
											  switch_payload_t payload,
											  uint32_t samples_per_interval,
											  uint32_t ms_per_packet,
											  switch_rtp_flag_t flags, char *timer_name, const char **err, switch_memory_pool_t *pool)
{
	switch_rtp_t *rtp_session = NULL;

	if (switch_strlen_zero(rx_host)) {
		*err = "Missing local host";
		goto end;
	}

	if (!rx_port) {
		*err = "Missing local port";
		goto end;
	}

	if (switch_strlen_zero(tx_host)) {
		*err = "Missing remote host";
		goto end;
	}

	if (!tx_port) {
		*err = "Missing remote port";
		goto end;
	}

	if (switch_rtp_create(&rtp_session, payload, samples_per_interval, ms_per_packet, flags, timer_name, err, pool) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}

	switch_mutex_lock(rtp_session->flag_mutex);

	if (switch_rtp_set_local_address(rtp_session, rx_host, rx_port, err) != SWITCH_STATUS_SUCCESS) {
		switch_mutex_unlock(rtp_session->flag_mutex);
		rtp_session = NULL;
		goto end;
	}

	if (switch_rtp_set_remote_address(rtp_session, tx_host, tx_port, SWITCH_TRUE, err) != SWITCH_STATUS_SUCCESS) {
		switch_mutex_unlock(rtp_session->flag_mutex);
		rtp_session = NULL;
		goto end;
	}

  end:

	if (rtp_session) {
		switch_mutex_unlock(rtp_session->flag_mutex);
		rtp_session->ready = 2;
		rtp_session->rx_host = switch_core_strdup(rtp_session->pool, rx_host);
		rtp_session->rx_port = rx_port;
	} else {
		switch_rtp_release_port(rx_host, rx_port);
	}

	return rtp_session;
}

SWITCH_DECLARE(void) switch_rtp_set_telephony_event(switch_rtp_t *rtp_session, switch_payload_t te)
{
	if (te > 95) {
		rtp_session->te = te;
	}
}

SWITCH_DECLARE(void) switch_rtp_set_cng_pt(switch_rtp_t *rtp_session, switch_payload_t pt)
{
	rtp_session->cng_pt = pt;
	switch_set_flag(rtp_session, SWITCH_RTP_FLAG_AUTO_CNG);
}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_stun_ping(switch_rtp_t *rtp_session, const char *stun_ip, switch_port_t stun_port,
															  uint32_t packet_count, switch_bool_t funny)
{
	
	if (switch_sockaddr_info_get(&rtp_session->remote_stun_addr, stun_ip, SWITCH_UNSPEC, 
								 stun_port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS || !rtp_session->remote_stun_addr) {
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error resolving stun ping addr\n");
		return SWITCH_STATUS_FALSE;
	}

	if (funny) {
		rtp_session->funny_stun++;
	}

	rtp_session->stun_port = stun_port;

	rtp_session->default_stuncount = packet_count;

	rtp_session->stun_ip = switch_core_strdup(rtp_session->pool, stun_ip);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_jitter_buffer(switch_rtp_t *rtp_session, uint32_t queue_frames)
{
	rtp_session->jb = stfu_n_init(queue_frames);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_ice(switch_rtp_t *rtp_session, char *login, char *rlogin)
{
	char ice_user[80];
	char user_ice[80];

	switch_snprintf(ice_user, sizeof(ice_user), "%s%s", login, rlogin);
	switch_snprintf(user_ice, sizeof(user_ice), "%s%s", rlogin, login);
	rtp_session->ice_user = switch_core_strdup(rtp_session->pool, ice_user);
	rtp_session->user_ice = switch_core_strdup(rtp_session->pool, user_ice);
	rtp_session->default_stuncount = 25;

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_rtp_break(switch_rtp_t *rtp_session)
{
	if (!switch_rtp_ready(rtp_session)) {
		return;
	}

	switch_mutex_lock(rtp_session->flag_mutex);
	switch_set_flag(rtp_session, SWITCH_RTP_FLAG_BREAK);
	
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK)) {
		switch_mutex_unlock(rtp_session->flag_mutex);
		return;
	}

	if (rtp_session->sock_input) {
		uint32_t o = UINT_MAX;
		switch_size_t len = sizeof(o);
		switch_socket_sendto(rtp_session->sock_input, rtp_session->local_addr, 0, (void *) &o, &len);
	}
	switch_mutex_unlock(rtp_session->flag_mutex);
}

SWITCH_DECLARE(void) switch_rtp_kill_socket(switch_rtp_t *rtp_session)
{
	switch_assert(rtp_session != NULL);
	switch_mutex_lock(rtp_session->flag_mutex);
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
		switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_IO);
		if (rtp_session->sock_input) {
			switch_socket_shutdown(rtp_session->sock_input, SWITCH_SHUTDOWN_READWRITE);
		}
		if (rtp_session->sock_output && rtp_session->sock_output != rtp_session->sock_input) {
			switch_socket_shutdown(rtp_session->sock_output, SWITCH_SHUTDOWN_READWRITE);
		}
	}
	switch_mutex_unlock(rtp_session->flag_mutex);
}

SWITCH_DECLARE(uint8_t) switch_rtp_ready(switch_rtp_t *rtp_session)
{
	uint8_t ret;

	if (!rtp_session || !rtp_session->flag_mutex || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SHUTDOWN)) {
		return 0;
	}

	switch_mutex_lock(rtp_session->flag_mutex);
	ret = (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) && rtp_session->sock_input && rtp_session->sock_output && rtp_session->remote_addr && rtp_session->ready == 2) ? 1 : 0;
	switch_mutex_unlock(rtp_session->flag_mutex);

	return ret;
}

SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp_t **rtp_session)
{
	void *pop;
	switch_socket_t *sock;

	if (!rtp_session || !*rtp_session || !(*rtp_session)->ready) {
		return;
	}

	switch_set_flag_locked((*rtp_session), SWITCH_RTP_FLAG_SHUTDOWN);

	READ_INC((*rtp_session));
	WRITE_INC((*rtp_session));

	(*rtp_session)->ready = 0;

	READ_DEC((*rtp_session));
	WRITE_DEC((*rtp_session));

	switch_mutex_lock((*rtp_session)->flag_mutex);

	switch_rtp_kill_socket(*rtp_session);

	while (switch_queue_trypop((*rtp_session)->dtmf_data.dtmf_inqueue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_safe_free(pop);
	}

	while (switch_queue_trypop((*rtp_session)->dtmf_data.dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_safe_free(pop);
	}

	if ((*rtp_session)->jb) {
		stfu_n_destroy(&(*rtp_session)->jb);
	}

	sock = (*rtp_session)->sock_input;
	(*rtp_session)->sock_input = NULL;
	switch_socket_close(sock);

	if ( (*rtp_session)->sock_output != sock ) {
		sock = (*rtp_session)->sock_output;
		(*rtp_session)->sock_output = NULL;
		switch_socket_close(sock);
	}

	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_VAD)) {
		switch_rtp_disable_vad(*rtp_session);
	}

	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_SECURE_SEND)) {
		srtp_dealloc((*rtp_session)->send_ctx);
		(*rtp_session)->send_ctx = NULL;
		switch_clear_flag((*rtp_session), SWITCH_RTP_FLAG_SECURE_SEND);
	}

	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_SECURE_RECV)) {
		srtp_dealloc((*rtp_session)->recv_ctx);
		(*rtp_session)->recv_ctx = NULL;
		switch_clear_flag((*rtp_session), SWITCH_RTP_FLAG_SECURE_RECV);
	}

	if ((*rtp_session)->timer.timer_interface) {
		switch_core_timer_destroy(&(*rtp_session)->timer);
	}

	switch_rtp_release_port((*rtp_session)->rx_host, (*rtp_session)->rx_port);
	switch_mutex_unlock((*rtp_session)->flag_mutex);

	return;
}

SWITCH_DECLARE(switch_socket_t *) switch_rtp_get_rtp_socket(switch_rtp_t *rtp_session)
{
	return rtp_session->sock_input;
}

SWITCH_DECLARE(void) switch_rtp_set_default_samples_per_interval(switch_rtp_t *rtp_session, uint32_t samples_per_interval)
{
	rtp_session->samples_per_interval = samples_per_interval;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_default_samples_per_interval(switch_rtp_t *rtp_session)
{
	return rtp_session->samples_per_interval;
}

SWITCH_DECLARE(void) switch_rtp_set_default_payload(switch_rtp_t *rtp_session, switch_payload_t payload)
{
	rtp_session->payload = payload;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_default_payload(switch_rtp_t *rtp_session)
{
	return rtp_session->payload;
}

SWITCH_DECLARE(void) switch_rtp_set_invald_handler(switch_rtp_t *rtp_session, switch_rtp_invalid_handler_t on_invalid)
{
	rtp_session->invalid_handler = on_invalid;
}

SWITCH_DECLARE(void) switch_rtp_set_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags)
{
	switch_set_flag_locked(rtp_session, flags);
	if (flags & SWITCH_RTP_FLAG_AUTOADJ) {
		rtp_session->autoadj_window = 20;
		rtp_session->autoadj_tally = 0;
	}
}

SWITCH_DECLARE(uint32_t) switch_rtp_test_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags)
{
	return (uint32_t) switch_test_flag(rtp_session, flags);
}

SWITCH_DECLARE(void) switch_rtp_clear_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags)
{
	switch_clear_flag_locked(rtp_session, flags);
}

static void do_2833(switch_rtp_t *rtp_session)
{
	switch_frame_flag_t flags = 0;
	uint32_t samples = rtp_session->samples_per_interval;

	if (rtp_session->dtmf_data.out_digit_dur > 0) {
		int x, loops = 1;
		rtp_session->dtmf_data.out_digit_sofar += samples;
		rtp_session->dtmf_data.out_digit_sub_sofar += samples;

		if (rtp_session->dtmf_data.out_digit_sub_sofar > 0xFFFF) {
			rtp_session->dtmf_data.out_digit_sub_sofar = samples;
			rtp_session->dtmf_data.timestamp_dtmf += 0xFFFF;
		}

		if (rtp_session->dtmf_data.out_digit_sofar >= rtp_session->dtmf_data.out_digit_dur) {
			rtp_session->dtmf_data.out_digit_packet[1] |= 0x80;
			loops = 3;
		}

		rtp_session->dtmf_data.out_digit_packet[2] = (unsigned char) (rtp_session->dtmf_data.out_digit_sub_sofar >> 8);
		rtp_session->dtmf_data.out_digit_packet[3] = (unsigned char) rtp_session->dtmf_data.out_digit_sub_sofar;

		for (x = 0; x < loops; x++) {
			switch_rtp_write_manual(rtp_session,
									rtp_session->dtmf_data.out_digit_packet, 4, 0, rtp_session->te, rtp_session->dtmf_data.timestamp_dtmf, &flags);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send %s packet for [%c] ts=%u dur=%d/%d/%d seq=%d\n",
							  loops == 1 ? "middle" : "end", rtp_session->dtmf_data.out_digit,
							  rtp_session->dtmf_data.timestamp_dtmf,
							  rtp_session->dtmf_data.out_digit_sofar,
							  rtp_session->dtmf_data.out_digit_sub_sofar, rtp_session->dtmf_data.out_digit_dur, rtp_session->seq);
		}

		if (loops != 1) {
			rtp_session->last_write_ts = rtp_session->dtmf_data.timestamp_dtmf + rtp_session->dtmf_data.out_digit_sub_sofar;
			rtp_session->sending_dtmf = 0;
			if (rtp_session->timer.interval) {
				rtp_session->last_write_samplecount = rtp_session->timer.samplecount;
				rtp_session->next_write_samplecount = rtp_session->timer.samplecount + samples * 5;
			}
			rtp_session->dtmf_data.out_digit_dur = 0;
		}
	}

	if (!rtp_session->dtmf_data.out_digit_dur && rtp_session->dtmf_data.dtmf_queue && switch_queue_size(rtp_session->dtmf_data.dtmf_queue)) {
		void *pop;

		if (rtp_session->timer.interval) {
			if (rtp_session->timer.samplecount < rtp_session->next_write_samplecount) {
				return;
			}
		}

		if (switch_queue_trypop(rtp_session->dtmf_data.dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_dtmf_t *rdigit = pop;
			int64_t offset;

			rtp_session->sending_dtmf = 1;

			memset(rtp_session->dtmf_data.out_digit_packet, 0, 4);
			rtp_session->dtmf_data.out_digit_sofar = samples;
			rtp_session->dtmf_data.out_digit_sub_sofar = samples;
			rtp_session->dtmf_data.out_digit_dur = rdigit->duration;
			rtp_session->dtmf_data.out_digit = rdigit->digit;
			rtp_session->dtmf_data.out_digit_packet[0] = (unsigned char) switch_char_to_rfc2833(rdigit->digit);
			rtp_session->dtmf_data.out_digit_packet[1] = 7;
			rtp_session->dtmf_data.out_digit_packet[2] = (unsigned char) (rtp_session->dtmf_data.out_digit_sub_sofar >> 8);
			rtp_session->dtmf_data.out_digit_packet[3] = (unsigned char) rtp_session->dtmf_data.out_digit_sub_sofar;


			rtp_session->dtmf_data.timestamp_dtmf = rtp_session->last_write_ts + samples;
			if (rtp_session->timer.interval) {
				offset = rtp_session->timer.samplecount - rtp_session->last_write_samplecount;
				if (offset > 0) {
					rtp_session->dtmf_data.timestamp_dtmf = (uint32_t) (rtp_session->dtmf_data.timestamp_dtmf + offset);
				}
			}

			switch_rtp_write_manual(rtp_session,
									rtp_session->dtmf_data.out_digit_packet,
									4,
									switch_test_flag(rtp_session, SWITCH_RTP_FLAG_BUGGY_2833) ? 0 : 1,
									rtp_session->te, rtp_session->dtmf_data.timestamp_dtmf, &flags);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send start packet for [%c] ts=%u dur=%d/%d/%d seq=%d\n",
							  rtp_session->dtmf_data.out_digit,
							  rtp_session->dtmf_data.timestamp_dtmf,
							  rtp_session->dtmf_data.out_digit_sofar,
							  rtp_session->dtmf_data.out_digit_sub_sofar, rtp_session->dtmf_data.out_digit_dur, rtp_session->seq);

			free(rdigit);
		}
	}
}

SWITCH_DECLARE(void) rtp_flush_read_buffer(switch_rtp_t *rtp_session)
{
	if (switch_rtp_ready(rtp_session) && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_FLUSH);
	}
}

static void do_flush(switch_rtp_t *rtp_session)
{
	int was_blocking = 0;
	switch_size_t bytes;
	switch_status_t status;

	if (!switch_rtp_ready(rtp_session) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		return;
	}
	
	READ_INC(rtp_session);

	if (switch_rtp_ready(rtp_session)) {
		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK)) {
			was_blocking = 1;
			switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);		
			switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, TRUE);
		}
	
		do {
			if (switch_rtp_ready(rtp_session)) {
				bytes = sizeof(rtp_msg_t);
				status = switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock_input, 0, (void *) &rtp_session->recv_msg, &bytes);
			} else {
				break;
			}
		} while(bytes > 0);
		
		if (was_blocking && switch_rtp_ready(rtp_session)) {
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
			switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, FALSE);
		}
	}
	
	READ_DEC(rtp_session);
}

#define return_cng_frame() do_cng = 1; goto timer_check

static int rtp_common_read(switch_rtp_t *rtp_session, switch_payload_t *payload_type, switch_frame_flag_t *flags, switch_io_flag_t io_flags)
{
	switch_size_t bytes = 0;
	switch_status_t status;
	int check = 0;
	stfu_frame_t *jb_frame;
	int ret = -1;
	int sleep_mss = 1000;

	if (!switch_rtp_ready(rtp_session)) {
		return -1;
	}

	if (rtp_session->timer.interval) {
		sleep_mss = rtp_session->timer.interval * 1000;
	} else {
		rtp_session->last_time = switch_time_now();
	}

	READ_INC(rtp_session);

	while (switch_rtp_ready(rtp_session)) {
		int do_cng = 0;

		if (rtp_session->timer.interval) {
			switch_core_timer_next(&rtp_session->timer);
		}

		bytes = sizeof(rtp_msg_t);
		status = switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock_input, 0, (void *) &rtp_session->recv_msg, &bytes);

		if (bytes < 0) {
			ret = (int) bytes;
			goto end;
		}
		
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH)) {
			do_flush(rtp_session);
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_FLUSH);
			bytes = 0;
		}

		if (rtp_session->max_missed_packets) {
			if (bytes) {
				rtp_session->missed_count = 0;
			} else if (++rtp_session->missed_count >= rtp_session->max_missed_packets) {
				ret = -2;
				goto end;
			}
		}

		check = !bytes;
		
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_BREAK) || (bytes && bytes == 4 && *((int *)&rtp_session->recv_msg) == UINT_MAX)) {
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_BREAK);
			do_2833(rtp_session);
			rtp_flush_read_buffer(rtp_session);
			bytes = 0;
			return_cng_frame();
		}

		if (bytes && bytes < 5) {
			continue;
		}

		if (bytes && rtp_session->recv_msg.header.m && rtp_session->recv_msg.header.pt != rtp_session->te) {
			rtp_flush_read_buffer(rtp_session);
		}

		if (bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ) && switch_sockaddr_get_port(rtp_session->from_addr)) {
			if (++rtp_session->autoadj_tally >= 10) {
				const char *tx_host;
				const char *old_host;
				char bufa[30], bufb[30];

				tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->from_addr);
				old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
				if ((switch_sockaddr_get_port(rtp_session->from_addr) != rtp_session->remote_port) || strcmp(tx_host, old_host)) {
					const char *err;
					uint32_t old = rtp_session->remote_port;
					
					if (!switch_strlen_zero(tx_host) && switch_sockaddr_get_port(rtp_session->from_addr) > 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
										  "Auto Changing port from %s:%u to %s:%u\n", old_host, old, tx_host,
										  switch_sockaddr_get_port(rtp_session->from_addr));
						switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(rtp_session->from_addr), SWITCH_FALSE, &err);
						switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}
				}
			}
		}
		
		if (bytes && rtp_session->autoadj_window) {
			if (--rtp_session->autoadj_window == 0) {
				switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}

		if (bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
			/* Fast PASS! */
			*flags |= SFF_PROXY_PACKET;
			ret = (int) bytes;
			goto end;
		}

		if (bytes) {
			rtp_session->missed_count = 0;

			if (rtp_session->recv_msg.header.pt && (rtp_session->recv_msg.header.pt == rtp_session->cng_pt || rtp_session->recv_msg.header.pt == 13)) {
				return_cng_frame();
			} 
		}

		if (!bytes && (io_flags & SWITCH_IO_FLAG_NOBLOCK)) {
			return_cng_frame();
		}

		
		if (check && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTO_CNG) &&
			rtp_session->timer.samplecount >= (rtp_session->last_write_samplecount + (rtp_session->samples_per_interval * 50))) {
			uint8_t data[10] = { 0 };
			switch_frame_flag_t frame_flags = SFF_NONE;
			data[0] = 65;
			rtp_session->cn++;
			rtp_common_write(rtp_session, NULL, (void *) data, 2, rtp_session->cng_pt, 0, &frame_flags);
		}

		if (check || bytes) {
			do_2833(rtp_session);
		}

		if (bytes && rtp_session->recv_msg.header.version != 2) {
			uint8_t *data = (uint8_t *) rtp_session->recv_msg.body;
			if (rtp_session->recv_msg.header.version == 0) {
				if (rtp_session->ice_user) {
					handle_ice(rtp_session, (void *) &rtp_session->recv_msg, bytes);
				} else if (rtp_session->remote_stun_addr) {
					handle_stun_ping_reply(rtp_session, (void *) &rtp_session->recv_msg, bytes);
				}
			}

			if (rtp_session->invalid_handler) {
				rtp_session->invalid_handler(rtp_session, rtp_session->sock_input, (void *) &rtp_session->recv_msg, bytes, rtp_session->from_addr);
			}

			memset(data, 0, 2);
			data[0] = 65;

			rtp_session->recv_msg.header.pt = (uint32_t) rtp_session->cng_pt ? rtp_session->cng_pt : SWITCH_RTP_CNG_PAYLOAD;
			*flags |= SFF_CNG;
			*payload_type = (switch_payload_t) rtp_session->recv_msg.header.pt;
			ret = 2 + rtp_header_len;
			goto end;
		}

		if (rtp_session->jb && ((bytes && rtp_session->recv_msg.header.pt == rtp_session->payload) || check)) {
			if (bytes) {
				if (rtp_session->recv_msg.header.m) {
					stfu_n_reset(rtp_session->jb);
				}

				stfu_n_eat(rtp_session->jb, ntohl(rtp_session->recv_msg.header.ts), rtp_session->recv_msg.body, bytes - rtp_header_len);
				bytes = 0;
			}

			if ((jb_frame = stfu_n_read_a_frame(rtp_session->jb))) {
				memcpy(rtp_session->recv_msg.body, jb_frame->data, jb_frame->dlen);
				if (jb_frame->plc) {
					*flags |= SFF_PLC;
				}
				bytes = jb_frame->dlen + rtp_header_len;
				rtp_session->recv_msg.header.ts = htonl(jb_frame->ts);
				rtp_session->recv_msg.header.pt = rtp_session->payload;
			} else {
				goto timer_check;
			}
		}


		if (bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV)) {
			int sbytes = (int) bytes;
			err_status_t stat = 0;

			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV_RESET)) {
				switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV_RESET);
				srtp_dealloc(rtp_session->recv_ctx);
				rtp_session->recv_ctx = NULL;
				if ((stat = srtp_create(&rtp_session->recv_ctx, &rtp_session->recv_policy))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP RECV\n");
					ret = -1;
					goto end;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "RE-Activating Secure RTP RECV\n");
					rtp_session->srtp_errs = 0;
				}
			}

			stat = srtp_unprotect(rtp_session->recv_ctx, &rtp_session->recv_msg.header, &sbytes);

			if (stat && rtp_session->recv_msg.header.pt != rtp_session->te && rtp_session->recv_msg.header.pt != rtp_session->cng_pt) {
				if (++rtp_session->srtp_errs >= MAX_SRTP_ERRS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Error: SRTP unprotect failed with code %d%s\n", stat,
									  stat == err_status_replay_fail ? " (replay check failed)" : stat ==
									  err_status_auth_fail ? " (auth check failed)" : "");
					ret = -1;
					goto end;
				} else {
					sbytes = 0;
				}
			} else {
				rtp_session->srtp_errs = 0;
			}

			bytes = sbytes;
		}



		/* RFC2833 ... like all RFC RE: VoIP, guaranteed to drive you to insanity! 
		   We know the real rules here, but if we enforce them, it's an interop nightmare so,
		   we put up with as much as we can so we don't have to deal with being punished for
		   doing it right. Nice guys finish last!
		 */
		if (bytes && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PASS_RFC2833) && rtp_session->recv_msg.header.pt == rtp_session->te) {
			unsigned char *packet = (unsigned char *) rtp_session->recv_msg.body;
			int end = packet[1] & 0x80 ? 1 : 0;
			uint16_t duration = (packet[2] << 8) + packet[3];
			char key = switch_rfc2833_to_char(packet[0]);
			uint16_t in_digit_seq = ntohs((uint16_t) rtp_session->recv_msg.header.seq);

			if (in_digit_seq > rtp_session->dtmf_data.in_digit_seq) {
				uint32_t ts = htonl(rtp_session->recv_msg.header.ts);
				//int m = rtp_session->recv_msg.header.m;

				rtp_session->dtmf_data.in_digit_seq = in_digit_seq;

				//printf("%c %u %u %u\n", key, in_digit_seq, ts, duration);

				if (rtp_session->dtmf_data.last_duration > duration && ts == rtp_session->dtmf_data.in_digit_ts) {
					rtp_session->dtmf_data.flip++;
				}

				if (end) {
					if (rtp_session->dtmf_data.in_digit_ts) {
						switch_dtmf_t dtmf = { key, duration };

						if (ts > rtp_session->dtmf_data.in_digit_ts) {
							dtmf.duration += (ts - rtp_session->dtmf_data.in_digit_ts);
						}
						if (rtp_session->dtmf_data.flip) {
							dtmf.duration += rtp_session->dtmf_data.flip * 0xFFFF;
							rtp_session->dtmf_data.flip = 0;
							//printf("you're welcome!\n");
						}
						//printf("done digit=%c ts=%u start_ts=%u dur=%u ddur=%u\n", 
						//dtmf.digit, ts, rtp_session->dtmf_data.in_digit_ts, duration, dtmf.duration);
						switch_rtp_queue_rfc2833_in(rtp_session, &dtmf);
						switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_BREAK);
						rtp_session->dtmf_data.last_digit = rtp_session->dtmf_data.first_digit;
					}
					rtp_session->dtmf_data.in_digit_ts = 0;
				} else if (!rtp_session->dtmf_data.in_digit_ts) {
					rtp_session->dtmf_data.in_digit_ts = ts;
					rtp_session->dtmf_data.first_digit = key;
				}

				rtp_session->dtmf_data.last_duration = duration;

			}

			do_cng = 1;
		}

	  timer_check:

		if (do_cng) {
			uint8_t *data = (uint8_t *) rtp_session->recv_msg.body;
			memset(data, 0, 2);
			data[0] = 65;
			rtp_session->recv_msg.header.pt = (uint32_t) rtp_session->cng_pt ? rtp_session->cng_pt : SWITCH_RTP_CNG_PAYLOAD;
			*flags |= SFF_CNG;
			*payload_type = (switch_payload_t) rtp_session->recv_msg.header.pt;
			ret = 2 + rtp_header_len;
			goto end;
		}

		if (check || (bytes && !rtp_session->timer.interval)) {
			if (rtp_session->jb && (jb_frame = stfu_n_read_a_frame(rtp_session->jb))) {
				memcpy(rtp_session->recv_msg.body, jb_frame->data, jb_frame->dlen);
				if (jb_frame->plc) {
					*flags |= SFF_PLC;
				}
				bytes = jb_frame->dlen + rtp_header_len;
				rtp_session->recv_msg.header.ts = htonl(jb_frame->ts);
			} else if (!bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {	/* We're late! We're Late! */
				if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK) && status == SWITCH_STATUS_BREAK) {
					switch_cond_next();
					continue;
				}
				
				return_cng_frame();
			}
		}

		if (status == SWITCH_STATUS_BREAK || bytes == 0) {
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_DATAWAIT)) {
				goto do_continue;
			}
			ret = 0;
			goto end;
		}

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_GOOGLEHACK) && rtp_session->recv_msg.header.pt == 102) {
			rtp_session->recv_msg.header.pt = 97;
		}

		rtp_session->rpayload = (switch_payload_t) rtp_session->recv_msg.header.pt;

		break;

	  do_continue:
		
		if (!bytes && !rtp_session->timer.interval) {
			switch_yield(sleep_mss);
		}

	}

	if (switch_rtp_ready(rtp_session)) {
		*payload_type = (switch_payload_t) rtp_session->recv_msg.header.pt;

		if (*payload_type == SWITCH_RTP_CNG_PAYLOAD) {
			*flags |= SFF_CNG;
		}

		ret = (int) bytes;
	} else {
		ret = -1;
	}

  end:

	READ_DEC(rtp_session);

	return ret;
}

SWITCH_DECLARE(switch_size_t) switch_rtp_has_dtmf(switch_rtp_t *rtp_session)
{
	switch_size_t has = 0;

	if (switch_rtp_ready(rtp_session)) {
		switch_mutex_lock(rtp_session->dtmf_data.dtmf_mutex);
		has = switch_queue_size(rtp_session->dtmf_data.dtmf_inqueue);
		switch_mutex_unlock(rtp_session->dtmf_data.dtmf_mutex);
	}

	return has;
}

SWITCH_DECLARE(switch_size_t) switch_rtp_dequeue_dtmf(switch_rtp_t *rtp_session, switch_dtmf_t *dtmf)
{
	switch_size_t bytes = 0;
	switch_dtmf_t *_dtmf = NULL;
	void *pop;

	if (!switch_rtp_ready(rtp_session)) {
		return bytes;
	}

	switch_mutex_lock(rtp_session->dtmf_data.dtmf_mutex);
	if (switch_queue_trypop(rtp_session->dtmf_data.dtmf_inqueue, &pop) == SWITCH_STATUS_SUCCESS) {
		_dtmf = (switch_dtmf_t *) pop;
		*dtmf = *_dtmf;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "RTP RECV DTMF %c:%d\n", dtmf->digit, dtmf->duration);
		bytes++;
	}
	switch_mutex_unlock(rtp_session->dtmf_data.dtmf_mutex);

	if (bytes) {
		rtp_flush_read_buffer(rtp_session);
	}

	return bytes;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_queue_rfc2833(switch_rtp_t *rtp_session, const switch_dtmf_t *dtmf)
{

	switch_dtmf_t *rdigit;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((rdigit = malloc(sizeof(*rdigit))) != 0) {
		*rdigit = *dtmf;
		if (rdigit->duration < switch_core_default_dtmf_duration(0)) {
			rdigit->duration = switch_core_default_dtmf_duration(0);
		}

		if ((switch_queue_trypush(rtp_session->dtmf_data.dtmf_queue, rdigit)) != SWITCH_STATUS_SUCCESS) {
			free(rdigit);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		abort();
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_queue_rfc2833_in(switch_rtp_t *rtp_session, const switch_dtmf_t *dtmf)
{
	switch_dtmf_t *rdigit;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((rdigit = malloc(sizeof(*rdigit))) != 0) {
		*rdigit = *dtmf;
		if (rdigit->duration < switch_core_default_dtmf_duration(0)) {
			rdigit->duration = switch_core_default_dtmf_duration(0);
		}

		if ((switch_queue_trypush(rtp_session->dtmf_data.dtmf_inqueue, rdigit)) != SWITCH_STATUS_SUCCESS) {
			free(rdigit);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		abort();
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_read(switch_rtp_t *rtp_session, void *data, uint32_t *datalen,
												switch_payload_t *payload_type, switch_frame_flag_t *flags, switch_io_flag_t io_flags)
{
	int bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = rtp_common_read(rtp_session, payload_type, flags, io_flags);

	if (bytes < 0) {
		*datalen = 0;
		return bytes == -2 ? SWITCH_STATUS_TIMEOUT : SWITCH_STATUS_GENERR;
	} else if (bytes == 0) {
		*datalen = 0;
		return SWITCH_STATUS_BREAK;
	} else {
		bytes -= rtp_header_len;
	}

	*datalen = bytes;

	memcpy(data, rtp_session->recv_msg.body, bytes);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_zerocopy_read_frame(switch_rtp_t *rtp_session, switch_frame_t *frame, switch_io_flag_t io_flags)
{
	int bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = rtp_common_read(rtp_session, &frame->payload, &frame->flags, io_flags);

	frame->data = rtp_session->recv_msg.body;
	frame->packet = &rtp_session->recv_msg;
	frame->packetlen = bytes;
	frame->source = __FILE__;
	switch_set_flag(frame, SFF_RAW_RTP);
	if (frame->payload == rtp_session->te) {
		switch_set_flag(frame, SFF_RFC2833);
	}
	frame->timestamp = ntohl(rtp_session->recv_msg.header.ts);
	frame->seq = (uint16_t) ntohs((u_short) rtp_session->recv_msg.header.seq);
	frame->ssrc = ntohl(rtp_session->recv_msg.header.ssrc);
	frame->m = rtp_session->recv_msg.header.m ? SWITCH_TRUE : SWITCH_FALSE;

	if (bytes < 0) {
		frame->datalen = 0;
		return bytes == -2 ? SWITCH_STATUS_TIMEOUT : SWITCH_STATUS_GENERR;
	} else if (bytes == 0) {
		frame->datalen = 0;
		return SWITCH_STATUS_BREAK;
	} else {
		bytes -= rtp_header_len;
	}

	frame->datalen = bytes;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_zerocopy_read(switch_rtp_t *rtp_session,
														 void **data, uint32_t *datalen, switch_payload_t *payload_type, switch_frame_flag_t *flags,
														 switch_io_flag_t io_flags)
{
	int bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = rtp_common_read(rtp_session, payload_type, flags, io_flags);
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

static int rtp_common_write(switch_rtp_t *rtp_session,
							rtp_msg_t *send_msg, void *data, uint32_t datalen, switch_payload_t payload, uint32_t timestamp, switch_frame_flag_t *flags)
{
	switch_size_t bytes;
	uint8_t send = 1;
	uint32_t this_ts = 0;
	int ret;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	WRITE_INC(rtp_session);

	if (send_msg) {
		bytes = datalen;
		if (flags && *flags & SFF_RFC2833) {
			send_msg->header.pt = rtp_session->te;
		}
		data = send_msg->body;
		datalen -= rtp_header_len;
	} else {
		uint8_t m = 0;

		if (*flags & SFF_RFC2833) {
			payload = rtp_session->te;
		}

		send_msg = &rtp_session->send_msg;
		send_msg->header.pt = payload;

		if (timestamp) {
			rtp_session->ts = (uint32_t) timestamp;
		} else if (rtp_session->timer.timer_interface) {
			rtp_session->ts = rtp_session->timer.samplecount;
			if (rtp_session->ts <= rtp_session->last_write_ts) {
				rtp_session->ts = rtp_session->last_write_ts + rtp_session->samples_per_interval;
			}
		} else {
			rtp_session->ts += rtp_session->samples_per_interval;
		}

		rtp_session->send_msg.header.ts = htonl(rtp_session->ts);

		if ((rtp_session->ts > (rtp_session->last_write_ts + (rtp_session->samples_per_interval * 10)))
			|| rtp_session->ts == rtp_session->samples_per_interval) {
			m++;
		}

		if (rtp_session->cn && payload != rtp_session->cng_pt) {
			rtp_session->cn = 0;
			m++;
		}
		send_msg->header.m = m ? 1 : 0;

		memcpy(send_msg->body, data, datalen);
		bytes = datalen + rtp_header_len;
	}

	send_msg->header.ssrc = htonl(rtp_session->ssrc);

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_GOOGLEHACK) && rtp_session->send_msg.header.pt == 97) {
		rtp_session->recv_msg.header.pt = 102;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VAD) &&
		rtp_session->recv_msg.header.pt == rtp_session->vad_data.read_codec->implementation->ianacode) {

		int16_t decoded[SWITCH_RECOMMENDED_BUFFER_SIZE / sizeof(int16_t)] = { 0 };
		uint32_t rate = 0;
		uint32_t codec_flags = 0;
		uint32_t len = sizeof(decoded);
		time_t now = switch_timestamp(NULL);
		send = 0;

		if (rtp_session->vad_data.scan_freq && rtp_session->vad_data.next_scan <= now) {
			rtp_session->vad_data.bg_count = rtp_session->vad_data.bg_level = 0;
			rtp_session->vad_data.next_scan = now + rtp_session->vad_data.scan_freq;
		}

		if (switch_core_codec_decode(&rtp_session->vad_data.vad_codec,
									 rtp_session->vad_data.read_codec,
									 data,
									 datalen,
									 rtp_session->vad_data.read_codec->implementation->actual_samples_per_second,
									 decoded, &len, &rate, &codec_flags) == SWITCH_STATUS_SUCCESS) {

			uint32_t energy = 0;
			uint32_t x, y = 0, z = len / sizeof(int16_t);
			uint32_t score = 0;
			int divisor = 0;
			if (z) {

				if (!(divisor = rtp_session->vad_data.read_codec->implementation->actual_samples_per_second / 8000)) {
					divisor = 1;
				}

				for (x = 0; x < z; x++) {
					energy += abs(decoded[y]);
					y += rtp_session->vad_data.read_codec->implementation->number_of_channels;
				}

				if (++rtp_session->vad_data.start_count < rtp_session->vad_data.start) {
					send = 1;
				} else {
					score = (energy / (z / divisor));
					if (score && (rtp_session->vad_data.bg_count < rtp_session->vad_data.bg_len)) {
						rtp_session->vad_data.bg_level += score;
						if (++rtp_session->vad_data.bg_count == rtp_session->vad_data.bg_len) {
							rtp_session->vad_data.bg_level /= rtp_session->vad_data.bg_len;
						}
						send = 1;
					} else {
						if (score > rtp_session->vad_data.bg_level && !switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING)) {
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
										switch_channel_event_set_data(switch_core_session_get_channel(rtp_session->vad_data.session), event);
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
											switch_channel_event_set_data(switch_core_session_get_channel(rtp_session->vad_data.session), event);
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
				}
			}
		} else {
			ret = -1;
			goto end;
		}
	}

	this_ts = ntohl(send_msg->header.ts);

	if (!switch_rtp_ready(rtp_session) || rtp_session->sending_dtmf || !this_ts) {
		send = 0;
	}

	if (send) {
		send_msg->header.seq = htons(++rtp_session->seq);

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
			int sbytes = (int) bytes;
			err_status_t stat;


			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET)) {
				switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET);
				srtp_dealloc(rtp_session->send_ctx);
				rtp_session->send_ctx = NULL;
				if ((stat = srtp_create(&rtp_session->send_ctx, &rtp_session->send_policy))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP SEND\n");
					ret = -1;
					goto end;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "RE-Activating Secure RTP SEND\n");
				}
			}


			stat = srtp_protect(rtp_session->send_ctx, &send_msg->header, &sbytes);
			if (stat) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: SRTP protection failed with code %d\n", stat);
			}

			bytes = sbytes;
		}

		if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, (void *) send_msg, &bytes) != SWITCH_STATUS_SUCCESS) {
			rtp_session->seq--;
			ret = -1;
			goto end;
		}

		if (rtp_session->timer.interval) {
			rtp_session->last_write_samplecount = rtp_session->timer.samplecount;
		}
		rtp_session->last_write_ts = this_ts;
	}

	if (rtp_session->remote_stun_addr) {
		do_stun_ping(rtp_session);
	}

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			ret = -1;
			goto end;
		}
	}

	ret = (int) bytes;

  end:

	WRITE_DEC(rtp_session);

	return ret;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_disable_vad(switch_rtp_t *rtp_session)
{

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VAD)) {
		return SWITCH_STATUS_GENERR;
	}
	switch_core_codec_destroy(&rtp_session->vad_data.vad_codec);
	switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_VAD);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_enable_vad(switch_rtp_t *rtp_session, switch_core_session_t *session, switch_codec_t *codec,
													  switch_vad_flag_t flags)
{
	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VAD)) {
		return SWITCH_STATUS_GENERR;
	}
	memset(&rtp_session->vad_data, 0, sizeof(rtp_session->vad_data));

	if (switch_core_codec_init(&rtp_session->vad_data.vad_codec,
							   codec->implementation->iananame,
							   NULL,
							   codec->implementation->samples_per_second,
							   codec->implementation->microseconds_per_packet / 1000,
							   codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Activate VAD codec %s %dms\n", codec->implementation->iananame,
					  codec->implementation->microseconds_per_packet / 1000);
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
	rtp_session->vad_data.next_scan = switch_timestamp(NULL);
	rtp_session->vad_data.scan_freq = 0;
	switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_VAD);
	switch_set_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_CNG);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_rtp_write_frame(switch_rtp_t *rtp_session, switch_frame_t *frame)
{
	uint8_t fwd = 0;
	void *data = NULL;
	uint32_t len, ts = 0;
	switch_payload_t payload;
	rtp_msg_t *send_msg = NULL;

	if (!switch_rtp_ready(rtp_session) || !rtp_session->remote_addr) {
		return -1;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		switch_size_t bytes;

		/* Fast PASS! */
		if (!switch_test_flag(frame, SFF_PROXY_PACKET)) {
			return 0;
		}
		bytes = frame->packetlen;
		if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, frame->packet, &bytes) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
		return (int) bytes;
	}


	fwd = (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) && switch_test_flag(frame, SFF_RAW_RTP)) ? 1 : 0;

	switch_assert(frame != NULL);

	if (switch_test_flag(frame, SFF_CNG)) {
		payload = rtp_session->cng_pt;
	} else {
		payload = rtp_session->payload;
	}

	if (switch_test_flag(frame, SFF_RTP_HEADER)) {
		return switch_rtp_write_manual(rtp_session, frame->data, frame->datalen, frame->m, frame->payload, (uint32_t) (frame->timestamp), &frame->flags);
	}

	if (fwd) {
		send_msg = frame->packet;
		len = frame->packetlen;
		ts = 0;
		if (frame->codec && frame->codec->agreed_pt == frame->payload) {
			send_msg->header.pt = payload;
		}
	} else {
		data = frame->data;
		len = frame->datalen;
		ts = (uint32_t) frame->timestamp;
	}

	return rtp_common_write(rtp_session, send_msg, data, len, payload, ts, &frame->flags);
}

SWITCH_DECLARE(int) switch_rtp_write_manual(switch_rtp_t *rtp_session,
											void *data, uint32_t datalen, uint8_t m, switch_payload_t payload, uint32_t ts, switch_frame_flag_t *flags)
{
	switch_size_t bytes;
	int ret = -1;

	if (!switch_rtp_ready(rtp_session) || !rtp_session->remote_addr || datalen > SWITCH_RTP_MAX_BUF_LEN) {
		return -1;
	}

	WRITE_INC(rtp_session);

	rtp_session->write_msg = rtp_session->send_msg;
	rtp_session->write_msg.header.seq = htons(++rtp_session->seq);
	rtp_session->write_msg.header.ts = htonl(ts);
	rtp_session->write_msg.header.pt = payload;
	rtp_session->write_msg.header.m = m ? 1 : 0;
	memcpy(rtp_session->write_msg.body, data, datalen);

	bytes = rtp_header_len + datalen;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
		int sbytes = (int) bytes;
		err_status_t stat;

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET)) {
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET);
			srtp_dealloc(rtp_session->send_ctx);
			rtp_session->send_ctx = NULL;
			if ((stat = srtp_create(&rtp_session->send_ctx, &rtp_session->send_policy))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP SEND\n");
				ret = -1;
				goto end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "RE-Activating Secure RTP SEND\n");
			}
		}

		stat = srtp_protect(rtp_session->send_ctx, &rtp_session->write_msg.header, &sbytes);
		if (stat) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: SRTP protection failed with code %d\n", stat);
		}
		bytes = sbytes;
	}

	if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, (void *) &rtp_session->write_msg, &bytes) != SWITCH_STATUS_SUCCESS) {
		rtp_session->seq--;
		ret = -1;
		goto end;
	}

	rtp_session->last_write_ts = ts;

	ret = (int) bytes;

  end:

	WRITE_DEC(rtp_session);

	return ret;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_ssrc(switch_rtp_t *rtp_session)
{
	return rtp_session->send_msg.header.ssrc;
}

SWITCH_DECLARE(void) switch_rtp_set_private(switch_rtp_t *rtp_session, void *private_data)
{
	rtp_session->private_data = private_data;
}

SWITCH_DECLARE(void *) switch_rtp_get_private(switch_rtp_t *rtp_session)
{
	return rtp_session->private_data;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
