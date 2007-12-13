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
#include "stfu.h"
#define MAX_KEY_LEN      64
#define rtp_header_len 12
#define RTP_START_PORT 16384
#define RTP_END_PORT 32768
#define MAX_KEY_LEN      64
#define MASTER_KEY_LEN   30
#define RTP_MAGIC_NUMBER 42

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


struct rfc2833_digit {
	char digit;
	int duration;
};

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
	unsigned int out_digit_dur;
	uint16_t in_digit_seq;
	uint32_t out_digit_ssrc;
	int32_t timestamp_dtmf;
	char last_digit;
	unsigned int dc;
	time_t last_digit_time;
	switch_buffer_t *dtmf_buffer;
	switch_mutex_t *dtmf_mutex;
};

struct switch_rtp {
	switch_socket_t *sock;

	switch_sockaddr_t *local_addr;
	rtp_msg_t send_msg;
	srtp_ctx_t *send_ctx;

	switch_sockaddr_t *remote_addr;
	rtp_msg_t recv_msg;
	srtp_ctx_t *recv_ctx;
	uint32_t autoadj_window;
	uint32_t autoadj_tally;

	uint16_t seq;
	uint16_t rseq;
	uint8_t sending_dtmf;
	switch_payload_t payload;
	switch_payload_t rpayload;
	switch_rtp_invalid_handler_t invalid_handler;
	void *private_data;
	uint32_t ts;
	uint32_t last_write_ts;
	uint32_t last_write_samplecount;
	uint16_t last_write_seq;
	uint32_t last_write_ssrc;
	uint32_t flags;
	switch_memory_pool_t *pool;
	switch_sockaddr_t *from_addr;
	char *rx_host;
	switch_port_t rx_port;
	char *ice_user;
	char *user_ice;
	char *timer_name;
	switch_time_t last_stun;
	uint32_t samples_per_interval;
	uint32_t conf_samples_per_interval;
	uint32_t rsamples_per_interval;
	uint32_t ms_per_packet;
	uint32_t remote_port;
	uint8_t stuncount;
	struct switch_rtp_vad_data vad_data;
	struct switch_rtp_rfc2833_data dtmf_data;
	switch_payload_t te;
	switch_payload_t cng_pt;
	switch_mutex_t *flag_mutex;
	switch_timer_t timer;
	uint8_t ready;
	uint8_t cn;
	switch_time_t last_time;
	stfu_instance_t *jb;
	uint32_t max_missed_packets;
	uint32_t missed_count;
};

static int global_init = 0;
static int rtp_common_write(switch_rtp_t *rtp_session, void *data, uint32_t datalen, switch_payload_t payload, switch_frame_flag_t *flags);

static switch_status_t ice_out(switch_rtp_t *rtp_session)
{
	uint8_t buf[256] = { 0 };
	switch_stun_packet_t *packet;
	unsigned int elapsed;
	switch_size_t bytes;

	switch_assert(rtp_session != NULL);
	switch_assert(rtp_session->ice_user != NULL);

	if (rtp_session->stuncount != 0) {
		rtp_session->stuncount--;
		return SWITCH_STATUS_SUCCESS;
	}

	if (rtp_session->last_stun) {
		elapsed = (unsigned int) ((switch_time_now() - rtp_session->last_stun) / 1000);

		if (elapsed > 30000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No stun for a long time (PUNT!)\n");
			return SWITCH_STATUS_FALSE;
		}
	}

	packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
	switch_stun_packet_attribute_add_username(packet, rtp_session->ice_user, 32);
	bytes = switch_stun_packet_length(packet);
	switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void *) packet, &bytes);
	rtp_session->stuncount = 25;

	return SWITCH_STATUS_SUCCESS;
}

static void handle_ice(switch_rtp_t *rtp_session, void *data, switch_size_t len)
{
	switch_stun_packet_t *packet;
	switch_stun_packet_attribute_t *attr;
	char username[33] = { 0 };
	unsigned char buf[512] = { 0 };

	memcpy(buf, data, len);
	packet = switch_stun_packet_parse(buf, sizeof(buf));
	rtp_session->last_stun = switch_time_now();

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
	} while (switch_stun_packet_next_attribute(attr));

	if ((packet->header.type == SWITCH_STUN_BINDING_REQUEST) && !strcmp(rtp_session->user_ice, username)) {
		uint8_t buf[512];
		switch_stun_packet_t *rpacket;
		const char *remote_ip;
		switch_size_t bytes;
		char ipbuf[25];

		memset(buf, 0, sizeof(buf));
		rpacket = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, packet->header.id, buf);
		switch_stun_packet_attribute_add_username(rpacket, username, 32);
		remote_ip = switch_get_addr(ipbuf, sizeof(ipbuf), rtp_session->from_addr);
		switch_stun_packet_attribute_add_binded_address(rpacket, (char *)remote_ip, switch_sockaddr_get_port(rtp_session->from_addr));
		bytes = switch_stun_packet_length(rpacket);
		switch_socket_sendto(rtp_session->sock, rtp_session->from_addr, 0, (void *) rpacket, &bytes);
	}
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

