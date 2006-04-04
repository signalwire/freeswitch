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
#define USEC_RATE        (5e5)
#define MAX_WORD_LEN     128  
#define ADDR_IS_MULTICAST(a) IN_MULTICAST(htonl(a))
#define MAX_KEY_LEN      64
#define MASTER_KEY_LEN   30

#include <switch.h>
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#include <datatypes.h>
#include <srtp.h>

#define rtp_header_len 12

typedef srtp_hdr_t rtp_hdr_t;

#define RTP_MAX_BUF_LEN 16384

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
};

static int global_init = 0;

static void init_rtp(void)
{
	if (global_init) {
		return;
	}

  srtp_init();
  global_init = 1;

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

	if (!global_init) {
		init_rtp();
	}

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

SWITCH_DECLARE(void) switch_rtp_killread(switch_rtp *rtp_session)
{
	apr_socket_shutdown(rtp_session->sock, APR_SHUTDOWN_READWRITE);
	switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_IO);
	
}


SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp **rtp_session)
{

	switch_rtp_killread(*rtp_session);
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