static void release_port(const char *host, switch_port_t port)
{
	switch_core_port_allocator_t *alloc = NULL;

	if (!host) {
		return;
	}

    switch_mutex_lock(port_lock);
    if ((alloc = switch_core_hash_find(alloc_hash, host))) {
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
	char o[5] = "TEST", i[5] = "";
	switch_size_t len, ilen = 0;
	int x;

	*err = NULL;

	if (switch_sockaddr_info_get(&rtp_session->local_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Local Address Error!";
		goto done;
	}

	if (rtp_session->sock) {
		switch_rtp_kill_socket(rtp_session);
	}

	if (switch_socket_create(&new_sock, AF_INET, SOCK_DGRAM, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
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

	len = sizeof(i);
	switch_socket_opt_set(new_sock, SWITCH_SO_NONBLOCK, TRUE);

	switch_socket_sendto(new_sock, rtp_session->local_addr, 0, (void *) o, &len);

	x = 0;
	while(!ilen) {
		switch_status_t status;
		ilen = len;
		status = switch_socket_recvfrom(rtp_session->from_addr, new_sock, 0, (void *) i, &ilen);

		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			break;
		}

		if (++x > 500) {
			break;
		}
		switch_yield(1000);
	}
	switch_socket_opt_set(new_sock, SWITCH_SO_NONBLOCK, FALSE);

	if (!ilen) {
		*err = "Send myself a packet failed!";
		goto done;
	}
	
	old_sock = rtp_session->sock;
	rtp_session->sock = new_sock;
	new_sock = NULL;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK)) {
		switch_socket_opt_set(rtp_session->sock, SWITCH_SO_NONBLOCK, TRUE);
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

	return status;
}

SWITCH_DECLARE(void) switch_rtp_set_max_missed_packets(switch_rtp_t *rtp_session, uint32_t max)
{
	rtp_session->max_missed_packets = max;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_set_remote_address(switch_rtp_t *rtp_session, const char *host, switch_port_t port, const char **err)
{
	*err = "Success";

	if (switch_sockaddr_info_get(&rtp_session->remote_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) 
		!= SWITCH_STATUS_SUCCESS || !rtp_session->remote_addr) {
		*err = "Remote Address Error!";
		return SWITCH_STATUS_FALSE;
	}

	rtp_session->remote_port = port;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_create(switch_rtp_t **new_rtp_session,
												  switch_payload_t payload,
												  uint32_t samples_per_interval,
												  uint32_t ms_per_packet,
												  switch_rtp_flag_t flags, char *crypto_key, char *timer_name, const char **err,
												  switch_memory_pool_t *pool)
{
	switch_rtp_t *rtp_session = NULL;
	srtp_policy_t policy;
	char key[MAX_KEY_LEN];
	uint32_t ssrc = rand() & 0xffff;

	*new_rtp_session = NULL;

	if (samples_per_interval > SWITCH_RTP_MAX_BUF_LEN) {
		*err = "Packet Size Too Large!";
		return SWITCH_STATUS_FALSE;
	}

	if (!(rtp_session = switch_core_alloc(pool, sizeof(*rtp_session)))) {
		*err = "Memory Error!";
		return SWITCH_STATUS_MEMERR;
	}

	rtp_session->dtmf_data.out_digit_ssrc = ssrc;
	rtp_session->pool = pool;
	rtp_session->te = 101;

	switch_mutex_init(&rtp_session->flag_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->dtmf_data.dtmf_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_buffer_create_dynamic(&rtp_session->dtmf_data.dtmf_buffer, 128, 128, 0);
	switch_rtp_set_flag(rtp_session, flags);

	/* for from address on recvfrom calls */
	switch_sockaddr_info_get(&rtp_session->from_addr, NULL, SWITCH_UNSPEC, 0, 0, pool);

	memset(&policy, 0, sizeof(policy));
	if (crypto_key) {
		int len;

		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_SECURE);
		crypto_policy_set_rtp_default(&policy.rtp);
		crypto_policy_set_rtcp_default(&policy.rtcp);
		policy.ssrc.type = ssrc_any_inbound;
		policy.ssrc.value = ssrc;
		policy.key = (uint8_t *) key;
		policy.next = NULL;
		policy.rtp.sec_serv = sec_serv_conf_and_auth;
		policy.rtcp.sec_serv = sec_serv_none;

		/* read key from hexadecimal on command line into an octet string */
		len = hex_string_to_octet_string(key, crypto_key, MASTER_KEY_LEN * 2);

		/* check that hex string is the right length */
		if (len < MASTER_KEY_LEN * 2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "error: too few digits in key/salt " "(should be %d hexadecimal digits, found %d)\n", MASTER_KEY_LEN * 2, len);
			*err = "Crypt Error";
			return SWITCH_STATUS_FALSE;
		}
		if (strlen(crypto_key) > MASTER_KEY_LEN * 2) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "error: too many digits in key/salt "
							  "(should be %d hexadecimal digits, found %u)\n", MASTER_KEY_LEN * 2, (unsigned) strlen(crypto_key));
			*err = "Crypt Error";
			return SWITCH_STATUS_FALSE;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Activating Secure RTP!\n");
	}

	rtp_session->seq = (uint16_t) rand();
	rtp_session->send_msg.header.ssrc = htonl(ssrc);
	rtp_session->send_msg.header.ts = 0;
	rtp_session->send_msg.header.m = 0;
	rtp_session->send_msg.header.pt = (switch_payload_t) htonl(payload);
	rtp_session->send_msg.header.version = 2;
	rtp_session->send_msg.header.p = 0;
	rtp_session->send_msg.header.x = 0;
	rtp_session->send_msg.header.cc = 0;

	rtp_session->recv_msg.header.ssrc = htonl(ssrc);
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
	rtp_session->timer_name = switch_core_strdup(pool, timer_name);

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

	if (!switch_strlen_zero(timer_name)) {
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) && switch_strlen_zero(timer_name)) {
		timer_name = "soft";
	}

	if (!switch_strlen_zero(timer_name)) {
		if (switch_core_timer_init(&rtp_session->timer, timer_name, ms_per_packet / 1000, samples_per_interval, pool) ==
			SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Starting timer [%s] %d bytes per %dms\n", timer_name, samples_per_interval,
							  ms_per_packet);
		} else {
			memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error starting timer [%s], async RTP disabled\n", timer_name);
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		}
	}

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
											  switch_rtp_flag_t flags, char *crypto_key, char *timer_name, const char **err, switch_memory_pool_t *pool)
{
	switch_rtp_t *rtp_session = NULL;
	
	if (switch_rtp_create(&rtp_session, payload, samples_per_interval, ms_per_packet, flags, crypto_key, timer_name, err, pool) != SWITCH_STATUS_SUCCESS) {
		goto end;
	}
	
	if (switch_rtp_set_remote_address(rtp_session, tx_host, tx_port, err) != SWITCH_STATUS_SUCCESS) {
		rtp_session = NULL;
		goto end;
	}

	if (switch_rtp_set_local_address(rtp_session, rx_host, rx_port, err) != SWITCH_STATUS_SUCCESS) {
		rtp_session = NULL;
	}

 end:

	if (rtp_session) {
		rtp_session->ready = 1;
		rtp_session->rx_host = switch_core_strdup(rtp_session->pool, rx_host);
		rtp_session->rx_port = rx_port;
	} else {
		release_port(rx_host, rx_port);
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

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_rtp_kill_socket(switch_rtp_t *rtp_session)
{
	switch_assert(rtp_session != NULL);
	switch_mutex_lock(rtp_session->flag_mutex);
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
		switch_assert(rtp_session->sock != NULL);
		switch_socket_shutdown(rtp_session->sock, SWITCH_SHUTDOWN_READWRITE);
		switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_IO);
	}
	switch_mutex_unlock(rtp_session->flag_mutex);
}

SWITCH_DECLARE(uint8_t) switch_rtp_ready(switch_rtp_t *rtp_session)
{
	return (rtp_session != NULL && rtp_session->sock && rtp_session->ready) ? 1 : 0;
}

SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp_t **rtp_session)
{
	if (!switch_rtp_ready(*rtp_session)) {
		return;
	}


	(*rtp_session)->ready = 0;

	switch_mutex_lock((*rtp_session)->flag_mutex);

	if ((*rtp_session)->jb) {
		stfu_n_destroy(&(*rtp_session)->jb);
	}

	if ((*rtp_session)->dtmf_data.dtmf_buffer) {
		switch_buffer_destroy(&(*rtp_session)->dtmf_data.dtmf_buffer);
	}

	switch_rtp_kill_socket(*rtp_session);
	switch_socket_close((*rtp_session)->sock);
	(*rtp_session)->sock = NULL;


	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_VAD)) {
		switch_rtp_disable_vad(*rtp_session);
	}

	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_SECURE)) {
		srtp_dealloc((*rtp_session)->recv_ctx);
		srtp_dealloc((*rtp_session)->send_ctx);
	}

	if ((*rtp_session)->timer.timer_interface) {
		switch_core_timer_destroy(&(*rtp_session)->timer);
	}

	release_port((*rtp_session)->rx_host, (*rtp_session)->rx_port);

	switch_mutex_unlock((*rtp_session)->flag_mutex);
	return;
}

SWITCH_DECLARE(switch_socket_t *) switch_rtp_get_rtp_socket(switch_rtp_t *rtp_session)
{
	return rtp_session->sock;
}

SWITCH_DECLARE(void) switch_rtp_set_default_samples_per_interval(switch_rtp_t *rtp_session, uint16_t samples_per_interval)
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

SWITCH_DECLARE(uint8_t) switch_rtp_test_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags)
{
	return (uint8_t) switch_test_flag(rtp_session, flags);
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
		int x, loops = 1, duration;
		rtp_session->dtmf_data.out_digit_sofar += samples;

		if (rtp_session->dtmf_data.out_digit_sofar >= rtp_session->dtmf_data.out_digit_dur) {
			duration = rtp_session->dtmf_data.out_digit_dur;
			rtp_session->dtmf_data.out_digit_packet[1] |= 0x80;
			rtp_session->dtmf_data.out_digit_dur = 0;
			loops = 3;
		} else {
			duration = rtp_session->dtmf_data.out_digit_sofar;
		}

		rtp_session->dtmf_data.out_digit_packet[2] = (unsigned char) (duration >> 8);
		rtp_session->dtmf_data.out_digit_packet[3] = (unsigned char) duration;
		rtp_session->dtmf_data.timestamp_dtmf += samples;
		
		for (x = 0; x < loops; x++) {
			rtp_session->seq++;

			switch_rtp_write_manual(rtp_session,
									rtp_session->dtmf_data.out_digit_packet,
									4,
									0,
									rtp_session->te,
									rtp_session->dtmf_data.timestamp_dtmf, rtp_session->seq, rtp_session->dtmf_data.out_digit_ssrc,
									&flags);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send %s packet for [%c] ts=%d sofar=%u dur=%d seq=%d\n",
							  loops == 1 ? "middle" : "end", rtp_session->dtmf_data.out_digit, rtp_session->dtmf_data.timestamp_dtmf,
							  rtp_session->dtmf_data.out_digit_sofar, duration, rtp_session->seq);
		}

		if (loops != 1) {
			rtp_session->sending_dtmf = 0;
		}
	}

	if (!rtp_session->dtmf_data.out_digit_dur && rtp_session->dtmf_data.dtmf_queue && switch_queue_size(rtp_session->dtmf_data.dtmf_queue)) {
		void *pop;

		if (switch_queue_trypop(rtp_session->dtmf_data.dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			int x;
			struct rfc2833_digit *rdigit = pop;

			memset(rtp_session->dtmf_data.out_digit_packet, 0, 4);
			rtp_session->dtmf_data.out_digit_sofar = 0;
			rtp_session->dtmf_data.out_digit_dur = rdigit->duration;
			rtp_session->dtmf_data.out_digit = rdigit->digit;
			rtp_session->dtmf_data.out_digit_packet[0] = (unsigned char) switch_char_to_rfc2833(rdigit->digit);
			rtp_session->dtmf_data.out_digit_packet[1] = 7;

			if (rtp_session->timer.timer_interface) {
				rtp_session->dtmf_data.timestamp_dtmf = rtp_session->timer.samplecount;
			} else {
				rtp_session->dtmf_data.timestamp_dtmf = rtp_session->last_write_ts;
			}
			
			rtp_session->sending_dtmf = 1;

			for (x = 0; x < 3; x++) {
				rtp_session->seq++;
				switch_rtp_write_manual(rtp_session,
										rtp_session->dtmf_data.out_digit_packet,
										4,
										switch_test_flag(rtp_session, SWITCH_RTP_FLAG_BUGGY_2833) ? 0 : 1,
										rtp_session->te,
										rtp_session->dtmf_data.timestamp_dtmf,
										rtp_session->seq, 
										rtp_session->dtmf_data.out_digit_ssrc, &flags);
				switch_log_printf(SWITCH_CHANNEL_LOG,
								  SWITCH_LOG_DEBUG,
								  "Send start packet for [%c] ts=%d sofar=%u dur=%d seq=%d\n",
								  rtp_session->dtmf_data.out_digit,
								  rtp_session->dtmf_data.timestamp_dtmf, rtp_session->dtmf_data.out_digit_sofar, 0, rtp_session->seq);
			}
			rtp_session->dtmf_data.timestamp_dtmf += samples;
			free(rdigit);
		}
	}
}

static int rtp_common_read(switch_rtp_t *rtp_session, switch_payload_t *payload_type, switch_frame_flag_t *flags)
{
	switch_size_t bytes = 0;
	switch_status_t status;
	uint8_t check = 1;
	stfu_frame_t *jb_frame;
	
	if (!rtp_session->timer.interval) {
		rtp_session->last_time = switch_time_now();
	}

	while (switch_rtp_ready(rtp_session)) {
		bytes = sizeof(rtp_msg_t);
		status = switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock, 0, (void *) &rtp_session->recv_msg, &bytes);

		if (!SWITCH_STATUS_IS_BREAK(status) && rtp_session->timer.interval) {
			switch_core_timer_step(&rtp_session->timer);
		}

		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO)) {
			return -1;
		}

		if (rtp_session->jb && bytes && rtp_session->recv_msg.header.pt == rtp_session->payload) {
			if (rtp_session->recv_msg.header.m) {
				stfu_n_reset(rtp_session->jb);
			} 
			
			stfu_n_eat(rtp_session->jb, ntohl(rtp_session->recv_msg.header.ts), rtp_session->recv_msg.body, bytes - rtp_header_len);
			if ((jb_frame = stfu_n_read_a_frame(rtp_session->jb))) {
				memcpy(rtp_session->recv_msg.body, jb_frame->data, jb_frame->dlen);
				if (jb_frame->plc) {
					*flags |= SFF_PLC;
					}
				bytes = jb_frame->dlen + rtp_header_len;
				rtp_session->recv_msg.header.ts = htonl(jb_frame->ts);
			} else {
				bytes = 0;
				continue;
			}			
		}

		if (!bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_BREAK)) {
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_BREAK);

			memset(&rtp_session->recv_msg.body, 0, 2);
			rtp_session->recv_msg.body[0] = 127;
			rtp_session->recv_msg.header.pt = SWITCH_RTP_CNG_PAYLOAD;
			*flags |= SFF_CNG;
			/* Return a CNG frame */
			*payload_type = SWITCH_RTP_CNG_PAYLOAD;
			return 2 + rtp_header_len;
		}

		if (bytes < 0) {
			return (int) bytes;
		}

		if (rtp_session->timer.interval) {
			check = (uint8_t) (switch_core_timer_check(&rtp_session->timer) == SWITCH_STATUS_SUCCESS);

			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTO_CNG) &&
				rtp_session->timer.samplecount >= (rtp_session->last_write_samplecount + (rtp_session->samples_per_interval * 50))) {
				uint8_t data[2] = { 0 };
				switch_frame_flag_t flags = SFF_NONE;
				data[0] = 65;
				rtp_session->cn++;
				rtp_common_write(rtp_session, (void *) data, sizeof(data), rtp_session->cng_pt, &flags);
			}
		}

		if (check) {
			do_2833(rtp_session);
			if (!bytes && rtp_session->max_missed_packets) {
				if (++rtp_session->missed_count >= rtp_session->max_missed_packets) {
					return -2;
				}
			}
			
			if (rtp_session->jb && (jb_frame = stfu_n_read_a_frame(rtp_session->jb))) {
				memcpy(rtp_session->recv_msg.body, jb_frame->data, jb_frame->dlen);
				if (jb_frame->plc) {
					*flags |= SFF_PLC;
				}
				bytes = jb_frame->dlen + rtp_header_len;
				rtp_session->recv_msg.header.ts = htonl(jb_frame->ts);
			} else if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) { /* We're late! We're Late! */
				uint8_t *data = (uint8_t *) rtp_session->recv_msg.body;				
				
				if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK) && status == SWITCH_STATUS_BREAK) {
					switch_yield(1000);
					continue;
				}
				
				memset(data, 0, 2);
				data[0] = 65;
				
				rtp_session->recv_msg.header.pt = (uint32_t) rtp_session->cng_pt ? rtp_session->cng_pt : SWITCH_RTP_CNG_PAYLOAD;
				*flags |= SFF_CNG;
				*payload_type = (switch_payload_t)rtp_session->recv_msg.header.pt;
				return 2 + rtp_header_len;
			}
		}

		if (bytes && rtp_session->recv_msg.header.version != 2) {
			uint8_t *data = (uint8_t *) rtp_session->recv_msg.body;
			if (rtp_session->recv_msg.header.version == 0 && rtp_session->ice_user) {
				handle_ice(rtp_session, (void *) &rtp_session->recv_msg, bytes);
			}

			if (rtp_session->invalid_handler) {
				rtp_session->invalid_handler(rtp_session, rtp_session->sock, (void *) &rtp_session->recv_msg, bytes, rtp_session->from_addr);
			}
			
			memset(data, 0, 2);
			data[0] = 65;

			rtp_session->recv_msg.header.pt = (uint32_t) rtp_session->cng_pt ? rtp_session->cng_pt : SWITCH_RTP_CNG_PAYLOAD;
			*flags |= SFF_CNG;
			*payload_type = (switch_payload_t)rtp_session->recv_msg.header.pt;
			return 2 + rtp_header_len;
		}

		if (bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
			int sbytes = (int) bytes;
			err_status_t stat;

			stat = srtp_unprotect(rtp_session->recv_ctx, &rtp_session->recv_msg.header, &sbytes);
			if (stat) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
								  "error: srtp unprotection failed with code %d%s\n", stat,
								  stat == err_status_replay_fail ? " (replay check failed)" : stat == err_status_auth_fail ? " (auth check failed)" : "");
				return -1;
			}
			bytes = sbytes;
		}

		if (bytes > 0) {
			rtp_session->missed_count = 0;
		}

		if (status == SWITCH_STATUS_BREAK || bytes == 0) {
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_DATAWAIT)) {
				goto do_continue;
			}
			return 0;
		}
		
		if (bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ) && switch_sockaddr_get_port(rtp_session->from_addr)) {
			const char *tx_host;
			const char *old_host;
			char bufa[30], bufb[30];
			tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->from_addr);
			old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
			if ((switch_sockaddr_get_port(rtp_session->from_addr) != rtp_session->remote_port) || strcmp(tx_host, old_host)) {
				const char *err;
				uint32_t old = rtp_session->remote_port;

				if (!switch_strlen_zero(tx_host) && switch_sockaddr_get_port(rtp_session->from_addr) > 0) {
					if (++rtp_session->autoadj_tally >= 10) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
										  "Auto Changing port from %s:%u to %s:%u\n", old_host, old, tx_host,
										  switch_sockaddr_get_port(rtp_session->from_addr));
						switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(rtp_session->from_addr), &err);
					}
				}
			}
		}

		if (rtp_session->autoadj_window) {
			if (--rtp_session->autoadj_window == 0) {
				switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}

		if (bytes && rtp_session->cng_pt && rtp_session->recv_msg.header.pt == rtp_session->cng_pt) {
			goto do_continue;
		}

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_GOOGLEHACK) && rtp_session->recv_msg.header.pt == 102) {
			rtp_session->recv_msg.header.pt = 97;
		}

		rtp_session->rseq = ntohs((uint16_t) rtp_session->recv_msg.header.seq);
		rtp_session->rpayload = (switch_payload_t) rtp_session->recv_msg.header.pt;

		/* RFC2833 ... TBD try harder to honor the duration etc. */
		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PASS_RFC2833)
			&& rtp_session->recv_msg.header.pt == rtp_session->te) {
			unsigned char *packet = (unsigned char *) rtp_session->recv_msg.body;
			int end = packet[1] & 0x80;
			int duration = (packet[2] << 8) + packet[3];
			char key = switch_rfc2833_to_char(packet[0]);
			uint16_t in_digit_seq = ntohs((uint16_t) rtp_session->recv_msg.header.seq);

			/* SHEESH.... Curse you RFC2833 inventors!!!! */
			if ((time(NULL) - rtp_session->dtmf_data.last_digit_time) > 2) {
				rtp_session->dtmf_data.last_digit = 0;
				rtp_session->dtmf_data.dc = 0;
				rtp_session->dtmf_data.in_digit_seq = 0;
			}
			if (in_digit_seq > rtp_session->dtmf_data.in_digit_seq) {
				rtp_session->dtmf_data.in_digit_seq = in_digit_seq;
				if (duration && end) {
					if (key != rtp_session->dtmf_data.last_digit) {
						char digit_str[] = { key, 0 };
						time(&rtp_session->dtmf_data.last_digit_time);
						switch_rtp_queue_dtmf(rtp_session, digit_str);
						switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_BREAK);
					}
					if (++rtp_session->dtmf_data.dc >= 3) {
						rtp_session->dtmf_data.last_digit = 0;
						rtp_session->dtmf_data.dc = 0;
					}

					rtp_session->dtmf_data.last_digit = key;
				} else {
					rtp_session->dtmf_data.last_digit = 0;
					rtp_session->dtmf_data.dc = 0;
				}
			}
			goto do_continue;
		}
		break;

	do_continue:

		if (rtp_session->ms_per_packet) {
			switch_yield((rtp_session->ms_per_packet / 1000) * 750);
		} else {
			switch_yield(1000);
		}
	}

	*payload_type = (switch_payload_t) rtp_session->recv_msg.header.pt;

	if (*payload_type == SWITCH_RTP_CNG_PAYLOAD) {
		*flags |= SFF_CNG;
	}

	if (bytes > 0) {
		do_2833(rtp_session);
	}

	return (int) bytes;
}

SWITCH_DECLARE(switch_size_t) switch_rtp_has_dtmf(switch_rtp_t *rtp_session)
{
	switch_size_t has = 0;

	if (switch_rtp_ready(rtp_session)) {
		switch_mutex_lock(rtp_session->dtmf_data.dtmf_mutex);
		has = switch_buffer_inuse(rtp_session->dtmf_data.dtmf_buffer);
		switch_mutex_unlock(rtp_session->dtmf_data.dtmf_mutex);
	}

	return has;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_queue_dtmf(switch_rtp_t *rtp_session, char *dtmf)
{
	switch_status_t status;
	register switch_size_t len, inuse;
	switch_size_t wr = 0;
	char *p;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(rtp_session->dtmf_data.dtmf_mutex);

	inuse = switch_buffer_inuse(rtp_session->dtmf_data.dtmf_buffer);
	len = strlen(dtmf);

	if (len + inuse > switch_buffer_len(rtp_session->dtmf_data.dtmf_buffer)) {
		switch_buffer_toss(rtp_session->dtmf_data.dtmf_buffer, strlen(dtmf));
	}

	p = dtmf;
	while (wr < len && p) {
		if (is_dtmf(*p)) {
			wr++;
		} else {
			break;
		}
		p++;
	}

	status = switch_buffer_write(rtp_session->dtmf_data.dtmf_buffer, dtmf, wr) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_MEMERR;
	switch_mutex_unlock(rtp_session->dtmf_data.dtmf_mutex);

	return status;
}


SWITCH_DECLARE(switch_size_t) switch_rtp_dequeue_dtmf(switch_rtp_t *rtp_session, char *dtmf, switch_size_t len)
{
	switch_size_t bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return bytes;
	}

	switch_mutex_lock(rtp_session->dtmf_data.dtmf_mutex);
	if ((bytes = switch_buffer_read(rtp_session->dtmf_data.dtmf_buffer, dtmf, len)) > 0) {
		*(dtmf + bytes) = '\0';
	}
	switch_mutex_unlock(rtp_session->dtmf_data.dtmf_mutex);

	return bytes;
}


SWITCH_DECLARE(switch_status_t) switch_rtp_queue_rfc2833(switch_rtp_t *rtp_session, char *digits, uint32_t duration)
{
	char *c;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!rtp_session->dtmf_data.dtmf_queue) {
		switch_queue_create(&rtp_session->dtmf_data.dtmf_queue, 100, rtp_session->pool);
	}

	for (c = digits; *c; c++) {
		struct rfc2833_digit *rdigit;

		if ((rdigit = malloc(sizeof(*rdigit))) != 0) {
			memset(rdigit, 0, sizeof(*rdigit));
			rdigit->digit = *c;
			rdigit->duration = duration;
			switch_queue_push(rtp_session->dtmf_data.dtmf_queue, rdigit);
		} else {
			return SWITCH_STATUS_MEMERR;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_read(switch_rtp_t *rtp_session, void *data, uint32_t * datalen,
												switch_payload_t *payload_type, switch_frame_flag_t *flags)
{
	int bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = rtp_common_read(rtp_session, payload_type, flags);

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

SWITCH_DECLARE(switch_status_t) switch_rtp_zerocopy_read_frame(switch_rtp_t *rtp_session, switch_frame_t *frame)
{
	int bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = rtp_common_read(rtp_session, &frame->payload, &frame->flags);
	
	frame->data = rtp_session->recv_msg.body;
	frame->packet = &rtp_session->recv_msg;
	frame->packetlen = bytes;
	frame->source = __FILE__;
	frame->flags |= SFF_RAW_RTP;
	if (frame->payload == rtp_session->te) {
		frame->flags |= SFF_RFC2833;
	}
	frame->timestamp = ntohl(rtp_session->recv_msg.header.ts);
	frame->seq = (uint16_t)ntohs((u_short)rtp_session->recv_msg.header.seq);
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
														 void **data, uint32_t * datalen, switch_payload_t *payload_type, switch_frame_flag_t *flags)
{
	int bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = rtp_common_read(rtp_session, payload_type, flags);
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

static int rtp_common_write(switch_rtp_t *rtp_session, void *data, uint32_t datalen, switch_payload_t payload, switch_frame_flag_t *flags)
{
	switch_size_t bytes;
	uint8_t fwd = 0;
	rtp_msg_t *send_msg;
	uint8_t send = 1;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	fwd = (uint8_t) (!flags || (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) && (*flags & SFF_RAW_RTP)));

	if (fwd) {
		bytes = datalen;
		send_msg = (rtp_msg_t *) data;
		if (*flags & SFF_RFC2833) {
			send_msg->header.pt = rtp_session->te;
		}
	} else {
		uint8_t m = 0;
		
		if (*flags & SFF_RFC2833) {
			payload = rtp_session->te;
		}

		if ((rtp_session->ts > (rtp_session->last_write_ts + (rtp_session->samples_per_interval * 10)))
			|| rtp_session->ts == rtp_session->samples_per_interval) {
			m++;
		}

		if (rtp_session->cn && payload != rtp_session->cng_pt) {
			rtp_session->cn = 0;
			m++;
		}
		
		send_msg = &rtp_session->send_msg;
		send_msg->header.pt = payload;
		send_msg->header.m = m ? 1 : 0;
		rtp_session->seq++;
		rtp_session->send_msg.header.seq = htons(rtp_session->seq);
		rtp_session->send_msg.header.ts = htonl(rtp_session->ts);

		memcpy(send_msg->body, data, datalen);
		bytes = datalen + rtp_header_len;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
		int sbytes = (int) bytes;
		err_status_t stat;

		stat = srtp_protect(rtp_session->send_ctx, &send_msg->header, &sbytes);
		if (stat) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error: srtp protection failed with code %d\n", stat);
		}

		bytes = sbytes;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_GOOGLEHACK) && rtp_session->send_msg.header.pt == 97) {
		rtp_session->recv_msg.header.pt = 102;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VAD) &&
		rtp_session->recv_msg.header.pt == rtp_session->vad_data.read_codec->implementation->ianacode) {
		
		int16_t decoded[SWITCH_RECOMMENDED_BUFFER_SIZE / sizeof(int16_t)] = {0};
		uint32_t rate = 0;
		uint32_t flags = 0;
		uint32_t len = sizeof(decoded);
		time_t now = time(NULL);
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
									 decoded, &len, &rate, &flags) == SWITCH_STATUS_SUCCESS) {
			

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
										switch_channel_t *channel = switch_core_session_get_channel(rtp_session->vad_data.session);
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
											switch_channel_t *channel = switch_core_session_get_channel(rtp_session->vad_data.session);
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
				} 
			}
		} else {
			return SWITCH_STATUS_GENERR;
		}
	}

	if (rtp_session->sending_dtmf) {
		send = 0;
	}

	if (send) {

		rtp_session->last_write_ts = ntohl(send_msg->header.ts);
		rtp_session->last_write_ssrc = ntohl(send_msg->header.ssrc);
		rtp_session->last_write_seq = rtp_session->seq;
		if (rtp_session->timer.interval) {
			switch_core_timer_check(&rtp_session->timer);
			rtp_session->last_write_samplecount = rtp_session->timer.samplecount;
		}
		switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void *) send_msg, &bytes);
	} else if (!fwd) {
		/* nevermind save this seq inc for next time */
		rtp_session->seq--;
		rtp_session->send_msg.header.seq = htons(rtp_session->seq);
	}

	if (rtp_session->ice_user) {
		if (ice_out(rtp_session) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}
	}

	return (int) bytes;
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
							   codec->implementation->microseconds_per_frame / 1000,
							   codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Activate VAD codec %s %dms\n", codec->implementation->iananame, 
					  codec->implementation->microseconds_per_frame / 1000);
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
	switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_VAD);
	switch_set_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_CNG);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_rtp_write(switch_rtp_t *rtp_session, void *data, uint32_t datalen, uint32_t ts, switch_frame_flag_t *flags)
{

	if (!switch_rtp_ready(rtp_session)) {
		return -1;
	}

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}

	if (ts) {
		rtp_session->ts = ts;
	} else if (!ts && rtp_session->timer.timer_interface) {
		uint32_t sc = rtp_session->timer.samplecount;
		if (rtp_session->last_write_ts == sc) {
			rtp_session->ts = sc + rtp_session->samples_per_interval;
		} else {
			rtp_session->ts = sc;
		}
	} else {
		rtp_session->ts += rtp_session->samples_per_interval;
	}

	return rtp_common_write(rtp_session, data, datalen, rtp_session->payload, flags);
}

SWITCH_DECLARE(int) switch_rtp_write_frame(switch_rtp_t *rtp_session, switch_frame_t *frame, uint32_t ts)
{
	uint8_t fwd = 0;
	void *data;
	uint32_t len;
	switch_payload_t payload;

	if (!switch_rtp_ready(rtp_session)) {
		return -1;
	}

	fwd = (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) && switch_test_flag(frame, SFF_RAW_RTP)) ? 1 : 0;

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}

	switch_assert(frame != NULL);

	if (switch_test_flag(frame, SFF_CNG)) {
		payload = rtp_session->cng_pt;
	} else {
		payload = rtp_session->payload;
	}

	if (switch_test_flag(frame, SFF_RTP_HEADER)) {
		return switch_rtp_write_manual(rtp_session, frame->data, frame->datalen, frame->m, frame->payload, 
									   (uint32_t)(frame->timestamp), frame->seq, frame->ssrc, &frame->flags);
	}

	if (fwd) {
		data = frame->packet;
		len = frame->packetlen;
	} else {
		data = frame->data;
		len = frame->datalen;

		if (ts) {
			rtp_session->ts = ts;
		} else if (frame->timestamp) {
			rtp_session->ts = (uint32_t) frame->timestamp;
		} else if (rtp_session->timer.timer_interface) {
			uint32_t sc = rtp_session->timer.samplecount;
			if (sc <= rtp_session->last_write_ts) {
				sc = rtp_session->last_write_ts + rtp_session->samples_per_interval;
			}
			rtp_session->ts = sc;
		} else {
			rtp_session->ts += rtp_session->samples_per_interval;
		}
	}

	return rtp_common_write(rtp_session, data, len, payload, &frame->flags);
}

SWITCH_DECLARE(int) switch_rtp_write_manual(switch_rtp_t *rtp_session,
											void *data,
											uint32_t datalen,
											uint8_t m, switch_payload_t payload, uint32_t ts, uint16_t mseq, uint32_t ssrc, switch_frame_flag_t *flags)
{
	rtp_msg_t send_msg = { {0} };
	switch_size_t bytes;

	if (!switch_rtp_ready(rtp_session)) {
		return -1;
	}

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) || !rtp_session->remote_addr) {
		return -1;
	}

	send_msg = rtp_session->send_msg;
	send_msg.header.seq = htons(mseq);
	send_msg.header.ts = htonl(ts);
	send_msg.header.ssrc = htonl(ssrc);
	send_msg.header.pt = payload;
	send_msg.header.m = m ? 1 : 0;
	memcpy(send_msg.body, data, datalen);

	bytes = rtp_header_len + datalen;


	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE)) {
		int sbytes = (int) bytes;
		err_status_t stat;

		stat = srtp_protect(rtp_session->send_ctx, &send_msg.header, &sbytes);
		if (stat) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error: srtp protection failed with code %d\n", stat);
		}

		bytes = sbytes;
	}

	if (switch_socket_sendto(rtp_session->sock, rtp_session->remote_addr, 0, (void *) &send_msg, &bytes) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}
	return (int) bytes;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
