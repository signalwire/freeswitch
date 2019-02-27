/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * Seven Du <dujinfang@gmail.com>
 *
 * switch_rtp.c -- RTP
 *
 */
#include <switch.h>
#ifndef _MSC_VER
#include <switch_private.h>
#endif
#include <switch_stun.h>
#include <apr_network_io.h>
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#undef VERSION
#undef PACKAGE
#undef inline
#include <srtp.h>
#include <srtp_priv.h>
#include <switch_ssl.h>
#include <switch_jitterbuffer.h>

//#define DEBUG_TS_ROLLOVER
//#define TS_ROLLOVER_START 4294951295

//#define DEBUG_2833
//#define RTP_DEBUG_WRITE_DELTA
//#define DEBUG_MISSED_SEQ
//#define DEBUG_EXTRA
//#define DEBUG_RTCP
#define DEBUG_ESTIMATORS_


#define JITTER_LEAD_FRAMES 10
#define READ_INC(rtp_session) switch_mutex_lock(rtp_session->read_mutex); rtp_session->reading++
#define READ_DEC(rtp_session)  switch_mutex_unlock(rtp_session->read_mutex); rtp_session->reading--
#define WRITE_INC(rtp_session)  switch_mutex_lock(rtp_session->write_mutex); rtp_session->writing++
#define WRITE_DEC(rtp_session) switch_mutex_unlock(rtp_session->write_mutex); rtp_session->writing--

#define RTP_STUN_FREQ 1000000
#define rtp_header_len 12
#define RTP_START_PORT 16384
#define RTP_END_PORT 32768
#define MASTER_KEY_LEN   30
#define RTP_MAGIC_NUMBER 42
#define WARN_SRTP_ERRS 10
#define MAX_SRTP_ERRS 100
#define NTP_TIME_OFFSET 2208988800UL
#define ZRTP_MAGIC_COOKIE 0x5a525450
static const switch_payload_t INVALID_PT = 255;

#define DTMF_SANITY (rtp_session->one_second * 30)

#define rtp_session_name(_rtp_session) _rtp_session->session ? switch_core_session_get_name(_rtp_session->session) : "-"

static switch_port_t START_PORT = RTP_START_PORT;
static switch_port_t END_PORT = RTP_END_PORT;
static switch_mutex_t *port_lock = NULL;
static switch_size_t do_flush(switch_rtp_t *rtp_session, int force, switch_size_t bytes_in);

typedef srtp_hdr_t rtp_hdr_t;

#ifdef ENABLE_ZRTP
#include "zrtp.h"
static zrtp_global_t *zrtp_global;
#ifndef WIN32
static zrtp_zid_t zid = { "FreeSWITCH01" };
#else
static zrtp_zid_t zid = { "FreeSWITCH0" };
#endif
static int zrtp_on = 0;
#define ZRTP_MITM_TRIES 100
#endif

#ifdef _MSC_VER
#pragma pack(4)
#endif

#ifdef _MSC_VER
#pragma pack()
#define ENABLE_SRTP
#endif

static switch_hash_t *alloc_hash = NULL;

typedef struct {
	srtp_hdr_t header;
	char body[SWITCH_RTP_MAX_BUF_LEN+4+sizeof(char *)];
	switch_rtp_hdr_ext_t *ext;
	char *ebody;
} rtp_msg_t;

#define RTP_BODY(_s) (char *) (_s->recv_msg.ebody ? _s->recv_msg.ebody : _s->recv_msg.body)

typedef struct {
	uint32_t ssrc;
	uint8_t seq;
	uint8_t r1;
	uint8_t r2;
	uint8_t r3;
} rtcp_fir_t;

#ifdef _MSC_VER
#pragma pack(push, r1, 1)
#endif

typedef struct switch_rtcp_sdes_unit_s {
	unsigned char type;
	unsigned char length;
	char value[];
} switch_rtcp_sdes_unit_t;

typedef struct {
	uint32_t ssrc;
	uint8_t parts[4];
} rtcp_tmmbx_t;

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN

typedef struct {
	unsigned version:2;
	unsigned p:1;
	unsigned fmt:5;
	unsigned pt:8;
	unsigned length:16;
	uint32_t send_ssrc;
	uint32_t recv_ssrc;
} switch_rtcp_ext_hdr_t;

#else /*  BIG_ENDIAN */

typedef struct {
	unsigned fmt:5;
	unsigned p:1;
	unsigned version:2;
	unsigned pt:8;
	unsigned length:16;
	uint32_t send_ssrc;
	uint32_t recv_ssrc;
} switch_rtcp_ext_hdr_t;

#endif

#ifdef _MSC_VER
#pragma pack(pop, r1)
#endif

#define KALMAN_SYSTEM_MODELS 3 /*loss, jitter, rtt*/
#define EST_LOSS 0
#define EST_JITTER 1
#define EST_RTT 2

typedef struct {
	switch_rtcp_ext_hdr_t header;
	char body[SWITCH_RTCP_MAX_BUF_LEN];
} rtcp_ext_msg_t;

typedef struct {
	switch_rtcp_hdr_t header;
	char body[SWITCH_RTCP_MAX_BUF_LEN];
} rtcp_msg_t;


typedef enum {
	VAD_FIRE_TALK = (1 << 0),
	VAD_FIRE_NOT_TALK = (1 << 1)
} vad_talk_mask_t;

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
	switch_time_t start_talking;
	switch_time_t stop_talking;
	switch_time_t total_talk_time;
	int fire_events;
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
	uint32_t last_in_digit_ts;
	uint32_t in_digit_sanity;
	uint32_t in_interleaved;
	uint32_t timestamp_dtmf;
	uint16_t last_duration;
	uint32_t flip;
	char first_digit;
	char last_digit;
	switch_queue_t *dtmf_inqueue;
	switch_mutex_t *dtmf_mutex;
	uint8_t in_digit_queued;
};

typedef struct {
	char *ice_user;
	char *user_ice;
	char *luser_ice;
	char *pass;
	char *rpass;
	switch_sockaddr_t *addr;
	uint32_t funny_stun;
	switch_time_t next_run;
	switch_core_media_ice_type_t type;
	ice_t *ice_params;
	ice_proto_t proto;
	uint8_t sending;
	uint8_t ready;
	uint8_t rready;
	int missed_count;
	char last_sent_id[13];
	switch_time_t last_ok;
} switch_rtp_ice_t;

struct switch_rtp;

#define MAX_DTLS_MTU 4096

typedef struct switch_dtls_s {
	/* DTLS */
	SSL_CTX *ssl_ctx;
	SSL *ssl;
	BIO *read_bio;
	BIO *write_bio;
	BIO *filter_bio;
	dtls_fingerprint_t *local_fp;
	dtls_fingerprint_t *remote_fp;
	dtls_state_t state;
	dtls_state_t last_state;
	uint8_t new_state;
	dtls_type_t type;
	switch_size_t bytes;
	void *data;
	switch_socket_t *sock_output;
	switch_sockaddr_t *remote_addr;
	char *rsa;
	char *pvt;
	char *ca;
	char *pem;
	struct switch_rtp *rtp_session;
	int mtu;
} switch_dtls_t;

typedef int (*dtls_state_handler_t)(switch_rtp_t *, switch_dtls_t *);


static int dtls_state_handshake(switch_rtp_t *rtp_session, switch_dtls_t *dtls);
static int dtls_state_ready(switch_rtp_t *rtp_session, switch_dtls_t *dtls);
static int dtls_state_setup(switch_rtp_t *rtp_session, switch_dtls_t *dtls);
static int dtls_state_fail(switch_rtp_t *rtp_session, switch_dtls_t *dtls);

dtls_state_handler_t dtls_states[DS_INVALID] = {NULL, dtls_state_handshake, dtls_state_setup, dtls_state_ready, dtls_state_fail};

typedef struct ts_normalize_s {
	uint32_t last_ssrc;
	uint32_t last_frame;
	uint32_t ts;
	uint32_t delta;
	uint32_t delta_ttl;
	int last_external;
} ts_normalize_t;

struct switch_rtp {
	/*
	 * Two sockets are needed because we might be transcoding protocol families
	 * (e.g. receive over IPv4 and send over IPv6). In case the protocol
	 * families are equal, sock_input == sock_output and only one socket is
	 * used.
	 */
	switch_socket_t *sock_input, *sock_output, *rtcp_sock_input, *rtcp_sock_output;
	switch_pollfd_t *read_pollfd, *rtcp_read_pollfd;
	switch_pollfd_t *jb_pollfd;

	switch_sockaddr_t *local_addr, *rtcp_local_addr;
	rtp_msg_t send_msg;
	rtcp_msg_t rtcp_send_msg;
	switch_rtcp_frame_t rtcp_frame;

	uint8_t send_rr;
	uint8_t fir_seq;
	uint16_t fir_count;
	uint16_t pli_count;
	uint32_t cur_tmmbr;
	uint32_t tmmbr;
	uint32_t tmmbn;

	ts_normalize_t ts_norm;
	switch_sockaddr_t *remote_addr, *rtcp_remote_addr;
	rtp_msg_t recv_msg;
	rtcp_msg_t rtcp_recv_msg;
	rtcp_msg_t *rtcp_recv_msg_p;

	uint32_t autoadj_window;
	uint32_t autoadj_threshold;
	uint32_t autoadj_tally;

	uint32_t rtcp_autoadj_window;
	uint32_t rtcp_autoadj_threshold;
	uint32_t rtcp_autoadj_tally;

	srtp_ctx_t *send_ctx[2];
	srtp_ctx_t *recv_ctx[2];

	srtp_policy_t send_policy[2];
	srtp_policy_t recv_policy[2];

	uint32_t srtp_errs[2];
	uint32_t srctp_errs[2];


	int srtp_idx_rtp;
	int srtp_idx_rtcp;

	switch_dtls_t *dtls;
	switch_dtls_t *rtcp_dtls;

	rtp_hdr_t last_rtp_hdr;

	uint16_t seq;
	uint32_t ssrc;
	uint32_t remote_ssrc;
	uint32_t last_jb_read_ssrc;
	int8_t sending_dtmf;
	uint8_t need_mark;
	switch_payload_t payload;
	switch_rtp_invalid_handler_t invalid_handler;
	void *private_data;
	uint32_t ts;
	//uint32_t last_clock_ts;
	uint32_t last_write_ts;
	uint32_t last_read_ts;
	uint32_t last_cng_ts;
	uint32_t last_write_samplecount;
	uint32_t delay_samples;
	uint32_t next_write_samplecount;
	uint32_t max_next_write_samplecount;
	uint32_t queue_delay;
	switch_time_t last_write_timestamp;
	uint32_t flags[SWITCH_RTP_FLAG_INVALID];
	switch_memory_pool_t *pool;
	switch_sockaddr_t *from_addr, *rtp_from_addr, *rtcp_from_addr;
	char *rx_host;
	switch_port_t rx_port;
	switch_rtp_ice_t ice;
	switch_rtp_ice_t rtcp_ice;
	char *timer_name;
	char *local_host_str;
	char *remote_host_str;
	char *eff_remote_host_str;
	switch_time_t first_stun;
	switch_time_t last_stun;
	uint32_t wrong_addrs;
	uint32_t samples_per_interval;
	uint32_t samples_per_second;
	uint32_t conf_samples_per_interval;
	switch_time_t rtcp_last_sent;
	uint32_t rsamples_per_interval;
	uint32_t ms_per_packet;
	uint32_t one_second;
	uint32_t consecutive_flaws;
	uint32_t jitter_lead;
	double old_mean;
	switch_time_t next_stat_check_time;
	switch_port_t local_port;
	switch_port_t remote_port;
	switch_port_t eff_remote_port;
	switch_port_t remote_rtcp_port;

	struct switch_rtp_vad_data vad_data;
	struct switch_rtp_rfc2833_data dtmf_data;
	switch_payload_t te;
	switch_payload_t recv_te;
	switch_payload_t cng_pt;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *read_mutex;
	switch_mutex_t *write_mutex;
	switch_mutex_t *ice_mutex;
	switch_timer_t timer;
	switch_timer_t write_timer;
	uint8_t ready;
	uint8_t cn;
	switch_jb_t *jb;
	switch_jb_t *vb;
	switch_jb_t *vbw;
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
	switch_rtp_bug_flag_t rtp_bugs;
	switch_rtp_stats_t stats;
	uint32_t clean_stream;
	uint32_t bad_stream;
	uint32_t recovering_stream;

	uint32_t hot_hits;
	uint32_t sync_packets;
	int rtcp_interval;
	int rtcp_sent_packets;
	switch_bool_t rtcp_fresh_frame;

	switch_time_t send_time;
	switch_byte_t auto_adj_used;
	switch_byte_t rtcp_auto_adj_used;
	uint8_t pause_jb;
	uint16_t last_seq;
	uint16_t last_write_seq;
	uint8_t video_delta_mode;
	switch_time_t last_read_time;
	switch_size_t last_flush_packet_count;
	uint32_t interdigit_delay;
	switch_core_session_t *session;
	payload_map_t **pmaps;
	payload_map_t *pmap_tail;
	kalman_estimator_t *estimators[KALMAN_SYSTEM_MODELS];
	cusum_kalman_detector_t *detectors[KALMAN_SYSTEM_MODELS];
	int ice_adj;
	uint8_t has_rtp;
	uint8_t has_rtcp;
	uint8_t has_ice;
	uint8_t punts;
	uint8_t clean;
	uint32_t last_max_vb_frames;
	int skip_timer;
	uint32_t prev_nacks_inflight;
#ifdef ENABLE_ZRTP
	zrtp_session_t *zrtp_session;
	zrtp_profile_t *zrtp_profile;
	zrtp_stream_t *zrtp_stream;
	int zrtp_mitm_tries;
	int zinit;
#endif

};

struct switch_rtcp_report_block {
	uint32_t ssrc; /* The SSRC identifier of the source to which the information in this reception report block pertains. */
	unsigned int fraction :8; /* The fraction of RTP data packets from source SSRC_n lost since the previous SR or RR packet was sent */
	int lost :24; /* The total number of RTP data packets from source SSRC_n that have been lost since the beginning of reception */
	uint32_t highest_sequence_number_received;
	uint32_t jitter; /* An estimate of the statistical variance of the RTP data packet interarrival time, measured in timestamp units and expressed as an unsigned integer. */
	uint32_t lsr; /* The middle 32 bits out of 64 in the NTP timestamp */
	uint32_t dlsr; /* The delay, expressed in units of 1/65536 seconds, between receiving the last SR packet from source SSRC_n and sending this reception report block */
};

struct switch_rtcp_sr_head {
	uint32_t ssrc;
	uint32_t ntp_msw;
	uint32_t ntp_lsw;
	uint32_t ts;
	uint32_t pc;
	uint32_t oc;
};

struct switch_rtcp_sender_info {
	uint32_t ntp_msw;
	uint32_t ntp_lsw;
	uint32_t ts;
	uint32_t pc;
	uint32_t oc;
};

struct switch_rtcp_sender_report {
	uint32_t ssrc;
	struct switch_rtcp_sender_info sender_info;
	struct switch_rtcp_report_block report_block;
};

struct switch_rtcp_receiver_report {
	uint32_t ssrc;
	struct switch_rtcp_report_block report_block;
};

typedef enum {
	RESULT_CONTINUE,
	RESULT_GOTO_END,
	RESULT_GOTO_RECVFROM,
	RESULT_GOTO_TIMERCHECK
} handle_rfc2833_result_t;

static void do_2833(switch_rtp_t *rtp_session);


#define rtp_type(rtp_session) rtp_session->flags[SWITCH_RTP_FLAG_TEXT] ?  "text" : (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] ? "video" : "audio")


static void switch_rtp_change_ice_dest(switch_rtp_t *rtp_session, switch_rtp_ice_t *ice, const char *host, switch_port_t port)
{
	int is_rtcp = ice == &rtp_session->rtcp_ice;
	const char *err = "";

	ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].con_addr = switch_core_strdup(rtp_session->pool, host);
	ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].con_port = port;
	ice->missed_count = 0;

	if (is_rtcp) {
		ice->addr = rtp_session->rtcp_remote_addr;
	} else {
		switch_rtp_set_remote_address(rtp_session, host, port, 0, SWITCH_FALSE, &err);

		if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
			ice->addr = rtp_session->remote_addr;
		}
	}

}



static handle_rfc2833_result_t handle_rfc2833(switch_rtp_t *rtp_session, switch_size_t bytes, int *do_cng)
{

	if (rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON]) {
		rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON]++;

		if (rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON] > DTMF_SANITY) {
			rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON] = 0;
		} else {
			rtp_session->stats.inbound.last_processed_seq = 0;
		}
	}


#ifdef DEBUG_2833
	if (rtp_session->dtmf_data.in_digit_sanity && !(rtp_session->dtmf_data.in_digit_sanity % 100)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sanity %d %ld\n", rtp_session->dtmf_data.in_digit_sanity, bytes);
	}
#endif

	if (rtp_session->dtmf_data.in_digit_sanity && !--rtp_session->dtmf_data.in_digit_sanity) {

		rtp_session->dtmf_data.last_digit = 0;
		rtp_session->dtmf_data.in_digit_ts = 0;
		rtp_session->dtmf_data.in_digit_queued = 0;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Failed DTMF sanity check.\n");
	}

	if (!bytes) return RESULT_CONTINUE;


	/* RFC2833 ... like all RFC RE: VoIP, guaranteed to drive you to insanity!
	   We know the real rules here, but if we enforce them, it's an interop nightmare so,
	   we put up with as much as we can so we don't have to deal with being punished for
	   doing it right. Nice guys finish last!
	*/

	if (bytes > rtp_header_len && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] &&
		rtp_session->last_rtp_hdr.pt == rtp_session->recv_te) {
		switch_size_t len = bytes - rtp_header_len;
		unsigned char *packet = (unsigned char *) RTP_BODY(rtp_session);
		int end;
		uint16_t duration;
		char key;
		uint16_t in_digit_seq;
		uint32_t ts;

		rtp_session->stats.inbound.last_processed_seq = 0;

		if (!(packet[0] || packet[1] || packet[2] || packet[3]) && len >= 8) {
			packet += 4;
			len -= 4;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "DTMF payload offset by 4 bytes.\n");
		}

		if (!(packet[0] || packet[1] || packet[2] || packet[3]) && rtp_session->dtmf_data.in_digit_ts) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed DTMF payload check.\n");
			rtp_session->dtmf_data.last_digit = 0;
			rtp_session->dtmf_data.in_digit_ts = 0;
			rtp_session->dtmf_data.in_digit_sanity = 0;
			rtp_session->dtmf_data.in_digit_queued = 0;
		}

		end = packet[1] & 0x80 ? 1 : 0;
		duration = (packet[2] << 8) + packet[3];
		key = switch_rfc2833_to_char(packet[0]);
		in_digit_seq = ntohs((uint16_t) rtp_session->last_rtp_hdr.seq);
		ts = htonl(rtp_session->last_rtp_hdr.ts);

		if (rtp_session->flags[SWITCH_RTP_FLAG_PASS_RFC2833]) {

			if (end) {
				rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON] = DTMF_SANITY - 3;
			} else if (!rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON]) {
				rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON] = 1;
			}

			return RESULT_CONTINUE;
		}

		if (in_digit_seq < rtp_session->dtmf_data.in_digit_seq) {
			if (rtp_session->dtmf_data.in_digit_seq - in_digit_seq > 100) {
				rtp_session->dtmf_data.in_digit_seq = 0;
			}
		}
#ifdef DEBUG_2833
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "packet[%d]: %02x %02x %02x %02x\n", (int) len, (unsigned char) packet[0], (unsigned char) packet[1], (unsigned char) packet[2], (unsigned char) packet[3]);
#endif

		if (in_digit_seq > rtp_session->dtmf_data.in_digit_seq) {

			rtp_session->dtmf_data.in_digit_seq = in_digit_seq;
#ifdef DEBUG_2833

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "read: %c %u %u %u %u %d %d %s\n",
							  key, in_digit_seq, rtp_session->dtmf_data.in_digit_seq,
				   ts, duration, rtp_session->last_rtp_hdr.m, end, end && !rtp_session->dtmf_data.in_digit_ts ? "ignored" : "");
#endif


			if (rtp_session->dtmf_data.in_digit_ts && rtp_session->dtmf_data.in_digit_ts != ts) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "TS changed from last packet, resetting....\n");
				rtp_session->dtmf_data.last_digit = 0;
				rtp_session->dtmf_data.in_digit_ts = 0;
				rtp_session->dtmf_data.in_digit_sanity = 0;
				rtp_session->dtmf_data.in_digit_queued = 0;
			}


			if (!rtp_session->dtmf_data.in_digit_queued && rtp_session->dtmf_data.in_digit_ts) {
				if ((rtp_session->rtp_bugs & RTP_BUG_IGNORE_DTMF_DURATION)) {
					switch_dtmf_t dtmf = { key, switch_core_min_dtmf_duration(0), 0, SWITCH_DTMF_RTP };
#ifdef DEBUG_2833
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Early Queuing digit %c:%d\n", dtmf.digit, dtmf.duration / 8);
#endif
					switch_rtp_queue_rfc2833_in(rtp_session, &dtmf);
					rtp_session->dtmf_data.in_digit_queued = 1;
				}

				if (rtp_session->jb && (rtp_session->rtp_bugs & RTP_BUG_FLUSH_JB_ON_DTMF)) {
					switch_jb_reset(rtp_session->jb);
				}

			}

			/* only set sanity if we do NOT ignore the packet */
			if (rtp_session->dtmf_data.in_digit_ts) {
				rtp_session->dtmf_data.in_digit_sanity = 2000;
			}

			if (rtp_session->dtmf_data.last_duration > duration &&
				rtp_session->dtmf_data.last_duration > 0xFC17 && ts == rtp_session->dtmf_data.in_digit_ts) {
				rtp_session->dtmf_data.flip++;
			}

			if (end) {
				if (!rtp_session->dtmf_data.in_digit_ts && rtp_session->dtmf_data.last_in_digit_ts != ts) {
#ifdef DEBUG_2833
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "start with end packet %d\n", ts);
#endif
					rtp_session->dtmf_data.last_in_digit_ts = ts;
					rtp_session->dtmf_data.in_digit_ts = ts;
					rtp_session->dtmf_data.first_digit = key;
					rtp_session->dtmf_data.in_digit_sanity = 2000;
				}
				if (rtp_session->dtmf_data.in_digit_ts) {
					switch_dtmf_t dtmf = { key, duration, 0, SWITCH_DTMF_RTP };

					if (ts > rtp_session->dtmf_data.in_digit_ts) {
						dtmf.duration += (ts - rtp_session->dtmf_data.in_digit_ts);
					}
					if (rtp_session->dtmf_data.flip) {
						dtmf.duration += rtp_session->dtmf_data.flip * 0xFFFF;
						rtp_session->dtmf_data.flip = 0;
#ifdef DEBUG_2833
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "you're welcome!\n");
#endif
					}
#ifdef DEBUG_2833
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "done digit=%c ts=%u start_ts=%u dur=%u ddur=%u\n",
						   dtmf.digit, ts, rtp_session->dtmf_data.in_digit_ts, duration, dtmf.duration);
#endif

					if (!(rtp_session->rtp_bugs & RTP_BUG_IGNORE_DTMF_DURATION) && !rtp_session->dtmf_data.in_digit_queued) {
#ifdef DEBUG_2833
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Queuing digit %c:%d\n", dtmf.digit, dtmf.duration / 8);
#endif
						switch_rtp_queue_rfc2833_in(rtp_session, &dtmf);
					}

					rtp_session->dtmf_data.last_digit = rtp_session->dtmf_data.first_digit;

					rtp_session->dtmf_data.in_digit_ts = 0;
					rtp_session->dtmf_data.in_digit_sanity = 0;
					rtp_session->dtmf_data.in_digit_queued = 0;
					*do_cng = 1;
				} else {
					if (!switch_rtp_ready(rtp_session)) {
						return RESULT_GOTO_END;
					}
					switch_cond_next();
					return RESULT_GOTO_RECVFROM;
				}

			} else if (!rtp_session->dtmf_data.in_digit_ts) {
#ifdef DEBUG_2833
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "start %d [%c]\n", ts, key);
#endif
				rtp_session->dtmf_data.in_digit_ts = ts;
				rtp_session->dtmf_data.last_in_digit_ts = ts;
				rtp_session->dtmf_data.first_digit = key;
				rtp_session->dtmf_data.in_digit_sanity = 2000;
			}

			rtp_session->dtmf_data.last_duration = duration;
		} else {
#ifdef DEBUG_2833
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "drop: %c %u %u %u %u %d %d\n",
				   key, in_digit_seq, rtp_session->dtmf_data.in_digit_seq, ts, duration, rtp_session->last_rtp_hdr.m, end);
#endif
			switch_cond_next();
			return RESULT_GOTO_RECVFROM;
		}
	}

	if (bytes && rtp_session->dtmf_data.in_digit_ts) {
		if (!switch_rtp_ready(rtp_session)) {
			return RESULT_GOTO_END;
		}

		if (!rtp_session->dtmf_data.in_interleaved && rtp_session->last_rtp_hdr.pt != rtp_session->recv_te) {
			/* Drat, they are sending audio still as well as DTMF ok fine..... *sigh* */
			rtp_session->dtmf_data.in_interleaved = 1;
		}

		if (rtp_session->dtmf_data.in_interleaved || (rtp_session->rtp_bugs & RTP_BUG_IGNORE_DTMF_DURATION)) {
			if (rtp_session->last_rtp_hdr.pt == rtp_session->recv_te) {
				return RESULT_GOTO_RECVFROM;
			}
		} else {
			*do_cng = 1;
			return RESULT_GOTO_TIMERCHECK;
		}
	}

	return RESULT_CONTINUE;
}

static int rtp_write_ready(switch_rtp_t *rtp_session, uint32_t bytes, int line);
static int global_init = 0;
static int rtp_common_write(switch_rtp_t *rtp_session,
							rtp_msg_t *send_msg, void *data, uint32_t datalen, switch_payload_t payload, uint32_t timestamp, switch_frame_flag_t *flags);


static switch_status_t ice_out(switch_rtp_t *rtp_session, switch_rtp_ice_t *ice)
{
	uint8_t buf[256] = { 0 };
	switch_stun_packet_t *packet;
	unsigned int elapsed;
	switch_size_t bytes;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	//switch_sockaddr_t *remote_addr = rtp_session->remote_addr;
	switch_socket_t *sock_output = rtp_session->sock_output;
	switch_time_t now = switch_micro_time_now();

	if (ice->type & ICE_LITE) {
		// no connectivity checks for ICE-Lite
		return SWITCH_STATUS_BREAK;
	}

	if (ice->next_run && ice->next_run > now) {
		return SWITCH_STATUS_BREAK;
	}

	ice->next_run = now + RTP_STUN_FREQ;

	if (ice == &rtp_session->rtcp_ice && rtp_session->rtcp_sock_output) {
		sock_output = rtp_session->rtcp_sock_output;
	}

	if (!sock_output) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(rtp_session != NULL);
	switch_assert(ice->ice_user != NULL);

	READ_INC(rtp_session);

	if (rtp_session->last_stun) {
		elapsed = (unsigned int) ((switch_micro_time_now() - rtp_session->last_stun) / 1000);

		if (elapsed > 30000) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "No %s stun for a long time!\n", rtp_type(rtp_session));
			rtp_session->last_stun = switch_micro_time_now();
			//status = SWITCH_STATUS_GENERR;
			//goto end;
		}
	}

	packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
	switch_stun_packet_attribute_add_username(packet, ice->ice_user, (uint16_t)strlen(ice->ice_user));

	memcpy(ice->last_sent_id, packet->header.id, 12);

	//if (ice->pass && ice->type == ICE_GOOGLE_JINGLE) {
	//	switch_stun_packet_attribute_add_password(packet, ice->pass, (uint16_t)strlen(ice->pass));
	//}

	if ((ice->type & ICE_VANILLA)) {
		char sw[128] = "";

		switch_stun_packet_attribute_add_priority(packet, ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].priority);

		switch_snprintf(sw, sizeof(sw), "FreeSWITCH (%s)", switch_version_revision_human());
		switch_stun_packet_attribute_add_software(packet, sw, (uint16_t)strlen(sw));

		if ((ice->type & ICE_CONTROLLED)) {
			switch_stun_packet_attribute_add_controlled(packet);
		} else {
			switch_stun_packet_attribute_add_controlling(packet);
			switch_stun_packet_attribute_add_use_candidate(packet);
		}

		switch_stun_packet_attribute_add_integrity(packet, ice->rpass);
		switch_stun_packet_attribute_add_fingerprint(packet);
	}


	bytes = switch_stun_packet_length(packet);

#ifdef DEBUG_EXTRA
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_CRIT, "%s send %s stun\n", rtp_session_name(rtp_session), rtp_type(rtp_session));
#endif
	switch_socket_sendto(sock_output, ice->addr, 0, (void *) packet, &bytes);

	ice->sending = 3;

	// end:
	READ_DEC(rtp_session);

	return status;
}

int icecmp(const char *them, switch_rtp_ice_t *ice)
{
	if (strchr(them, ':')) {
		return strcmp(them, ice->user_ice);
	}

	return strcmp(them, ice->luser_ice);
}

static void handle_ice(switch_rtp_t *rtp_session, switch_rtp_ice_t *ice, void *data, switch_size_t len)
{
	switch_stun_packet_t *packet;
	switch_stun_packet_attribute_t *attr;
	void *end_buf;
	char username[34] = { 0 };
	unsigned char buf[512] = { 0 };
	switch_size_t cpylen = len;
	int xlen = 0;
	int ok = 1;
	uint32_t *pri = NULL;
	int is_rtcp = ice == &rtp_session->rtcp_ice;
	uint32_t elapsed;
	switch_time_t ref_point;

	//if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
	//	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "WTF OK %s CALL\n", rtp_type(rtp_session));
	//}

	if (!switch_rtp_ready(rtp_session) || zstr(ice->user_ice) || zstr(ice->ice_user)) {
		return;
	}

	READ_INC(rtp_session);
	WRITE_INC(rtp_session);

	switch_mutex_lock(rtp_session->ice_mutex);

	if (!switch_rtp_ready(rtp_session)) {
		goto end;
	}

	if (cpylen > sizeof(buf)) {
		cpylen = sizeof(buf);
	}


	memcpy(buf, data, cpylen);
	packet = switch_stun_packet_parse(buf, (uint32_t)cpylen);
	if (!packet) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Invalid STUN/ICE packet received %ld bytes\n", (long)cpylen);
		goto end;

	}

	rtp_session->last_stun = switch_micro_time_now();

	if (!rtp_session->first_stun) {
		rtp_session->first_stun = rtp_session->last_stun;
	}

	if (ice->last_ok) {
		ref_point = ice->last_ok;
	} else {
		ref_point = rtp_session->first_stun;
	}

	elapsed = (unsigned int) ((switch_micro_time_now() - ref_point) / 1000);


	end_buf = buf + ((sizeof(buf) > packet->header.length) ? packet->header.length : sizeof(buf));

	switch_stun_packet_first_attribute(packet, attr);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG8, "%s STUN PACKET TYPE: %s\n",
					  rtp_type(rtp_session), switch_stun_value_to_name(SWITCH_STUN_TYPE_PACKET_TYPE, packet->header.type));
	do {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG8, "|---: %s STUN ATTR %d %x %s\n", rtp_type(rtp_session), attr->type, attr->type,
						  switch_stun_value_to_name(SWITCH_STUN_TYPE_ATTRIBUTE, attr->type));

		switch (attr->type) {
		case SWITCH_STUN_ATTR_USE_CAND:
			{
				ice->rready = 1;
			}
			break;
		case SWITCH_STUN_ATTR_ERROR_CODE:
			{
				switch_stun_error_code_t *err = (switch_stun_error_code_t *) attr->value;
				uint32_t code = (err->code * 100) + err->number;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "%s got %s stun binding response %u\n",
								  rtp_session_name(rtp_session),
								  rtp_type(rtp_session),
								  code
								  );

				if ((ice->type & ICE_VANILLA) && code == 487) {
					if ((ice->type & ICE_CONTROLLED)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "%s STUN Changing role to CONTROLLING\n", rtp_type(rtp_session));
						ice->type &= ~ICE_CONTROLLED;
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "%s STUN Changing role to CONTROLLED\n", rtp_type(rtp_session));
						ice->type |= ICE_CONTROLLED;
					}
					packet->header.type = SWITCH_STUN_BINDING_RESPONSE;
				}

			}
			break;
		case SWITCH_STUN_ATTR_MAPPED_ADDRESS:
			if (attr->type) {
				char ip[50];
				uint16_t port;
				switch_stun_packet_attribute_get_mapped_address(attr, ip, sizeof(ip), &port);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG8, "|------: %s:%d\n", ip, port);
			}
			break;
		case SWITCH_STUN_ATTR_XOR_MAPPED_ADDRESS:
			if (attr->type) {
				char ip[50];
				uint16_t port;
				switch_stun_packet_attribute_get_xor_mapped_address(attr, &packet->header, ip, sizeof(ip), &port);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG8, "|------: %s:%d\n", ip, port);
			}
			break;
		case SWITCH_STUN_ATTR_USERNAME:
			if (attr->type) {
				switch_stun_packet_attribute_get_username(attr, username, sizeof(username));
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG8, "|------: %s\n", username);
			}
			break;

		case SWITCH_STUN_ATTR_PRIORITY:
			{
				uint32_t priority = 0;
				pri = (uint32_t *) attr->value;
				priority = ntohl(*pri);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG8, "|------: %u\n", priority);
				ok = priority == ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].priority;
			}
			break;
		}

		if (!switch_stun_packet_next_attribute(attr, end_buf)) {
			break;
		}

		xlen += 4 + switch_stun_attribute_padded_length(attr);
	} while (xlen <= packet->header.length);

	if ((ice->type & ICE_GOOGLE_JINGLE) && ok) {
		ok = !strcmp(ice->user_ice, username);
	}

	if (packet->header.type != SWITCH_STUN_BINDING_REQUEST && packet->header.type != SWITCH_STUN_BINDING_RESPONSE) {
		goto end;
	}

	if ((ice->type & ICE_VANILLA)) {
		if (!ok) ok = !memcmp(packet->header.id, ice->last_sent_id, 12);

		if (packet->header.type == SWITCH_STUN_BINDING_RESPONSE) {
			ok = 1;
			if (!ice->rready) {
				if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
					rtp_session->ice.rready = 1;
					rtp_session->rtcp_ice.rready = 1;
				} else {
					ice->rready = 1;
				}

				if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
					switch_core_session_video_reinit(rtp_session->session);
				}
			}
		}

		if (!ok && ice == &rtp_session->ice && rtp_session->rtcp_ice.ice_params && pri &&
			*pri == rtp_session->rtcp_ice.ice_params->cands[rtp_session->rtcp_ice.ice_params->chosen[1]][1].priority) {
			ice = &rtp_session->rtcp_ice;
			ok = 1;
		}

		if (!zstr(username)) {
			if (!icecmp(username, ice)) {
				ok = 1;
			} else if(!zstr(rtp_session->rtcp_ice.user_ice) && !icecmp(username, &rtp_session->rtcp_ice)) {
				ice = &rtp_session->rtcp_ice;
				ok = 1;
			}
		}

		if (ok) {
			ice->missed_count = 0;
		} else {
			switch_rtp_ice_t *icep[2] = { &rtp_session->ice, &rtp_session->rtcp_ice };
			switch_port_t port = 0;
			char *host = NULL;

			if (elapsed > 20000 && pri) {
				int i, j;
				uint32_t old;
				//const char *tx_host;
				const char *old_host, *err = NULL;
				//char bufa[50];
				char bufb[50];
				char adj_port[6];
				switch_channel_t *channel = NULL;


				ice->missed_count++;
				//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "missed %d\n", ice->missed_count);


				if (rtp_session->session) {
					channel = switch_core_session_get_channel(rtp_session->session);
				}

				//ice->ice_params->cands[ice->ice_params->chosen][ice->proto].priority;
				for (j = 0; j < 2; j++) {
					if (!icep[j] || !icep[j]->ice_params) {
						continue;
					}
					for (i = 0; i < icep[j]->ice_params->cand_idx[icep[j]->proto]; i++) {
						if (icep[j]->ice_params &&  icep[j]->ice_params->cands[i][icep[j]->proto].priority == *pri) {
							if (j == IPR_RTP) {
								icep[j]->ice_params->chosen[j] = i;
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO, "Change candidate index to %d\n", i);
							}

							ice = icep[j];
							ok = 1;

							if (j != IPR_RTP) {
								break;
							}

							old = rtp_session->remote_port;

							//tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->from_addr);
							old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);

							host = ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].con_addr;
							port = ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].con_port;

							if (!host || !port) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error setting remote host!\n");
								return;
							}

							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO,
											  "%s ICE Auto Changing port from %s:%u to %s:%u\n", rtp_type(rtp_session), old_host, old, host, port);


							if (channel) {
								switch_channel_set_variable(channel, "remote_media_ip_reported", switch_channel_get_variable(channel, "remote_media_ip"));
								switch_channel_set_variable(channel, "remote_media_ip", host);
								switch_channel_set_variable(channel, "rtp_auto_adjust_ip", host);
								switch_snprintf(adj_port, sizeof(adj_port), "%u", port);
								switch_channel_set_variable(channel, "remote_media_port_reported", switch_channel_get_variable(channel, "remote_media_port"));
								switch_channel_set_variable(channel, "remote_media_port", adj_port);
								switch_channel_set_variable(channel, "rtp_auto_adjust_port", adj_port);
								switch_channel_set_variable(channel, "rtp_auto_candidate_adjust", "true");
							}
							rtp_session->auto_adj_used = 1;


							switch_rtp_set_remote_address(rtp_session, host, port, 0, SWITCH_FALSE, &err);
							if (switch_sockaddr_info_get(&ice->addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS ||
								!ice->addr) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error setting remote host!\n");
								return;
							}

							if ((rtp_session->rtp_bugs & RTP_BUG_ALWAYS_AUTO_ADJUST)) {
								switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
							} else {
								switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
							}

						}
					}
				}
			}
		}
	}

	if (ice->missed_count > 5 && !(ice->type & ICE_GOOGLE_JINGLE)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "missed too many: %d, looking for new ICE dest.\n",
						  ice->missed_count);
		ice->rready = 0;
		ok = 1;
	}


	//if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] || 1) {
	//	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "WTF OK %s %d\n", rtp_type(rtp_session), ok);
	//}

	if (ok) {
		const char *host = NULL, *host2 = NULL;
		switch_port_t port = 0, port2 = 0;
		char buf[80] = "";
		char buf2[80] = "";

		if (packet->header.type == SWITCH_STUN_BINDING_REQUEST) {
			uint8_t stunbuf[512];
			switch_stun_packet_t *rpacket;
			const char *remote_ip;
			switch_size_t bytes;
			char ipbuf[50];
			switch_sockaddr_t *from_addr = rtp_session->from_addr;
			switch_socket_t *sock_output = rtp_session->sock_output;
			uint8_t do_adj = 0;
			switch_time_t now = switch_micro_time_now();
			int cmp = 0;
			int cur_idx = -1;//, is_relay = 0;
			int i;
			
			if (is_rtcp) {
				from_addr = rtp_session->rtcp_from_addr;
				sock_output = rtp_session->rtcp_sock_output;
			}

			if (!ice->ready) {
				ice->ready = 1;
			}

			memset(stunbuf, 0, sizeof(stunbuf));
			rpacket = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, packet->header.id, stunbuf);

			if ((ice->type & ICE_GOOGLE_JINGLE)) {
				switch_stun_packet_attribute_add_username(rpacket, username, (uint16_t)strlen(username));
			}

			remote_ip = switch_get_addr(ipbuf, sizeof(ipbuf), from_addr);

			switch_stun_packet_attribute_add_xor_binded_address(rpacket, (char *) remote_ip, switch_sockaddr_get_port(from_addr), from_addr->family);

			if ((ice->type & ICE_VANILLA)) {
				switch_stun_packet_attribute_add_integrity(rpacket, ice->pass);
				switch_stun_packet_attribute_add_fingerprint(rpacket);
			}

			bytes = switch_stun_packet_length(rpacket);

			host = switch_get_addr(buf, sizeof(buf), from_addr);
			port = switch_sockaddr_get_port(from_addr);
			host2 = switch_get_addr(buf2, sizeof(buf2), ice->addr);
			port2 = switch_sockaddr_get_port(ice->addr);
			cmp = switch_cmp_addr(from_addr, ice->addr);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG4,
							  "STUN from %s:%d %s\n", host, port, cmp ? "EXPECTED" : "IGNORED");

			if (cmp) {
				ice->last_ok = now;
				rtp_session->wrong_addrs = 0;
			} else {
				if (((rtp_session->dtls && rtp_session->dtls->state != DS_READY) || !ice->ready || !ice->rready) &&
					rtp_session->wrong_addrs > 2 && rtp_session->ice_adj == 0) {
					do_adj++;
					rtp_session->ice_adj = 1;
					rtp_session->wrong_addrs = 0;
				} else if (rtp_session->wrong_addrs > 10 || elapsed >= 10000) {
					do_adj++;
				}

				if (!do_adj) {
					rtp_session->wrong_addrs++;
				}

				for (i = 0; i < ice->ice_params->cand_idx[ice->proto]; i++) {
					if (!strcmp(ice->ice_params->cands[i][ice->proto].con_addr, host)) {
						cur_idx = i;
						//if (!strcasecmp(ice->ice_params->cands[i][ice->proto].cand_type, "relay")) {
						//	is_relay = 1;
						//}
					}
				}
				
				
				if (ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].cand_type &&
					!strcasecmp(ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].cand_type, "relay")) {
					do_adj++;
				}
			}
			
			if ((ice->type & ICE_VANILLA) && ice->ice_params && do_adj) {
				ice->missed_count = 0;
				ice->rready = 1;

				if (cur_idx > -1) {
					ice->ice_params->chosen[ice->proto] = cur_idx;
				}
				
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_NOTICE,
								  "Auto Changing %s stun/%s/dtls port from %s:%u to %s:%u idx:%d\n", rtp_type(rtp_session), is_rtcp ? "rtcp" : "rtp",
								  host2, port2,
								  host, port, cur_idx);

				switch_rtp_change_ice_dest(rtp_session, ice, host, port);
				ice->last_ok = now;
				rtp_session->wrong_addrs = 0;
			}
			//if (cmp) {
			switch_socket_sendto(sock_output, from_addr, 0, (void *) rpacket, &bytes);
			//}
		}
	} else if (packet->header.type == SWITCH_STUN_BINDING_ERROR_RESPONSE) {

		if (rtp_session->session) {
			switch_core_session_message_t msg = { 0 };
			msg.from = __FILE__;
			msg.numeric_arg = packet->header.type;
			msg.pointer_arg = packet;
			msg.message_id = SWITCH_MESSAGE_INDICATE_STUN_ERROR;
			switch_core_session_receive_message(rtp_session->session, &msg);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG,
							  "STUN/ICE binding error received on %s channel\n", rtp_type(rtp_session));
		}

	}




 end:
	switch_mutex_unlock(rtp_session->ice_mutex);
	WRITE_DEC(rtp_session);
	READ_DEC(rtp_session);
}

#ifdef ENABLE_ZRTP
SWITCH_STANDARD_SCHED_FUNC(zrtp_cache_save_callback)
{
	zrtp_status_t status = zrtp_status_ok;

	status = zrtp_def_cache_store(zrtp_global);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Saving ZRTP cache: %s\n", zrtp_status_ok == status ? "OK" : "FAIL");
	task->runtime = switch_epoch_time_now(NULL) + 900;
}

static int zrtp_send_rtp_callback(const zrtp_stream_t *stream, char *rtp_packet, unsigned int rtp_packet_length)
{
	switch_rtp_t *rtp_session = zrtp_stream_get_userdata(stream);
	switch_size_t len = rtp_packet_length;
	zrtp_status_t status = zrtp_status_ok;

	if (!rtp_session->sock_output) {
		return status;
	}

	switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, rtp_packet, &len);
	return status;
}

static void zrtp_event_callback(zrtp_stream_t *stream, unsigned event)
{
	switch_rtp_t *rtp_session = zrtp_stream_get_userdata(stream);
	zrtp_session_info_t zrtp_session_info;

	switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
	switch_event_t *fsevent = NULL;
	const char *type;

	type = rtp_type(rtp_session);

	switch (event) {
	case ZRTP_EVENT_IS_SECURE:
		{
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_SEND] = 1;
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_RECV] = 1;
			if (!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
				rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND] = 1;
				rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV] = 1;
			}
			if (zrtp_status_ok == zrtp_session_get(stream->session, &zrtp_session_info)) {
				if (zrtp_session_info.sas_is_ready) {

					switch_channel_set_variable_name_printf(channel, "true", "zrtp_secure_media_confirmed_%s", type);
					switch_channel_set_variable_name_printf(channel, stream->session->sas1.buffer, "zrtp_sas1_string_%s", type);
					switch_channel_set_variable_name_printf(channel, stream->session->sas2.buffer, "zrtp_sas2_string", type);
					zrtp_verified_set(zrtp_global, &stream->session->zid, &stream->session->peer_zid, (uint8_t)1);
				}
			}

			if (!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {


				if (rtp_session->session) {
					switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
					switch_rtp_t *video_rtp_session = switch_channel_get_private(channel, "__zrtp_video_rtp_session");

					if (!video_rtp_session) {
						video_rtp_session = switch_channel_get_private_partner(channel, "__zrtp_video_rtp_session");
					}

					if (video_rtp_session) {
						if (zrtp_status_ok != zrtp_stream_attach(stream->session, &video_rtp_session->zrtp_stream)) {
							abort();
						}
						zrtp_stream_set_userdata(video_rtp_session->zrtp_stream, video_rtp_session);
						if (switch_true(switch_channel_get_variable(channel, "zrtp_enrollment"))) {
							zrtp_stream_registration_start(video_rtp_session->zrtp_stream, video_rtp_session->ssrc);
						} else {
							zrtp_stream_start(video_rtp_session->zrtp_stream, video_rtp_session->ssrc);
						}
					}
				}
			}

			if (switch_event_create(&fsevent, SWITCH_EVENT_CALL_SECURE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header(fsevent, SWITCH_STACK_BOTTOM, "secure_media_type", "%s", type);
				switch_event_add_header(fsevent, SWITCH_STACK_BOTTOM, "secure_type", "zrtp:%s:%s", stream->session->sas1.buffer,
										stream->session->sas2.buffer);
				switch_event_add_header_string(fsevent, SWITCH_STACK_BOTTOM, "caller-unique-id", switch_channel_get_uuid(channel));
				switch_event_fire(&fsevent);
			}
		}
		break;
#if 0
	case ZRTP_EVENT_NO_ZRTP_QUICK:
		{
			if (stream != NULL) {
				zrtp_stream_stop(stream);
			}
		}
		break;
#endif
	case ZRTP_EVENT_IS_CLIENT_ENROLLMENT:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Enrolled complete!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_enroll_complete_%s", type);
		}
		break;

	case ZRTP_EVENT_USER_ALREADY_ENROLLED:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "User already enrolled!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_already_enrolled_%s", type);
		}
		break;

	case ZRTP_EVENT_NEW_USER_ENROLLED:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "New user enrolled!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_new_user_enrolled_%s", type);
		}
		break;

	case ZRTP_EVENT_USER_UNENROLLED:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "User unenrolled!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_user_unenrolled_%s", type);
		}
		break;

	case ZRTP_EVENT_IS_PENDINGCLEAR:
		{
			switch_channel_set_variable_name_printf(channel, "false", "zrtp_secure_media_confirmed_%s", type);
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_SEND] = 0;
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_RECV] = 0;
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND] = 0;
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV] = 0;
			rtp_session->zrtp_mitm_tries = 0;
		}
		break;

	case ZRTP_EVENT_NO_ZRTP:
		{
			switch_channel_set_variable_name_printf(channel, "false", "zrtp_secure_media_confirmed_%s", type);
		}
		break;

	default:
		break;
	}
}

static void zrtp_logger(int level, const char *data, int len, int offset)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s", data);
}
#endif

SWITCH_DECLARE(void) switch_rtp_init(switch_memory_pool_t *pool)
{
#ifdef ENABLE_ZRTP
	const char *zid_string = switch_core_get_variable_pdup("switch_serial", pool);
	const char *zrtp_enabled = switch_core_get_variable_pdup("zrtp_enabled", pool);
	zrtp_config_t zrtp_config;
	char zrtp_cache_path[256] = "";
	zrtp_on = zrtp_enabled ? switch_true(zrtp_enabled) : 0;
#endif
	if (global_init) {
		return;
	}
	switch_core_hash_init(&alloc_hash);
#ifdef ENABLE_ZRTP
	if (zrtp_on) {
		uint32_t cache_len;
		zrtp_config_defaults(&zrtp_config);
		strcpy(zrtp_config.client_id, "FreeSWITCH");
		zrtp_config.is_mitm = 1;
		zrtp_config.lic_mode = ZRTP_LICENSE_MODE_ACTIVE;
		switch_snprintf(zrtp_cache_path, sizeof(zrtp_cache_path), "%s%szrtp.dat", SWITCH_GLOBAL_dirs.db_dir, SWITCH_PATH_SEPARATOR);
		cache_len=(uint32_t)strlen(zrtp_cache_path);
		ZSTR_SET_EMPTY(zrtp_config.def_cache_path);
		zrtp_config.def_cache_path.length = cache_len > zrtp_config.def_cache_path.max_length ? zrtp_config.def_cache_path.max_length : (uint16_t)cache_len;
		strncpy(zrtp_config.def_cache_path.buffer, zrtp_cache_path, zrtp_config.def_cache_path.max_length);
		zrtp_config.cb.event_cb.on_zrtp_protocol_event = (void (*)(zrtp_stream_t*,zrtp_protocol_event_t))zrtp_event_callback;
		zrtp_config.cb.misc_cb.on_send_packet = zrtp_send_rtp_callback;
		zrtp_config.cb.event_cb.on_zrtp_security_event = (void (*)(zrtp_stream_t*,zrtp_security_event_t))zrtp_event_callback;
		zrtp_log_set_log_engine((zrtp_log_engine *) zrtp_logger);
		zrtp_log_set_level(4);
		if (zrtp_status_ok == zrtp_init(&zrtp_config, &zrtp_global)) {
			memcpy(zid, zid_string, 12);
			switch_scheduler_add_task(switch_epoch_time_now(NULL) + 900, zrtp_cache_save_callback, "zrtp_cache_save", "core", 0, NULL,
									  SSHF_NONE | SSHF_NO_DEL);
		} else {
			switch_core_set_variable("zrtp_enabled", NULL);
			zrtp_on = 0;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "ZRTP init failed!\n");
		}
	}
#endif
#ifdef ENABLE_SRTP
	srtp_init();
#endif
	switch_mutex_init(&port_lock, SWITCH_MUTEX_NESTED, pool);
	global_init = 1;
}

static uint8_t get_next_write_ts(switch_rtp_t *rtp_session, uint32_t timestamp)
{
	uint8_t m = 0, changed = 0;

	if (!(rtp_session->rtp_bugs & RTP_BUG_SEND_LINEAR_TIMESTAMPS)) {
		if (timestamp) {
			rtp_session->ts = (uint32_t) timestamp;
			changed++;
		} else if (switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
			switch_core_timer_next(&rtp_session->write_timer);
			rtp_session->ts = rtp_session->write_timer.samplecount;
			changed++;
		}
	}

	if (!changed) {
		rtp_session->ts = rtp_session->last_write_ts + rtp_session->samples_per_interval;
	} else {
		/* Send marker bit if timestamp is lower/same as before (resetted/new timer) */
		if (abs((int32_t)(rtp_session->ts - rtp_session->last_write_ts)) > rtp_session->samples_per_interval 
			&& !(rtp_session->rtp_bugs & RTP_BUG_NEVER_SEND_MARKER)) {
			m++;
		}
	}

	return m;
}

static void do_mos(switch_rtp_t *rtp_session, int force) {

	if ((switch_size_t)rtp_session->stats.inbound.recved < rtp_session->stats.inbound.flaws) {
		rtp_session->stats.inbound.flaws = 0;
	}

	if (rtp_session->stats.inbound.recved > 0 &&
		rtp_session->stats.inbound.flaws && (force || rtp_session->stats.inbound.last_flaw != rtp_session->stats.inbound.flaws)) {
		int R;

		if (rtp_session->consecutive_flaws++) {
			int diff, penalty;

			diff = (rtp_session->stats.inbound.flaws - rtp_session->stats.inbound.last_flaw);

			if (diff < 1) diff = 1;

			penalty = diff * 2;

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "%s %s %d consecutive flaws, adding %d flaw penalty\n",
							  rtp_session_name(rtp_session), rtp_type(rtp_session),
							  rtp_session->consecutive_flaws, penalty);
			rtp_session->bad_stream++;
			rtp_session->stats.inbound.flaws += penalty;

			if (rtp_session->stats.inbound.error_log) {
				rtp_session->stats.inbound.error_log->flaws += penalty;
				rtp_session->stats.inbound.error_log->consecutive_flaws++;
			}
		}

		R = (int)((double)((double)(rtp_session->stats.inbound.recved - rtp_session->stats.inbound.flaws) / (double)rtp_session->stats.inbound.recved) * 100.0);

		if (R < 0 || R > 100) R = 100;

		rtp_session->stats.inbound.R = R;
		rtp_session->stats.inbound.mos = 1 + (0.035) * R + (.000007) * R * (R-60) * (100-R);
		rtp_session->stats.inbound.last_flaw = rtp_session->stats.inbound.flaws;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "%s %s stat %0.2f %ld/%d flaws: %ld mos: %0.2f v: %0.2f %0.2f/%0.2f\n",
						  rtp_session_name(rtp_session),
						  rtp_type(rtp_session),
						  rtp_session->stats.inbound.R,
						  (long int)(rtp_session->stats.inbound.recved - rtp_session->stats.inbound.flaws), rtp_session->stats.inbound.recved,
						  (long int)rtp_session->stats.inbound.flaws,
						  rtp_session->stats.inbound.mos,
						  rtp_session->stats.inbound.variance,
						  rtp_session->stats.inbound.min_variance,
						  rtp_session->stats.inbound.max_variance
						  );
	} else {
		rtp_session->consecutive_flaws = 0;
	}
}

void burstr_calculate ( int loss[], int received, double *burstr, double *lossr )
{
	int lost = 0;
	int bursts = 0;
	int i;

	for ( i = 0; i < LOST_BURST_ANALYZE; i++ ) {
		lost += i * loss[i];
		bursts += loss[i];
	}
	if (received > 0 && bursts > 0) {
		*burstr = (double)((double)lost / (double)bursts) / (double)(1.0 / ( 1.0 - (double)lost / (double)received ));
		if (*burstr < 0) {
			*burstr = - *burstr;
		}
	} else {
		*burstr = 0;
	}
	if (received > 0) {
		*lossr = (double)((double)lost / (double)received);
	} else {
		*lossr = 0;
	}
}

static void reset_jitter_seq(switch_rtp_t *rtp_session)
{
	rtp_session->stats.inbound.last_proc_time = 0;
	rtp_session->stats.inbound.last_processed_seq = 0;
	rtp_session->jitter_lead = 0;
	rtp_session->consecutive_flaws = 0;
	rtp_session->stats.inbound.last_flaw = 0;
}

static void check_jitter(switch_rtp_t *rtp_session)
{
	switch_time_t current_time;
	int64_t diff_time = 0, cur_diff = 0;
	int seq;

	current_time = switch_micro_time_now() / 1000;

	if (rtp_session->flags[SWITCH_RTP_FLAG_PAUSE] || rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON] || rtp_session->dtmf_data.in_digit_ts) {
		reset_jitter_seq(rtp_session);
		return;
	}

	if (++rtp_session->jitter_lead < JITTER_LEAD_FRAMES || !rtp_session->stats.inbound.last_proc_time) {
		rtp_session->stats.inbound.last_proc_time = current_time;
		return;
	}

	diff_time = (current_time - rtp_session->stats.inbound.last_proc_time);
	seq = (int)(uint16_t) ntohs((uint16_t) rtp_session->last_rtp_hdr.seq);

	/* Burst and Packet Loss */
	rtp_session->stats.inbound.recved++;

	if (rtp_session->stats.inbound.last_processed_seq > 0 && seq > (int)(rtp_session->stats.inbound.last_processed_seq + 1)) {
		int lost = (seq - rtp_session->stats.inbound.last_processed_seq - 1);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s Got: %s seq %d but expected: %d lost: %d\n",
						  rtp_session_name(rtp_session),
						  rtp_type(rtp_session),
						  seq,
						  (rtp_session->stats.inbound.last_processed_seq + 1), lost);
		rtp_session->stats.inbound.last_loss++;

		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
			switch_core_session_request_video_refresh(rtp_session->session);
		}

		if (rtp_session->stats.inbound.last_loss > 0 && rtp_session->stats.inbound.last_loss < LOST_BURST_CAPTURE) {
			rtp_session->stats.inbound.loss[rtp_session->stats.inbound.last_loss] += lost;
		}

		rtp_session->bad_stream++;
		rtp_session->stats.inbound.flaws += lost;

		if (rtp_session->stats.inbound.error_log) {
			rtp_session->stats.inbound.error_log->flaws += lost;
		}

	} else {
		rtp_session->stats.inbound.last_loss = 0;
	}

	rtp_session->stats.inbound.last_processed_seq = seq;

	/* Burst and Packet Loss */

	if (current_time > rtp_session->next_stat_check_time) {
		rtp_session->next_stat_check_time = current_time + 5000;
		burstr_calculate(rtp_session->stats.inbound.loss, rtp_session->stats.inbound.recved,
						 &(rtp_session->stats.inbound.burstrate), &(rtp_session->stats.inbound.lossrate));
		do_mos(rtp_session, SWITCH_TRUE);
	} else {
		do_mos(rtp_session, SWITCH_FALSE);
	}

	if (rtp_session->stats.inbound.last_loss || rtp_session->bad_stream) {
		if (rtp_session->session && (!rtp_session->stats.inbound.error_log || rtp_session->stats.inbound.error_log->stop)) {
			struct error_period *error = switch_core_session_alloc(rtp_session->session, sizeof(*error));
			error->start = switch_micro_time_now();
			error->next = rtp_session->stats.inbound.error_log;
			rtp_session->stats.inbound.error_log = error;
		}

		if (!rtp_session->stats.inbound.last_loss) {
			if (++rtp_session->recovering_stream > (rtp_session->one_second * 3)) {
				if (rtp_session->session && rtp_session->stats.inbound.error_log) {
					rtp_session->stats.inbound.error_log->stop = switch_micro_time_now();
				}

				rtp_session->bad_stream = 0;
			}
		} else {
			rtp_session->recovering_stream = 0;
			rtp_session->bad_stream++;
		}
	} else {
		rtp_session->recovering_stream = 0;
		rtp_session->clean_stream++;
	}


	if ( diff_time < 0 ) {
		diff_time = -diff_time;
	}

	rtp_session->stats.inbound.jitter_n++;
	rtp_session->stats.inbound.jitter_add += diff_time;

	if (rtp_session->stats.inbound.mean_interval) {
		cur_diff = (int64_t)(diff_time - rtp_session->stats.inbound.mean_interval);
	} else {
		cur_diff = 0;
	}

	rtp_session->stats.inbound.jitter_addsq += (cur_diff * cur_diff);
	rtp_session->stats.inbound.last_proc_time = current_time;

	if (rtp_session->stats.inbound.jitter_n > 0) {
		double ipdv;

		rtp_session->stats.inbound.mean_interval = (double)rtp_session->stats.inbound.jitter_add / (double)rtp_session->stats.inbound.jitter_n;

		if (!rtp_session->old_mean) {
			rtp_session->old_mean = rtp_session->stats.inbound.mean_interval;
		}

		rtp_session->stats.inbound.variance = (double)rtp_session->stats.inbound.jitter_addsq / (double)rtp_session->stats.inbound.jitter_n;

		//printf("CHECK %d +%ld +%ld %f %f\n", rtp_session->write_timer.samplecount, diff_time, (diff_time * diff_time), rtp_session->stats.inbound.mean_interval, rtp_session->stats.inbound.variance);

		ipdv = rtp_session->old_mean - rtp_session->stats.inbound.mean_interval;

		if ( ipdv > IPDV_THRESHOLD ) { /* It shows Increasing Delays */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG3, "Calculated Instantaneous Packet Delay Variation: %s packet %lf\n",
							  rtp_type(rtp_session), ipdv);
		}

		if ( rtp_session->stats.inbound.variance < rtp_session->stats.inbound.min_variance || rtp_session->stats.inbound.min_variance == 0 ) {
			rtp_session->stats.inbound.min_variance = rtp_session->stats.inbound.variance;
		}

		if ( rtp_session->stats.inbound.variance > rtp_session->stats.inbound.max_variance ) {
			rtp_session->stats.inbound.max_variance = rtp_session->stats.inbound.variance;
		}

		rtp_session->old_mean = rtp_session->stats.inbound.mean_interval;
	}
}

static void rtcp_generate_sender_info(switch_rtp_t *rtp_session, struct switch_rtcp_sender_info *sr){
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
	switch_time_t now;
	uint32_t sec, ntp_sec, ntp_usec;
	switch_time_exp_t now_hr;
	now = switch_micro_time_now();
	sec = (uint32_t)(now/1000000);        /* convert to seconds     */
	ntp_sec = sec+NTP_TIME_OFFSET;  /* convert to NTP seconds */
	sr->ntp_msw = htonl(ntp_sec);   /* store result in "most significant word" */
	ntp_usec = (uint32_t)(now - (sec*1000000)); /* remove seconds to keep only the microseconds */
	sr->ntp_lsw = htonl((u_long)(ntp_usec*(double)(((uint64_t)1)<<32)*1.0e-6)); /* convert microseconds to fraction of 32bits and store result in "least significatn word" */

	sr->ts = htonl(rtp_session->last_write_ts);
	sr->pc = htonl(rtp_session->stats.outbound.packet_count);
	sr->oc = htonl((rtp_session->stats.outbound.raw_bytes - rtp_session->stats.outbound.packet_count * sizeof(srtp_hdr_t)));

	switch_time_exp_gmt(&now_hr,now);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10,"Sending an RTCP packet[%04d-%02d-%02d %02d:%02d:%02d.%d] lsr[%u] msw[%u] lsw[%u] stats_ssrc[%u]\n",
			1900 + now_hr.tm_year,  now_hr.tm_mday, now_hr.tm_mon, now_hr.tm_hour, now_hr.tm_min, now_hr.tm_sec, now_hr.tm_usec,
			(ntohl(sr->ntp_lsw)&0xffff0000)>>16 | (ntohl(sr->ntp_msw)&0x0000ffff)<<16,
			ntohl(sr->ntp_msw),ntohl(sr->ntp_lsw), rtp_session->stats.rtcp.ssrc
			);
}

static inline uint32_t calc_local_lsr_now() 
{
	switch_time_t now;
	uint32_t ntp_sec, ntp_usec, lsr_now, sec;
	now = switch_micro_time_now();
	sec = (uint32_t)(now/1000000);        /* convert to seconds     */
	ntp_sec = sec+NTP_TIME_OFFSET;  /* convert to NTP seconds */
	ntp_usec = (uint32_t)(now - ((switch_time_t) sec*1000000)); /* remove seconds to keep only the microseconds */

	lsr_now = (uint32_t)(ntp_usec*0.065536) | (ntp_sec&0x0000ffff)<<16; /* 0.065536 is used for convertion from useconds to fraction of 65536 (x65536/1000000) */

	return lsr_now;
}

//#define DEBUG_RTCP
/* extra param is for duplicates (received NACKed packets) */ 
static void rtcp_generate_report_block(switch_rtp_t *rtp_session, struct switch_rtcp_report_block *rtcp_report_block, 
		int16_t extra_expected)
{
#ifdef DEBUG_RTCP
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
#endif
	switch_rtcp_numbers_t * stats=&rtp_session->stats.rtcp;
	uint32_t expected_pkt, dlsr = 0;
	int32_t pkt_lost;

	/* Packet loss */
	if (stats->rtcp_rtp_count == 0) {
		expected_pkt = stats->high_ext_seq_recv - stats->base_seq + 1;
	} else {
		expected_pkt = stats->high_ext_seq_recv - stats->last_rpt_ext_seq + extra_expected;
	}

	pkt_lost = expected_pkt - stats->period_pkt_count;
	stats->cum_lost=stats->cum_lost+pkt_lost;
	if (expected_pkt > 0 && pkt_lost > 0) {
		rtcp_report_block->fraction = (uint8_t) (pkt_lost * 256 / expected_pkt);
	} else {
		rtcp_report_block->fraction = 0;
	}
#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
	rtcp_report_block->lost = stats->cum_lost;
#else
	/* Reversing byte order for 24bits */
	rtcp_report_block->lost = htonl(stats->cum_lost) >> 8;
#endif

#ifdef DEBUG_RTCP
			if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO])
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "rtcp_generate_sr: stats_ssrc[%u]\nreceived[%d]\nexpected[%d]\ncum[%d]\nlost[%d|%d/256]pkt\nlast_seq[%d]\ncyc[%d]\nlast_rpt_seq[%d]\ncyc[%d]\nssrc[%d]\n",
						rtp_session->remote_ssrc, stats->period_pkt_count, expected_pkt,
						stats->cum_lost, pkt_lost, rtcp_report_block->fraction, stats->high_ext_seq_recv&0x0000ffff,
						stats->cycle, stats->last_rpt_ext_seq&0x0000ffff, stats->last_rpt_cycle, rtp_session->stats.rtcp.peer_ssrc
						);
#endif
	rtcp_report_block->highest_sequence_number_received = htonl(stats->high_ext_seq_recv);

	/* Jitter */
	rtcp_report_block->jitter = htonl((uint32_t)stats->inter_jitter);

	/* Delay since Last Sender Report (DLSR) : 32bits, 1/65536 seconds */
	if (stats->last_recv_lsr_local) {
		uint32_t lsr_now = calc_local_lsr_now();
		/* check lsr_now: what we just read from clock may be in the past (race cond), don't send huge dlsr due to uint wrap around */
		if (lsr_now > stats->last_recv_lsr_local) {
			dlsr = lsr_now - stats->last_recv_lsr_local;
		}
	}
	rtcp_report_block->lsr = stats->last_recv_lsr_peer;
	rtcp_report_block->dlsr = htonl(dlsr);
	rtcp_report_block->ssrc = htonl(rtp_session->stats.rtcp.peer_ssrc);
	stats->rtcp_rtp_count++;
}

static void rtcp_stats_init(switch_rtp_t *rtp_session)
{
	switch_rtcp_numbers_t * stats = &rtp_session->stats.rtcp;
	srtp_hdr_t * hdr = &rtp_session->last_rtp_hdr;
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
	stats->ssrc = ntohl(hdr->ssrc);
	stats->last_rpt_ts = rtp_session->write_timer.samplecount;
	stats->init = 1;
	stats->last_rpt_ext_seq = 0;
	stats->last_rpt_cycle = 0;
	stats->last_pkt_tsdiff = 0;
	stats->inter_jitter = 0;
	stats->cycle = 0;
	stats->high_ext_seq_recv = ntohs((uint16_t)hdr->seq);
	stats->base_seq = ntohs((uint16_t)hdr->seq);
	stats->bad_seq = (1<<16) + 1; /* Make sure we wont missmatch 2 consecutive packets, so seq == bad_seq is false */
	stats->cum_lost = 0;
	stats->period_pkt_count = 0;
	stats->sent_pkt_count = 0;
	stats->pkt_count = 0;
	stats->rtcp_rtp_count = 0;

	if (!rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "rtcp_stats_init: %s rtcp disabled\n", rtp_type(rtp_session));
	} else if (!rtp_session->rtcp_sock_output) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "rtcp_stats_init: %s no rtcp socket\n", rtp_type(rtp_session));
	} else if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_PASSTHRU]) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "rtcp_stats_init: %s rtcp passthru\n", rtp_type(rtp_session));
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "rtcp_stats_init: %s ssrc[%u] base_seq[%u]\n", rtp_type(rtp_session), stats->ssrc, stats->base_seq);
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP] && (switch_core_media_codec_get_cap(rtp_session->session,
															SWITCH_MEDIA_TYPE_AUDIO, SWITCH_CODEC_FLAG_HAS_ADJ_BITRATE))) {
			kalman_estimator_t *estimators[KALMAN_SYSTEM_MODELS];
			cusum_kalman_detector_t *detectors[KALMAN_SYSTEM_MODELS];

			rtp_session->flags[SWITCH_RTP_FLAG_ADJ_BITRATE_CAP] = 1;
			rtp_session->flags[SWITCH_RTP_FLAG_ESTIMATORS] = 1;

			rtp_session->estimators[EST_LOSS] = switch_core_alloc(rtp_session->pool, sizeof(*estimators[0]));
			switch_kalman_init(rtp_session->estimators[EST_LOSS],0.1,0.1);
			rtp_session->estimators[EST_RTT] = switch_core_alloc(rtp_session->pool, sizeof(*estimators[0]));
			switch_kalman_init(rtp_session->estimators[EST_RTT],0.03,1);
			rtp_session->detectors[EST_RTT] = switch_core_alloc(rtp_session->pool, sizeof(*detectors[0]));
			switch_kalman_cusum_init(rtp_session->detectors[EST_RTT],0.005,0.5);
			rtp_session->detectors[EST_LOSS] = switch_core_alloc(rtp_session->pool, sizeof(*detectors[0]));
			switch_kalman_cusum_init(rtp_session->detectors[EST_LOSS],0.005,0.5);
	}
}

static int rtcp_stats(switch_rtp_t *rtp_session)
{
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
	srtp_hdr_t * hdr = &rtp_session->last_rtp_hdr;
	switch_rtcp_numbers_t * stats = &rtp_session->stats.rtcp;
	uint32_t packet_spacing_diff = 0, pkt_tsdiff, pkt_extended_seq;
	uint16_t pkt_seq, seq_diff, max_seq;
	const int MAX_DROPOUT = 3000;
	const int MAX_MISORDER = 100;
	const int RTP_SEQ_MOD = (1<<16);

	if(!rtp_session->rtcp_sock_output || !rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP] || rtp_session->flags[SWITCH_RTP_FLAG_RTCP_PASSTHRU] || !rtp_session->rtcp_interval)
		return 0; /* do not process RTCP in current state */

	pkt_seq = (uint16_t) ntohs((uint16_t) rtp_session->last_rtp_hdr.seq);

	/* Detect sequence number cycle change */
	max_seq = stats->high_ext_seq_recv&0x0000ffff;
	seq_diff = pkt_seq - max_seq;

	if (seq_diff < MAX_DROPOUT) {  /* in order, with permissible gap */
		if (pkt_seq < max_seq) {
			stats->cycle++;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "rtcp_stats:[cycle change] pkt_seq[%d] cycle[%d] max_seq[%d] stats_ssrc[%u] local_ts[%u]\n",
					pkt_seq, stats->cycle, max_seq, stats->ssrc, rtp_session->timer.samplecount);
		}
		pkt_extended_seq = stats->cycle << 16 | pkt_seq; /* getting the extended packet extended sequence ID */
		if (pkt_extended_seq > stats->high_ext_seq_recv) {
			stats->high_ext_seq_recv = pkt_extended_seq;
		}
	}
	else if (seq_diff <= (RTP_SEQ_MOD - MAX_MISORDER)) {   /* the sequence number made a very large jump */
		if (pkt_seq == stats->bad_seq) {
			rtcp_stats_init(rtp_session);
		} else {
			stats->bad_seq = (pkt_seq + 1) & (RTP_SEQ_MOD-1);
		}
		return 0; /* no stats, packet is out of sync and will be accounted as lost */
	} else {
		/* duplicate or reordered packet */
	}

	/* Verify that we are on the same stream source (we do not support multiple sources) */
	if (ntohl(hdr->ssrc) != stats->ssrc || !stats->init) {
		rtcp_stats_init(rtp_session);
	}

	stats->period_pkt_count++;
	stats->pkt_count++;
#ifdef DEBUG_RTCP
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "rtcp_stats: period_pkt_count[%d]last_seq[%d]cycle[%d]stats_ssrc[%u]local_ts[%u]\n",
			stats->period_pkt_count, pkt_seq, stats->cycle, stats->ssrc, rtp_session->write_timer.samplecount);
#endif
	/* Interarrival jitter calculation */
	pkt_tsdiff = abs((int32_t)(rtp_session->timer.samplecount - ntohl(hdr->ts)));  /* relative transit times for this packet */
	if (stats->pkt_count < 2) { /* Can not compute Jitter with only one packet */
		stats->last_pkt_tsdiff = pkt_tsdiff;
	} else {
		/* Jitter : difference of relative transit times for the two packets */
		packet_spacing_diff = abs((int32_t)(pkt_tsdiff - stats->last_pkt_tsdiff));
		stats->last_pkt_tsdiff = pkt_tsdiff;
		/* Interarrival jitter estimation, "J(i) = J(i-1) + ( |D(i-1,i)| - J(i-1) )/16" */
		stats->inter_jitter = (stats->inter_jitter + (((double)packet_spacing_diff - stats->inter_jitter) /16.));
	}

#ifdef DEBUG_RTCP
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "rtcp_stats: pkt_ts[%d]local_ts[%d]diff[%d]pkt_spacing[%d]inter_jitter[%f]seq[%d]stats_ssrc[%d]",
			ntohl(hdr->ts), rtp_session->timer.samplecount, pkt_tsdiff, packet_spacing_diff, stats->inter_jitter, ntohs(hdr->seq), stats->ssrc);
#endif
	return 1;
}

static void calc_bw_exp(uint32_t bps, uint8_t bits, rtcp_tmmbx_t *tmmbx)
{
	uint32_t mantissa_max, i = 0;
	uint8_t exp = 0;
	uint32_t mantissa = 0;
	uint16_t overhead = 60;

	switch_assert(bits<=32);

	mantissa_max = (1 << bits) - 1;

	for (i = 0; i < 32; ++i) {
		if (bps <= (mantissa_max << i)) {
			exp = i;
			break;
		}
	}

	mantissa = (bps >> exp);

	tmmbx->parts[0] = (uint8_t) ((exp << 2) + ((mantissa >> 15) & 0x03));
	tmmbx->parts[1] = (uint8_t) (mantissa >> 7);
	tmmbx->parts[2] = (uint8_t) ((mantissa >> 1) + ((overhead >> 8) & 0x01));
	tmmbx->parts[3] = (uint8_t) (overhead);
}

static int using_ice(switch_rtp_t *rtp_session)
{
	if (rtp_session->ice.ice_user || rtp_session->rtcp_ice.ice_user) {
		return 1;
	}

	return 0;
}

#define MAX_NACK 10
static int check_rtcp_and_ice(switch_rtp_t *rtp_session)
{
	int ret = 0;
	int rtcp_ok = 0, rtcp_cyclic = 0, rtcp_fb = 0, force_send_rr = 0;
	switch_time_t now = switch_micro_time_now();
	int rate = 0, nack_ttl = 0, nack_dup = 0; 
	uint32_t cur_nack[MAX_NACK] = { 0 };

	if (!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] &&
		rtp_session->flags[SWITCH_RTP_FLAG_AUTO_CNG] &&
		rtp_session->send_msg.header.ts &&
		rtp_session->cng_pt != INVALID_PT &&
		(rtp_session->write_timer.samplecount - rtp_session->last_write_samplecount >= rtp_session->samples_per_interval * 60)) {
		uint8_t data[10] = { 0 };
		switch_frame_flag_t frame_flags = SFF_NONE;
		data[0] = 65;
		rtp_session->cn++;

		get_next_write_ts(rtp_session, 0);
		rtp_session->send_msg.header.ts = htonl(rtp_session->ts);

		switch_rtp_write_manual(rtp_session, (void *) data, 2, 0, rtp_session->cng_pt, ntohl(rtp_session->send_msg.header.ts), &frame_flags);

		if (switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
			rtp_session->last_write_samplecount = rtp_session->write_timer.samplecount;
		}
	}

	rate = rtp_session->rtcp_interval;

	if (rtp_session->flags[SWITCH_RTP_FLAG_NACK] && rtp_session->vb) {
		int n;
		for (n = 0; n < MAX_NACK; n++) {
			uint32_t nack = switch_jb_pop_nack(rtp_session->vb);

			if (!nack) break;

			cur_nack[nack_ttl++] = nack;
		}
	}



	if (rtp_session->rtcp_sent_packets < 4) {
		rate = 4000;
	} else  {
		if (rtp_session->pli_count || rtp_session->fir_count || nack_ttl || rtp_session->tmmbr || rtp_session->tmmbn) {
			//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "MARK BW/FIR ETC %d %d\n", rtp_session->pli_count, rtp_session->fir_count);
			rtcp_ok = 1;
			rtcp_fb = 1;
		}
	}

	if (rtp_session->send_rr) {
		rtp_session->send_rr = 0;
		rtcp_ok = 1;
		force_send_rr = 1;
	}

	//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "TIME CHECK %d > %d\n", (int)((now - rtp_session->rtcp_last_sent) / 1000), rate);

	if (!rtcp_ok && (!rtp_session->rtcp_last_sent || (int)((now - rtp_session->rtcp_last_sent) / 1000) > rate)) {
		//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "TIME UP\n");
		rtcp_cyclic = 1;
		rtcp_ok = 1;
	}

	if (rtcp_ok && using_ice(rtp_session)) {
		if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
			if (!rtp_session->ice.rready) {
				rtcp_ok = 0;
			}
		} else {
			if (!rtp_session->rtcp_ice.rready) {
				rtcp_ok = 0;
			}
		}
	}

	//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "WTF %d %d %d %d\n", rate, rtp_session->rtcp_sent_packets, rtcp_ok, nack_ttl);

	if (rtp_session->rtcp_sock_output && rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP] && !rtp_session->flags[SWITCH_RTP_FLAG_RTCP_PASSTHRU] && rtcp_ok) {
		switch_rtcp_numbers_t * stats = &rtp_session->stats.rtcp;
		struct switch_rtcp_receiver_report *rr;
		struct switch_rtcp_sender_report *sr;
		struct switch_rtcp_report_block *rtcp_report_block = NULL;
		switch_size_t rtcp_bytes = sizeof(struct switch_rtcp_hdr_s)+sizeof(uint32_t); /* add size of the packet header and the ssrc */
		switch_rtcp_hdr_t *sdes;
		uint8_t *p;
		switch_size_t sdes_bytes = sizeof(struct switch_rtcp_hdr_s);
		uint32_t *ssrc;
		switch_rtcp_sdes_unit_t *unit;
		switch_bool_t is_only_receiver = FALSE;

		if (!rtcp_fb) {
			rtp_session->rtcp_last_sent = now;
			rtp_session->rtcp_sent_packets++;
		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && 
				rtp_session->vb && rtcp_cyclic) {
				nack_dup = rtp_session->prev_nacks_inflight;
				rtp_session->prev_nacks_inflight = 0;
		}

		rtp_session->rtcp_send_msg.header.version = 2;
		rtp_session->rtcp_send_msg.header.p = 0;

		if ((switch_core_session_media_flow(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO) == SWITCH_MEDIA_FLOW_RECVONLY) ||
				switch_core_session_media_flow(rtp_session->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY) {
			is_only_receiver = TRUE;
		}
		if (!rtp_session->stats.rtcp.sent_pkt_count || is_only_receiver || force_send_rr) {
			rtp_session->rtcp_send_msg.header.type = _RTCP_PT_RR; /* Receiver report */
			rr=(struct switch_rtcp_receiver_report*) rtp_session->rtcp_send_msg.body;
			rr->ssrc = htonl(rtp_session->ssrc);
			rtcp_report_block = &rr->report_block;
			rtcp_bytes += sizeof(struct switch_rtcp_report_block);
			rtcp_generate_report_block(rtp_session, rtcp_report_block, nack_dup);
			rtp_session->rtcp_send_msg.header.count = 1; /* reception report block count */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Sending RTCP RR");
		} else {
			struct switch_rtcp_sender_info *rtcp_sender_info;
			rtp_session->rtcp_send_msg.header.type = _RTCP_PT_SR; /* Sender report */
			sr = (struct switch_rtcp_sender_report*) rtp_session->rtcp_send_msg.body;
			sr->ssrc = htonl(rtp_session->ssrc);
			rtcp_sender_info = &sr->sender_info;
			rtcp_generate_sender_info(rtp_session, rtcp_sender_info);
			rtcp_bytes += sizeof(struct switch_rtcp_sender_info);
			if (!rtcp_cyclic && rtcp_fb) {
				 /* rtcp-fb only, don't send receive report block */
				rtp_session->rtcp_send_msg.header.count = 0;
			} else {
				rtcp_report_block = &sr->report_block;
				rtcp_bytes += sizeof(struct switch_rtcp_report_block);
				rtcp_generate_report_block(rtp_session, rtcp_report_block, nack_dup);
				rtp_session->rtcp_send_msg.header.count = 1; /* reception report block count */
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Sending RTCP SR");
		}

		rtp_session->rtcp_send_msg.header.length = htons((uint16_t)(rtcp_bytes / 4) - 1);

		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
			if (rtp_session->pli_count) {
				switch_rtcp_ext_hdr_t *ext_hdr;

				p = (uint8_t *) (&rtp_session->rtcp_send_msg) + rtcp_bytes;
				ext_hdr = (switch_rtcp_ext_hdr_t *) p;

				ext_hdr->version = 2;
				ext_hdr->p = 0;
				ext_hdr->fmt = _RTCP_PSFB_PLI;
				ext_hdr->pt = _RTCP_PT_PSFB;

				ext_hdr->send_ssrc = htonl(rtp_session->ssrc);
				ext_hdr->recv_ssrc = htonl(rtp_session->remote_ssrc);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Sending RTCP PLI %u %u\n",
								  rtp_session->ssrc, rtp_session->remote_ssrc);

				ext_hdr->length = htons((uint8_t)(sizeof(switch_rtcp_ext_hdr_t) / 4) - 1);
				rtcp_bytes += sizeof(switch_rtcp_ext_hdr_t);
				rtp_session->pli_count = 0;
			}

			if (rtp_session->flags[SWITCH_RTP_FLAG_NACK] && nack_ttl > 0) {
				int n = 0;

				for (n = 0; n < nack_ttl; n++) {
					switch_rtcp_ext_hdr_t *ext_hdr;
					uint32_t *nack;
					p = (uint8_t *) (&rtp_session->rtcp_send_msg) + rtcp_bytes;
					ext_hdr = (switch_rtcp_ext_hdr_t *) p;

					ext_hdr->version = 2;
					ext_hdr->p = 0;
					ext_hdr->fmt = _RTCP_RTPFB_NACK;
					ext_hdr->pt = _RTCP_PT_RTPFB;
					ext_hdr->send_ssrc = htonl(rtp_session->ssrc);
					ext_hdr->recv_ssrc = htonl(rtp_session->remote_ssrc);
					ext_hdr->length = htons(3);
					p += sizeof(switch_rtcp_ext_hdr_t);
					nack = (uint32_t *) p;
					*nack = cur_nack[n];

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Sending RTCP NACK %u [%d]\n",
									  ntohs(*nack & 0xFFFF), rtp_session->rtcp_vstats.video_in.nack_count);

					rtcp_bytes += sizeof(switch_rtcp_ext_hdr_t) + sizeof(cur_nack[n]);
					cur_nack[n] = 0;
				}
				rtp_session->prev_nacks_inflight = n;
				nack_ttl = 0;
			}

			if (rtp_session->fir_count) {
				switch_rtcp_ext_hdr_t *ext_hdr;
				rtcp_fir_t *fir;

				if (switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_OLD_FIR)) {
					p = (uint8_t *) (&rtp_session->rtcp_send_msg) + rtcp_bytes;
					ext_hdr = (switch_rtcp_ext_hdr_t *) p;

					ext_hdr->version = 2;
					ext_hdr->pt = _RTCP_PT_FIR;
					rtcp_bytes += sizeof(switch_rtcp_ext_hdr_t);
				}


				p = (uint8_t *) (&rtp_session->rtcp_send_msg) + rtcp_bytes;
				ext_hdr = (switch_rtcp_ext_hdr_t *) p;

				p += sizeof(switch_rtcp_ext_hdr_t);
				fir = (rtcp_fir_t *) p;

				ext_hdr->version = 2;
				ext_hdr->p = 0;
				ext_hdr->fmt = _RTCP_PSFB_FIR;
				ext_hdr->pt = _RTCP_PT_PSFB;

				ext_hdr->send_ssrc = htonl(rtp_session->ssrc);
				ext_hdr->recv_ssrc = htonl(rtp_session->remote_ssrc);

				fir->ssrc = htonl(rtp_session->remote_ssrc);
				fir->seq = rtp_session->fir_seq;
				fir->r1 = fir->r2 = fir->r3 = 0;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Sending RTCP FIR SEQ %d\n", rtp_session->fir_seq);

				rtp_session->fir_seq++;

				ext_hdr->length = htons((uint8_t)((sizeof(switch_rtcp_ext_hdr_t) + sizeof(rtcp_fir_t)) / 4) - 1);
				rtcp_bytes += sizeof(switch_rtcp_ext_hdr_t) + sizeof(rtcp_fir_t);
				rtp_session->fir_count = 0;
			}

			//if (!rtp_session->tmmbr && rtp_session->cur_tmmbr) {
			//	rtp_session->tmmbr = rtp_session->cur_tmmbr;
			//}

			while (rtp_session->tmmbr || rtp_session->tmmbn) {
				switch_rtcp_ext_hdr_t *ext_hdr;
				rtcp_tmmbx_t *tmmbx;
				uint32_t bps = 0;
				p = (uint8_t *) (&rtp_session->rtcp_send_msg) + rtcp_bytes;
				ext_hdr = (switch_rtcp_ext_hdr_t *) p;

				p += sizeof(switch_rtcp_ext_hdr_t);
				tmmbx = (rtcp_tmmbx_t *) p;

				ext_hdr->version = 2;
				ext_hdr->p = 0;
				ext_hdr->pt = _RTCP_PT_RTPFB;
				ext_hdr->send_ssrc = htonl(rtp_session->ssrc);
				ext_hdr->recv_ssrc = 0;

				if (rtp_session->tmmbr) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Sending RTCP TMMBR %u\n", rtp_session->tmmbr);
					ext_hdr->fmt = _RTCP_RTPFB_TMMBR;
					bps = rtp_session->tmmbr;
					rtp_session->tmmbr = 0;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Sending RTCP TMMBN %u\n", rtp_session->tmmbr);
					ext_hdr->fmt = _RTCP_RTPFB_TMMBN;
					bps = rtp_session->tmmbn;
					rtp_session->tmmbn = 0;
				}

				tmmbx->ssrc = htonl(rtp_session->remote_ssrc);
				calc_bw_exp(bps, 17, tmmbx);

				ext_hdr->length = htons((uint8_t)((sizeof(switch_rtcp_ext_hdr_t) + sizeof(rtcp_tmmbx_t)) / 4) - 1);
				rtcp_bytes += sizeof(switch_rtcp_ext_hdr_t) + sizeof(rtcp_tmmbx_t);
			}

		}

		//SDES + CNAME
		p = (uint8_t *) (&rtp_session->rtcp_send_msg) + rtcp_bytes;
		sdes = (switch_rtcp_hdr_t *) p;
		sdes->version = 2;
		sdes->type = _RTCP_PT_SDES;
		sdes->count = 1;
		sdes->p = 0;
		p = (uint8_t *) (sdes) + sdes_bytes;
		ssrc = (uint32_t *) p;
		*ssrc = htonl(rtp_session->ssrc);
		sdes_bytes += sizeof(uint32_t);


		p = (uint8_t *) (sdes) + sdes_bytes;
		unit = (switch_rtcp_sdes_unit_t *) p;
		unit->type = _RTCP_SDES_CNAME;
		snprintf((char *)unit->value, 80, "%x", rtp_session->ssrc);
		unit->length = strlen((char *)unit->value);
		sdes_bytes += sizeof(switch_rtcp_sdes_unit_t) + unit->length;


		p += sizeof(switch_rtcp_sdes_unit_t) + unit->length;
		unit = (switch_rtcp_sdes_unit_t *) p;
		unit->type = _RTCP_SDES_NOTE;
		snprintf((char *)unit->value, 80, "FreeSWITCH.org -- Come to ClueCon.com");
		unit->length = strlen((char *)unit->value);
		sdes_bytes += sizeof(switch_rtcp_sdes_unit_t) + unit->length;

		sdes_bytes ++;//END

		sdes_bytes += 4 - (sdes_bytes % 4);

		sdes->length = htons((uint16_t)(sdes_bytes / 4) - 1);
		rtcp_bytes += sdes_bytes;

		/* Prepare next report */

		stats->last_rpt_cycle = stats->cycle;
		stats->last_rpt_ext_seq = stats->high_ext_seq_recv;
		stats->last_rpt_ts = rtp_session->write_timer.samplecount;
		stats->period_pkt_count = 0;
		stats->sent_pkt_count = 0;



#ifdef ENABLE_SRTP
		if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND]) {
			int stat = 0;
			int sbytes = (int) rtcp_bytes;

			if (!rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_MKI]) {
				stat = srtp_protect_rtcp(rtp_session->send_ctx[rtp_session->srtp_idx_rtcp], &rtp_session->rtcp_send_msg.header, &sbytes);
			} else {
				stat = srtp_protect_rtcp_mki(rtp_session->send_ctx[rtp_session->srtp_idx_rtcp], &rtp_session->rtcp_send_msg.header, &sbytes, 1, SWITCH_CRYPTO_MKI_INDEX);
			}

			if (stat) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error: SRTP RTCP protection failed with code %d\n", stat);
				goto end;
			} else {
				rtcp_bytes = sbytes;
			}

		}
#endif

#ifdef ENABLE_ZRTP
		/* ZRTP Send */
		if (zrtp_on && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA]) {
			unsigned int sbytes = (int) rtcp_bytes;
			zrtp_status_t stat = zrtp_status_fail;

			stat = zrtp_process_rtcp(rtp_session->zrtp_stream, (void *) &rtp_session->rtcp_send_msg, &sbytes);

			switch (stat) {
			case zrtp_status_ok:
				break;
			case zrtp_status_drop:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection drop with code %d\n", stat);
				ret = (int)rtcp_bytes;
				goto end;
				break;
			case zrtp_status_fail:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
				break;
			default:
				break;
			}

			rtcp_bytes = sbytes;
		}
#endif
		//#define DEBUG_EXTRA
#ifdef DEBUG_EXTRA
		{
			const char *old_host;
			char bufb[50];
			old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->rtcp_remote_addr);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_CRIT, "%s SEND %s RTCP %s:%d %ld\n",
							  rtp_session_name(rtp_session),
							  rtp_type(rtp_session),
							  old_host,
							  switch_sockaddr_get_port(rtp_session->rtcp_remote_addr),
							  rtcp_bytes);
		}
#endif
		if (switch_socket_sendto(rtp_session->rtcp_sock_output, rtp_session->rtcp_remote_addr, 0, (void *)&rtp_session->rtcp_send_msg, &rtcp_bytes ) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG,"RTCP packet not written\n");
		} else {
			rtp_session->stats.inbound.period_packet_count = 0;
		}
	}

	if (rtp_session->ice.ice_user) {
		if (ice_out(rtp_session, &rtp_session->ice) == SWITCH_STATUS_GENERR) {
			ret = -1;
			goto end;
		}
	}

	if (!rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
		if (rtp_session->rtcp_ice.ice_user) {
			if (ice_out(rtp_session, &rtp_session->rtcp_ice) == SWITCH_STATUS_GENERR) {
				ret = -1;
				goto end;
			}
		}
	}

 end:

	return ret;
}

SWITCH_DECLARE(void) switch_rtp_ping(switch_rtp_t *rtp_session)
{
	check_rtcp_and_ice(rtp_session);
}

SWITCH_DECLARE(void) switch_rtp_get_random(void *buf, uint32_t len)
{
#ifdef HAVE_OPENSSL
	RAND_bytes(buf, len);
#else
	switch_stun_random_string(buf, len, NULL);
#endif
}


SWITCH_DECLARE(void) switch_rtp_shutdown(void)
{
	switch_core_port_allocator_t *alloc = NULL;
	switch_hash_index_t *hi;
	const void *var;
	void *val;

	if (!global_init) {
		return;
	}

	switch_mutex_lock(port_lock);

	for (hi = switch_core_hash_first(alloc_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &var, NULL, &val);
		if ((alloc = (switch_core_port_allocator_t *) val)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroy port allocator for %s\n", (char *) var);
			switch_core_port_allocator_destroy(&alloc);
		}
	}

	switch_core_hash_destroy(&alloc_hash);
	switch_mutex_unlock(port_lock);

#ifdef ENABLE_ZRTP
	if (zrtp_on) {
		zrtp_status_t status = zrtp_status_ok;

		status = zrtp_def_cache_store(zrtp_global);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Saving ZRTP cache: %s\n", zrtp_status_ok == status ? "OK" : "FAIL");
		zrtp_down(zrtp_global);
	}
#endif
#ifdef ENABLE_SRTP
	srtp_crypto_kernel_shutdown();
#endif

}

SWITCH_DECLARE(switch_port_t) switch_rtp_set_start_port(switch_port_t port)
{
	if (port) {
		if (port_lock) {
			switch_mutex_lock(port_lock);
		}
		START_PORT = port;
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
		if (port_lock) {
			switch_mutex_unlock(port_lock);
		}
	}
	return END_PORT;
}

SWITCH_DECLARE(void) switch_rtp_release_port(const char *ip, switch_port_t port)
{
	switch_core_port_allocator_t *alloc = NULL;

	if (!ip || !port) {
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
		if (switch_core_port_allocator_new(ip, START_PORT, END_PORT, SPF_EVEN, &alloc) != SWITCH_STATUS_SUCCESS) {
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

SWITCH_DECLARE(switch_status_t) switch_rtp_set_payload_map(switch_rtp_t *rtp_session, payload_map_t **pmap)
{

	if (rtp_session) {
		switch_mutex_lock(rtp_session->flag_mutex);
		rtp_session->pmaps = pmap;
		switch_mutex_unlock(rtp_session->flag_mutex);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(void) switch_rtp_intentional_bugs(switch_rtp_t *rtp_session, switch_rtp_bug_flag_t bugs)
{
	rtp_session->rtp_bugs = bugs;

	if ((rtp_session->rtp_bugs & RTP_BUG_START_SEQ_AT_ZERO)) {
		rtp_session->seq = 0;
	}

}


static switch_status_t enable_remote_rtcp_socket(switch_rtp_t *rtp_session, const char **err) {

	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {

		if (switch_sockaddr_info_get(&rtp_session->rtcp_remote_addr, rtp_session->eff_remote_host_str, SWITCH_UNSPEC,
									 rtp_session->remote_rtcp_port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS || !rtp_session->rtcp_remote_addr) {
			*err = "RTCP Remote Address Error!";
			return SWITCH_STATUS_FALSE;
		} else {
			const char *host;
			char bufa[50];

			host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtcp_remote_addr);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG,
							  "Setting RTCP remote addr to %s:%d %d\n", host, rtp_session->remote_rtcp_port, rtp_session->rtcp_remote_addr->family);
		}

		if (rtp_session->rtcp_sock_input && switch_sockaddr_get_family(rtp_session->rtcp_remote_addr) ==
			switch_sockaddr_get_family(rtp_session->rtcp_local_addr)) {
			rtp_session->rtcp_sock_output = rtp_session->rtcp_sock_input;
		} else {

			if (rtp_session->rtcp_sock_output && rtp_session->rtcp_sock_output != rtp_session->rtcp_sock_input) {
				switch_socket_close(rtp_session->rtcp_sock_output);
			}

			if ((status = switch_socket_create(&rtp_session->rtcp_sock_output,
											   switch_sockaddr_get_family(rtp_session->rtcp_remote_addr),
											   SOCK_DGRAM, 0, rtp_session->pool)) != SWITCH_STATUS_SUCCESS) {
				*err = "RTCP Socket Error!";
			}
		}

	} else {
		*err = "RTCP NOT ACTIVE!";
	}

	return status;

}

static switch_status_t enable_local_rtcp_socket(switch_rtp_t *rtp_session, const char **err) {

	const char *host = rtp_session->local_host_str;
	switch_port_t port = rtp_session->local_port;
	switch_socket_t *rtcp_new_sock = NULL, *rtcp_old_sock = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char bufa[50];

	if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
		if (switch_sockaddr_info_get(&rtp_session->rtcp_local_addr, host, SWITCH_UNSPEC, port+1, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
			*err = "RTCP Local Address Error!";
			goto done;
		}

		if (switch_socket_create(&rtcp_new_sock, switch_sockaddr_get_family(rtp_session->rtcp_local_addr), SOCK_DGRAM, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
			*err = "RTCP Socket Error!";
			goto done;
		}

		if (switch_socket_opt_set(rtcp_new_sock, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
			*err = "RTCP Socket Error!";
			goto done;
		}

		if (switch_socket_bind(rtcp_new_sock, rtp_session->rtcp_local_addr) != SWITCH_STATUS_SUCCESS) {
			*err = "RTCP Bind Error!";
			goto done;
		}

		if (switch_sockaddr_info_get(&rtp_session->rtcp_from_addr, switch_get_addr(bufa, sizeof(bufa), rtp_session->from_addr),
											 SWITCH_UNSPEC, switch_sockaddr_get_port(rtp_session->from_addr) + 1, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
			*err = "RTCP From Address Error!";
			goto done;
		}

		rtcp_old_sock = rtp_session->rtcp_sock_input;
		rtp_session->rtcp_sock_input = rtcp_new_sock;
		rtcp_new_sock = NULL;

		switch_socket_create_pollset(&rtp_session->rtcp_read_pollfd, rtp_session->rtcp_sock_input, SWITCH_POLLIN | SWITCH_POLLERR, rtp_session->pool);

 done:

		if (*err) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error allocating rtcp [%s]\n", *err);
			status = SWITCH_STATUS_FALSE;
		}

		if (rtcp_new_sock) {
			switch_socket_close(rtcp_new_sock);
		}

		if (rtcp_old_sock) {
			switch_socket_close(rtcp_old_sock);
		}
	} else {
		status = SWITCH_STATUS_FALSE;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_set_local_address(switch_rtp_t *rtp_session, const char *host, switch_port_t port, const char **err)
{
	switch_socket_t *new_sock = NULL, *old_sock = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int j = 0;
#ifndef WIN32
	char o[5] = "TEST", i[5] = "";
	switch_size_t len, ilen = 0;
	int x;
#endif

	if (rtp_session->ready != 1) {
		if (!switch_rtp_ready(rtp_session)) {
			return SWITCH_STATUS_FALSE;
		}

		READ_INC(rtp_session);
		WRITE_INC(rtp_session);

		if (!switch_rtp_ready(rtp_session)) {
			goto done;
		}
	}


	*err = NULL;

	if (zstr(host) || !port) {
		*err = "Address Error";
		goto done;
	}


	rtp_session->local_host_str = switch_core_strdup(rtp_session->pool, host);
	rtp_session->local_port = port;


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

	//if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
	//switch_socket_opt_set(new_sock, SWITCH_SO_RCVBUF, 1572864);
		//switch_socket_opt_set(new_sock, SWITCH_SO_SNDBUF, 1572864);
	//}

	if (switch_socket_bind(new_sock, rtp_session->local_addr) != SWITCH_STATUS_SUCCESS) {
		char *em = switch_core_sprintf(rtp_session->pool, "Bind Error! %s:%d", host, port);
		*err = em;
		goto done;
	}


	if ((j = atoi(host)) && j > 223 && j < 240) { /* mcast */
		if (switch_mcast_interface(new_sock, rtp_session->local_addr) != SWITCH_STATUS_SUCCESS) {
			*err = "Multicast Socket interface Error";
			goto done;
		}

		if (switch_mcast_join(new_sock, rtp_session->local_addr, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			*err = "Multicast Error";
			goto done;
		}

		if (rtp_session->session) {
			switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
			const char *var;

			if ((var = switch_channel_get_variable(channel, "multicast_ttl"))) {
				int ttl = atoi(var);

				if (ttl > 0 && ttl < 256) {
					if (switch_mcast_hops(new_sock, (uint8_t) ttl) != SWITCH_STATUS_SUCCESS) {
						*err = "Mutlicast TTL set failed";
						goto done;
					}

				}
			}

		}

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

	if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] || rtp_session->flags[SWITCH_RTP_FLAG_NOBLOCK] || rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
		switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, TRUE);
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
	}

	switch_socket_create_pollset(&rtp_session->read_pollfd, rtp_session->sock_input, SWITCH_POLLIN | SWITCH_POLLERR, rtp_session->pool);

	if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
		if ((status = enable_local_rtcp_socket(rtp_session, err)) == SWITCH_STATUS_SUCCESS) {
			*err = "Success";
		}
	} else {
		status = SWITCH_STATUS_SUCCESS;
		*err = "Success";
	}

	switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_IO);

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
	if (!switch_rtp_ready(rtp_session) || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
		return;
	}

	if (rtp_session->missed_count >= max) {

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING,
						  "new max missed packets(%d->%d) greater than current missed packets(%d). RTP will timeout.\n",
						  rtp_session->missed_count, max, rtp_session->missed_count);
	}

	rtp_session->max_missed_packets = max;
}

SWITCH_DECLARE(void) switch_rtp_reset_jb(switch_rtp_t *rtp_session)
{
	if (switch_rtp_ready(rtp_session)) {
		if (rtp_session->jb) {
			switch_jb_reset(rtp_session->jb);
		}
	}
}

SWITCH_DECLARE(void) switch_rtp_reset_vb(switch_rtp_t *rtp_session)
{

	if (rtp_session->vb) {
		switch_jb_reset(rtp_session->vb);
	}

	if (rtp_session->vbw) {
		switch_jb_reset(rtp_session->vbw);
	}
}

SWITCH_DECLARE(void) switch_rtp_reset(switch_rtp_t *rtp_session)
{
	if (!rtp_session) {
		return;
	}

	rtp_session->seq = (uint16_t) rand();
	rtp_session->ts = 0;
	memset(&rtp_session->ts_norm, 0, sizeof(rtp_session->ts_norm));

	rtp_session->last_stun = rtp_session->first_stun = 0;
	rtp_session->wrong_addrs = 0;
	rtp_session->rtcp_sent_packets = 0;
	rtp_session->rtcp_last_sent = 0;
	rtp_session->ice_adj = 0;

	//switch_rtp_del_dtls(rtp_session, DTLS_TYPE_RTP|DTLS_TYPE_RTCP);
	switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_PAUSE);
	switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_MUTE);
	rtcp_stats_init(rtp_session);

	if (rtp_session->ice.ready) {
		switch_rtp_reset_vb(rtp_session);
		rtp_session->ice.ready = rtp_session->ice.rready = 0;
	}

}

SWITCH_DECLARE(void) switch_rtp_reset_media_timer(switch_rtp_t *rtp_session)
{
	rtp_session->missed_count = 0;
}

SWITCH_DECLARE(char *) switch_rtp_get_remote_host(switch_rtp_t *rtp_session)
{
	return zstr(rtp_session->remote_host_str) ? "0.0.0.0" : rtp_session->remote_host_str;
}

SWITCH_DECLARE(switch_port_t) switch_rtp_get_remote_port(switch_rtp_t *rtp_session)
{
	return rtp_session->remote_port;
}

static void ping_socket(switch_rtp_t *rtp_session)
{
	uint32_t o = UINT_MAX;
	switch_size_t len = sizeof(o);
	switch_socket_sendto(rtp_session->sock_input, rtp_session->local_addr, 0, (void *) &o, &len);

	if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP] && rtp_session->rtcp_sock_input) {
		switch_socket_sendto(rtp_session->rtcp_sock_input, rtp_session->rtcp_local_addr, 0, (void *) &o, &len);
	}
}

SWITCH_DECLARE(switch_status_t) switch_rtp_udptl_mode(switch_rtp_t *rtp_session)
{
	switch_socket_t *sock;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		ping_socket(rtp_session);
	}

	READ_INC(rtp_session);
	WRITE_INC(rtp_session);

	if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] || rtp_session->timer.timer_interface) {
		switch_core_timer_destroy(&rtp_session->timer);
		memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
	}

	rtp_session->missed_count = 0;
	rtp_session->max_missed_packets = 0;

	rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP] = 0;

	if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
		rtp_session->rtcp_sock_input = NULL;
		rtp_session->rtcp_sock_output = NULL;
	} else {
		if (rtp_session->rtcp_sock_input && rtp_session->rtcp_sock_input != rtp_session->sock_input) {
			ping_socket(rtp_session);
			switch_socket_shutdown(rtp_session->rtcp_sock_input, SWITCH_SHUTDOWN_READWRITE);
		}

		if (rtp_session->rtcp_sock_output && rtp_session->rtcp_sock_output != rtp_session->rtcp_sock_input &&
			rtp_session->rtcp_sock_output != rtp_session->sock_input) {
			switch_socket_shutdown(rtp_session->rtcp_sock_output, SWITCH_SHUTDOWN_READWRITE);
		}

		if ((sock = rtp_session->rtcp_sock_input)) {
			rtp_session->rtcp_sock_input = NULL;
			switch_socket_close(sock);

			if (rtp_session->rtcp_sock_output && rtp_session->rtcp_sock_output != sock) {
				if ((sock = rtp_session->rtcp_sock_output)) {
					rtp_session->rtcp_sock_output = NULL;
					switch_socket_close(sock);
				}
			}
		}
	}

	switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL);
	switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA);
	switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, FALSE);
	switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);

	WRITE_DEC(rtp_session);
	READ_DEC(rtp_session);

	switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_STICKY_FLUSH);
	switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH);

	switch_rtp_break(rtp_session);

	return SWITCH_STATUS_SUCCESS;

}


SWITCH_DECLARE(switch_status_t) switch_rtp_set_remote_address(switch_rtp_t *rtp_session, const char *host, switch_port_t port, switch_port_t remote_rtcp_port,
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

	rtp_session->eff_remote_host_str = switch_core_strdup(rtp_session->pool, host);
	rtp_session->eff_remote_port = port;

	if (rtp_session->sock_input && switch_sockaddr_get_family(rtp_session->remote_addr) == switch_sockaddr_get_family(rtp_session->local_addr)) {
		rtp_session->sock_output = rtp_session->sock_input;
	} else {
		if (rtp_session->sock_output && rtp_session->sock_output != rtp_session->sock_input) {
			switch_socket_close(rtp_session->sock_output);
		}
		if ((status = switch_socket_create(&rtp_session->sock_output,
										   switch_sockaddr_get_family(rtp_session->remote_addr),
										   SOCK_DGRAM, 0, rtp_session->pool)) != SWITCH_STATUS_SUCCESS) {

			*err = "Socket Error!";
		}
	}

	if (rtp_session->dtls) {
		rtp_session->dtls->sock_output = rtp_session->sock_output;

		if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
			switch_sockaddr_info_get(&rtp_session->dtls->remote_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool);
		}
	}


	if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
		if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
			rtp_session->rtcp_remote_addr = rtp_session->remote_addr;
			rtp_session->rtcp_sock_output = rtp_session->sock_output;
		}/* else {
			if (remote_rtcp_port) {
				rtp_session->remote_rtcp_port = remote_rtcp_port;
			} else {
				rtp_session->remote_rtcp_port = rtp_session->eff_remote_port + 1;
			}
			status = enable_remote_rtcp_socket(rtp_session, err);

			if (rtp_session->rtcp_dtls) {
				//switch_sockaddr_info_get(&rtp_session->rtcp_dtls->remote_addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool);
				rtp_session->rtcp_dtls->remote_addr = rtp_session->rtcp_remote_addr;
				rtp_session->rtcp_dtls->sock_output = rtp_session->rtcp_sock_output;
			}
			}*/
	}

	switch_mutex_unlock(rtp_session->write_mutex);

	return status;
}


static const char *dtls_state_names_t[] = {"OFF", "HANDSHAKE", "SETUP", "READY", "FAIL", "INVALID"};
static const char *dtls_state_names(dtls_state_t s)
{
	if (s > DS_INVALID) {
		s = DS_INVALID;
	}

	return dtls_state_names_t[s];
}

#define dtls_set_state(_dtls, _state) switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO, "Changing %s DTLS state from %s to %s\n", rtp_type(rtp_session), dtls_state_names(_dtls->state), dtls_state_names(_state)); _dtls->new_state = 1; _dtls->last_state = _dtls->state; _dtls->state = _state

#define cr_keylen 16
#define cr_saltlen 14
#define cr_kslen 30

static int dtls_state_setup(switch_rtp_t *rtp_session, switch_dtls_t *dtls)
{
	X509 *cert;
	switch_secure_settings_t	ssec;	/* Used just to wrap over params in a call to switch_rtp_add_crypto_key. */
	int r = 0;

	uint8_t raw_key_data[cr_kslen * 2];
	unsigned char local_key_buf[cr_kslen];
	unsigned char remote_key_buf[cr_kslen];

	memset(&ssec, 0, sizeof(ssec));
	memset(&raw_key_data, 0, cr_kslen * 2 * sizeof(uint8_t));
	memset(&local_key_buf, 0, cr_kslen * sizeof(unsigned char));
	memset(&remote_key_buf, 0, cr_kslen * sizeof(unsigned char));

	if ((dtls->type & DTLS_TYPE_SERVER)) {
		r = 1;
	} else if ((cert = SSL_get_peer_certificate(dtls->ssl))) {
		switch_core_cert_extract_fingerprint(cert, dtls->remote_fp);
		r = switch_core_cert_verify(dtls->remote_fp);
		X509_free(cert);
	}

	if (!r) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s Fingerprint Verification Failed!\n", rtp_type(rtp_session));
		dtls_set_state(dtls, DS_FAIL);
		return -1;
	} else {
		unsigned char *local_key, *remote_key, *local_salt, *remote_salt;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO, "%s Fingerprint Verified.\n", rtp_type(rtp_session));

#ifdef HAVE_OPENSSL_DTLS_SRTP
		if (!SSL_export_keying_material(dtls->ssl, raw_key_data, sizeof(raw_key_data), "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s Key material export failure\n", rtp_type(rtp_session));
			dtls_set_state(dtls, DS_FAIL);
			return -1;
		}
#else
		return -1;
#endif

		if ((dtls->type & DTLS_TYPE_CLIENT)) {
			local_key = raw_key_data;
			remote_key = local_key + cr_keylen;
			local_salt = remote_key + cr_keylen;
			remote_salt = local_salt + cr_saltlen;

		} else {
			remote_key = raw_key_data;
			local_key = remote_key + cr_keylen;
			remote_salt = local_key + cr_keylen;
			local_salt = remote_salt + cr_saltlen;
		}

		memcpy(ssec.local_raw_key, local_key, cr_keylen);
		memcpy(ssec.local_raw_key + cr_keylen, local_salt, cr_saltlen);

		memcpy(ssec.remote_raw_key, remote_key, cr_keylen);
		memcpy(ssec.remote_raw_key + cr_keylen, remote_salt, cr_saltlen);

		ssec.crypto_type = AES_CM_128_HMAC_SHA1_80;

		if (dtls == rtp_session->rtcp_dtls && rtp_session->rtcp_dtls != rtp_session->dtls) {
			switch_rtp_add_crypto_key(rtp_session, SWITCH_RTP_CRYPTO_SEND_RTCP, 0, &ssec);
			switch_rtp_add_crypto_key(rtp_session, SWITCH_RTP_CRYPTO_RECV_RTCP, 0, &ssec);
		} else {
			switch_rtp_add_crypto_key(rtp_session, SWITCH_RTP_CRYPTO_SEND, 0, &ssec);
			switch_rtp_add_crypto_key(rtp_session, SWITCH_RTP_CRYPTO_RECV, 0, &ssec);
		}
	}

	dtls_set_state(dtls, DS_READY);

	return 0;
}

static int dtls_state_ready(switch_rtp_t *rtp_session, switch_dtls_t *dtls)
{

	if (dtls->new_state) {
		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
			switch_core_session_t *other_session;

			if (rtp_session->session && switch_core_session_get_partner(rtp_session->session, &other_session) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_request_video_refresh(other_session);
				switch_core_session_rwunlock(other_session);
			}
		}
		dtls->new_state = 0;
	}
	return 0;
}

static int dtls_state_fail(switch_rtp_t *rtp_session, switch_dtls_t *dtls)
{
	if (rtp_session->session) {
		switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}

	return -1;
}


static int dtls_state_handshake(switch_rtp_t *rtp_session, switch_dtls_t *dtls)
{
	int ret;

	if ((ret = SSL_do_handshake(dtls->ssl)) != 1){
		switch((ret = SSL_get_error(dtls->ssl, ret))){
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_NONE:
			break;
		default:
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s Handshake failure %d. This may happen when you use legacy DTLS v1.0 (legacyDTLS channel var is set) but endpoint requires DTLS v1.2.\n", rtp_type(rtp_session), ret);
			dtls_set_state(dtls, DS_FAIL);
			return -1;
		}
	}

	if (SSL_is_init_finished(dtls->ssl)) {
		dtls_set_state(dtls, DS_SETUP);
	}

	return 0;
}

static void free_dtls(switch_dtls_t **dtlsp)
{
	switch_dtls_t *dtls;

	if (!dtlsp) {
		return;
	}

	dtls = *dtlsp;
	*dtlsp = NULL;

	if (dtls->ssl) {
		SSL_free(dtls->ssl);
	}

	if (dtls->ssl_ctx) {
		SSL_CTX_free(dtls->ssl_ctx);
	}
}

static int do_dtls(switch_rtp_t *rtp_session, switch_dtls_t *dtls)
{
	int r = 0, ret = 0, len;
	switch_size_t bytes;
	unsigned char buf[MAX_DTLS_MTU] = "";
	int ready = rtp_session->ice.ice_user ? (rtp_session->ice.rready && rtp_session->ice.ready) : 1;
	int pending;

	if (!dtls->bytes && !ready) {
		return 0;
	}

	if (dtls->bytes > 0 && dtls->data) {
		ret = BIO_write(dtls->read_bio, dtls->data, (int)dtls->bytes);
		if (ret <= 0) {
			ret = SSL_get_error(dtls->ssl, ret);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS packet decode err: SSL err %d\n", rtp_type(rtp_session), ret);
		} else if (ret != (int)dtls->bytes) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS packet decode err: read %d bytes instead of %d\n", rtp_type(rtp_session), ret, (int)dtls->bytes);
		}
	}

	if (dtls_states[dtls->state]) {
		r = dtls_states[dtls->state](rtp_session, dtls);
	}

	while ((pending = BIO_ctrl_pending(dtls->filter_bio)) > 0) {
		switch_assert(pending <= sizeof(buf));

		len = BIO_read(dtls->write_bio, buf, pending);
		if (len > 0) {
			bytes = len;
			ret = switch_socket_sendto(dtls->sock_output, dtls->remote_addr, 0, (void *)buf, &bytes);

			if (ret != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS packet not written to socket: %d\n", rtp_type(rtp_session), ret);
			} else if (bytes != len) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS packet write err: written %d bytes instead of %d\n", rtp_type(rtp_session), (int)bytes, len);
			}
		} else {
			ret = SSL_get_error(dtls->ssl, len);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS packet encode err: SSL err %d\n", rtp_type(rtp_session), ret);
		}
	}

	return r;
}

#if VERIFY
static int cb_verify_peer(int preverify_ok, X509_STORE_CTX *ctx)
{
	SSL *ssl = NULL;
	switch_dtls_t *dtls;
	X509 *cert;
	int r = 0;

	ssl = X509_STORE_CTX_get_app_data(ctx);
	dtls = (switch_dtls_t *) SSL_get_app_data(ssl);

	if (!(ssl && dtls)) {
		return 0;
	}

	if ((cert = SSL_get_peer_certificate(dtls->ssl))) {
		switch_core_cert_extract_fingerprint(cert, dtls->remote_fp);

		r = switch_core_cert_verify(dtls->remote_fp);

		X509_free(cert);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(dtls->rtp_session->session), SWITCH_LOG_ERROR, "%s CERT ERR!\n", rtp_type(dtls->rtp_session));
	}

	return r;
}
#endif


////////////

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static BIO_METHOD dtls_bio_filter_methods;
#else
static BIO_METHOD *dtls_bio_filter_methods;
#endif

BIO_METHOD *BIO_dtls_filter(void) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L	
	return(&dtls_bio_filter_methods);
#else
	return(dtls_bio_filter_methods);
#endif
}

typedef struct packet_list_s {
	//void *packet;
	int size;
	struct packet_list_s *next;
} packet_list_t;
 
/* Helper struct to keep the filter state */
typedef struct dtls_bio_filter {
	packet_list_t *packets;
	packet_list_t *unused;
	packet_list_t *tail;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	long mtu;
} dtls_bio_filter;
 
 
static int dtls_bio_filter_new(BIO *bio) {
	/* Create a filter state struct */
	dtls_bio_filter *filter;
	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);
	filter = switch_core_alloc(pool, sizeof(*filter));
	filter->pool = pool;

	filter->packets = NULL;
	switch_mutex_init(&filter->mutex, SWITCH_MUTEX_NESTED, filter->pool);
 
	/* Set the BIO as initialized */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	bio->init = 1;
	bio->ptr = filter;
	bio->flags = 0;
#else
	BIO_set_init(bio, 1);
	BIO_set_data(bio, filter);
	BIO_clear_flags(bio, ~0);
#endif
	
	return 1;
}
 
static int dtls_bio_filter_free(BIO *bio) {
	dtls_bio_filter *filter;

	if (bio == NULL) {
		return 0;
	}
 
	/* Get rid of the filter state */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	filter = (dtls_bio_filter *)bio->ptr;
#else
	filter = (dtls_bio_filter *)BIO_get_data(bio);
#endif

	if (filter != NULL) {
		switch_memory_pool_t *pool = filter->pool;
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
		filter = NULL;
	}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	bio->ptr = NULL;
	bio->init = 0;
	bio->flags = 0;
#else
	BIO_set_init(bio, 0);
	BIO_set_data(bio, NULL);
	BIO_clear_flags(bio, ~0);
#endif
	return 1;
}
 
static int dtls_bio_filter_write(BIO *bio, const char *in, int inl) {
	long ret;
	dtls_bio_filter *filter;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "dtls_bio_filter_write: %p, %d\n", (void *)in, inl);
	/* Forward data to the write BIO */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	ret = BIO_write(bio->next_bio, in, inl);
#else
	ret = BIO_write(BIO_next(bio), in, inl);
#endif
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "  -- %ld\n", ret);
 
	/* Keep track of the packet, as we'll advertize them one by one after a pending check */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	filter = (dtls_bio_filter *)bio->ptr;
#else
	filter = (dtls_bio_filter *)BIO_get_data(bio);
#endif

	if (filter != NULL) {
		packet_list_t *node;

		switch_mutex_lock(filter->mutex);
		if (filter->unused) {
			node = filter->unused;
			node->next = NULL;
			filter->unused = filter->unused->next;
		} else {
			node = switch_core_alloc(filter->pool, sizeof(*node));
		}

		node->size = ret;

		if (filter->tail) {
			filter->tail->next = node;
		} else {
			filter->packets = node;
		}

		filter->tail = node;

		switch_mutex_unlock(filter->mutex);
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "New list length: %d\n", g_list_length(filter->packets));
	}
	return ret;
}
 
static long dtls_bio_filter_ctrl(BIO *bio, int cmd, long num, void *ptr) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	dtls_bio_filter *filter = (dtls_bio_filter *)bio->ptr;
#else
	dtls_bio_filter *filter = (dtls_bio_filter *)BIO_get_data(bio);
#endif

	switch(cmd) {
	case BIO_CTRL_DGRAM_GET_FALLBACK_MTU:
		return 1200;
	case BIO_CTRL_DGRAM_SET_MTU:
		filter->mtu = num;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Setting MTU: %ld\n", filter->mtu);
		return num;
	case BIO_CTRL_FLUSH:
		return 1;
	case BIO_CTRL_DGRAM_QUERY_MTU:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Advertizing MTU: %ld\n", filter->mtu);
		return filter->mtu;
	case BIO_CTRL_WPENDING:
		return 0L;
	case BIO_CTRL_PENDING: {
		int pending = 0;
		packet_list_t *top;

		if (filter == NULL) {
			return 0;
		}

		switch_mutex_lock(filter->mutex);
		if ((top = filter->packets)) {
			filter->packets = filter->packets->next;
			
			if (top == filter->tail || !filter->packets) {
				filter->tail = NULL;
			}
			
			pending = top->size;
			top->next = filter->unused;
			filter->unused = top;
		}
		switch_mutex_unlock(filter->mutex);

		return pending;
	}
	default:
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "dtls_bio_filter_ctrl: %d\n", cmd);
		break;
	}
	return 0;
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static BIO_METHOD dtls_bio_filter_methods = {
	BIO_TYPE_FILTER,
	"DTLS filter",
	dtls_bio_filter_write,
	NULL,
	NULL,
	NULL,
	dtls_bio_filter_ctrl,
	dtls_bio_filter_new,
	dtls_bio_filter_free,
	NULL
};
#else
static BIO_METHOD *dtls_bio_filter_methods = NULL;
#endif

///////////



SWITCH_DECLARE(int) switch_rtp_has_dtls(void) {
#ifdef HAVE_OPENSSL_DTLS_SRTP
	return 1;
#else
	return 0;
#endif
}

SWITCH_DECLARE(dtls_state_t) switch_rtp_dtls_state(switch_rtp_t *rtp_session, dtls_type_t type)
{
	dtls_state_t s = DS_OFF;

	switch_mutex_lock(rtp_session->ice_mutex);

	if (!rtp_session || (!rtp_session->dtls && !rtp_session->rtcp_dtls)) {
		s = DS_OFF;
		goto done;
	}

	if ((type == DTLS_TYPE_RTP) && rtp_session->dtls) {
		s = rtp_session->dtls->state;
		goto done;
	}

	if ((type == DTLS_TYPE_RTCP) && rtp_session->rtcp_dtls) {
		s = rtp_session->rtcp_dtls->state;
	}

 done:

	switch_mutex_unlock(rtp_session->ice_mutex);

	return s;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_del_dtls(switch_rtp_t *rtp_session, dtls_type_t type)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_mutex_lock(rtp_session->ice_mutex);

	if (!rtp_session || (!rtp_session->dtls && !rtp_session->rtcp_dtls)) {
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	if ((type & DTLS_TYPE_RTP)) {
		if (rtp_session->dtls && rtp_session->dtls == rtp_session->rtcp_dtls) {
			rtp_session->rtcp_dtls = NULL;
		}

		if (rtp_session->dtls) {
			free_dtls(&rtp_session->dtls);
		}

		if (rtp_session->jb) {
			switch_jb_reset(rtp_session->jb);
		}

		if (rtp_session->vb) {
			switch_jb_reset(rtp_session->vb);
		}

		if (rtp_session->vbw) {
			switch_jb_reset(rtp_session->vbw);
		}

	}

	if ((type & DTLS_TYPE_RTCP) && rtp_session->rtcp_dtls) {
		free_dtls(&rtp_session->rtcp_dtls);
	}


#ifdef ENABLE_SRTP
	if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND]) {
		int x;

		rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND] = 0;

		for(x = 0; x < 2; x++) {
			if (rtp_session->send_ctx[x]) {
				srtp_dealloc(rtp_session->send_ctx[x]);
				rtp_session->send_ctx[x] = NULL;
			}
		}
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV]) {
		int x;

		rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] = 0;

		for (x = 0; x < 2; x++) {
			if (rtp_session->recv_ctx[x]) {
				srtp_dealloc(rtp_session->recv_ctx[x]);
				rtp_session->recv_ctx[x] = NULL;
			}
		}
	}
#endif

 done:

	switch_mutex_unlock(rtp_session->ice_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_add_dtls(switch_rtp_t *rtp_session, dtls_fingerprint_t *local_fp, dtls_fingerprint_t *remote_fp, dtls_type_t type, uint8_t want_DTLSv1_2)
{
	switch_dtls_t *dtls;
	const char *var;
	int ret;
	const char *kind = "";
	BIO *bio;
	DH *dh;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
#ifndef OPENSSL_NO_EC
	EC_KEY* ecdh;
#endif

#ifndef HAVE_OPENSSL_DTLS_SRTP
	return SWITCH_STATUS_FALSE;
#endif

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(rtp_session->ice_mutex);

	if (!((type & DTLS_TYPE_RTP) || (type & DTLS_TYPE_RTCP)) || !((type & DTLS_TYPE_CLIENT) || (type & DTLS_TYPE_SERVER))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_CRIT, "INVALID TYPE!\n");
	}

	switch_rtp_del_dtls(rtp_session, type);

	if ((type & DTLS_TYPE_RTP) && (type & DTLS_TYPE_RTCP)) {
		kind = "RTP/RTCP";
	} else if ((type & DTLS_TYPE_RTP)) {
		kind = "RTP";
	} else {
		kind = "RTCP";
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO,
					  "Activate %s %s DTLS %s\n", kind, rtp_type(rtp_session), (type & DTLS_TYPE_SERVER) ? "server" : "client");

	if (((type & DTLS_TYPE_RTP) && rtp_session->dtls) || ((type & DTLS_TYPE_RTCP) && rtp_session->rtcp_dtls)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "DTLS ALREADY INIT\n");
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	dtls = switch_core_alloc(rtp_session->pool, sizeof(*dtls));

	dtls->pem = switch_core_sprintf(rtp_session->pool, "%s%s%s.pem", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, DTLS_SRTP_FNAME);

	if (switch_file_exists(dtls->pem, rtp_session->pool) == SWITCH_STATUS_SUCCESS) {
		dtls->pvt = dtls->rsa = dtls->pem;
	} else {
		dtls->pvt = switch_core_sprintf(rtp_session->pool, "%s%s%s.key", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, DTLS_SRTP_FNAME);
		dtls->rsa = switch_core_sprintf(rtp_session->pool, "%s%s%s.crt", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, DTLS_SRTP_FNAME);
	}

	dtls->ca = switch_core_sprintf(rtp_session->pool, "%s%sca-bundle.crt", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR);

#if OPENSSL_VERSION_NUMBER >= 0x10100000
	dtls->ssl_ctx = SSL_CTX_new((type & DTLS_TYPE_SERVER) ? DTLS_server_method() : DTLS_client_method());
#else
    #ifdef HAVE_OPENSSL_DTLSv1_2_method
	        dtls->ssl_ctx = SSL_CTX_new((type & DTLS_TYPE_SERVER) ? (want_DTLSv1_2 ? DTLSv1_2_server_method() : DTLSv1_server_method()) : (want_DTLSv1_2 ? DTLSv1_2_client_method() : DTLSv1_client_method()));
    #else
            dtls->ssl_ctx = SSL_CTX_new((type & DTLS_TYPE_SERVER) ? DTLSv1_server_method() : DTLSv1_client_method());
    #endif // HAVE_OPENSSL_DTLSv1_2_method
#endif
	switch_assert(dtls->ssl_ctx);

	bio = BIO_new_file(dtls->pem, "r");
	dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (dh) {
		SSL_CTX_set_tmp_dh(dtls->ssl_ctx, dh);
		DH_free(dh);
	}

	SSL_CTX_set_mode(dtls->ssl_ctx, SSL_MODE_AUTO_RETRY);

	//SSL_CTX_set_verify(dtls->ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_verify(dtls->ssl_ctx, SSL_VERIFY_NONE, NULL);

	//SSL_CTX_set_cipher_list(dtls->ssl_ctx, "ECDH:!RC4:!SSLv3:RSA_WITH_AES_128_CBC_SHA");
	//SSL_CTX_set_cipher_list(dtls->ssl_ctx, "ECDHE-RSA-AES256-GCM-SHA384");
	SSL_CTX_set_cipher_list(dtls->ssl_ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	//SSL_CTX_set_cipher_list(dtls->ssl_ctx, "SUITEB128");
	SSL_CTX_set_read_ahead(dtls->ssl_ctx, 1);
#ifdef HAVE_OPENSSL_DTLS_SRTP
	//SSL_CTX_set_tlsext_use_srtp(dtls->ssl_ctx, "SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32");
	SSL_CTX_set_tlsext_use_srtp(dtls->ssl_ctx, "SRTP_AES128_CM_SHA1_80");
#endif

	dtls->type = type;
	dtls->read_bio = BIO_new(BIO_s_mem());
	switch_assert(dtls->read_bio);

	dtls->write_bio = BIO_new(BIO_s_mem());
	switch_assert(dtls->write_bio);

	BIO_set_mem_eof_return(dtls->read_bio, -1);
	BIO_set_mem_eof_return(dtls->write_bio, -1);

	if ((ret=SSL_CTX_use_certificate_file(dtls->ssl_ctx, dtls->rsa, SSL_FILETYPE_PEM)) != 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS cert err [%d]\n", rtp_type(rtp_session), SSL_get_error(dtls->ssl, ret));
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	if ((ret=SSL_CTX_use_PrivateKey_file(dtls->ssl_ctx, dtls->pvt, SSL_FILETYPE_PEM)) != 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS key err [%d]\n", rtp_type(rtp_session), SSL_get_error(dtls->ssl, ret));
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	if (SSL_CTX_check_private_key(dtls->ssl_ctx) == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS check key failed\n", rtp_type(rtp_session));
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	if (!zstr(dtls->ca) && switch_file_exists(dtls->ca, rtp_session->pool) == SWITCH_STATUS_SUCCESS
		&& (ret = SSL_CTX_load_verify_locations(dtls->ssl_ctx, dtls->ca, NULL)) != 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "%s DTLS check chain cert failed [%d]\n",
						  rtp_type(rtp_session) ,
						  SSL_get_error(dtls->ssl, ret));
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	dtls->ssl = SSL_new(dtls->ssl_ctx);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	dtls->filter_bio = BIO_new(BIO_dtls_filter());
#else
	dtls_bio_filter_methods = BIO_meth_new(BIO_TYPE_FILTER | BIO_get_new_index(), "DTLS filter");
	BIO_meth_set_write(dtls_bio_filter_methods, dtls_bio_filter_write);
	BIO_meth_set_ctrl(dtls_bio_filter_methods, dtls_bio_filter_ctrl);
	BIO_meth_set_create(dtls_bio_filter_methods, dtls_bio_filter_new);
	BIO_meth_set_destroy(dtls_bio_filter_methods, dtls_bio_filter_free);
	dtls->filter_bio = BIO_new(dtls_bio_filter_methods);
#endif

	switch_assert(dtls->filter_bio);

	BIO_push(dtls->filter_bio, dtls->write_bio);

	SSL_set_bio(dtls->ssl, dtls->read_bio, dtls->filter_bio);

	SSL_set_mode(dtls->ssl, SSL_MODE_AUTO_RETRY);
	SSL_set_read_ahead(dtls->ssl, 1);


	//SSL_set_verify(dtls->ssl, (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT), cb_verify_peer);

#ifndef OPENSSL_NO_EC
	ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if (!ecdh) {
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	SSL_set_options(dtls->ssl, SSL_OP_SINGLE_ECDH_USE);
	SSL_set_tmp_ecdh(dtls->ssl, ecdh);
	EC_KEY_free(ecdh);
#endif

	SSL_set_verify(dtls->ssl, SSL_VERIFY_NONE, NULL);
	SSL_set_app_data(dtls->ssl, dtls);

	dtls->local_fp = local_fp;
	dtls->remote_fp = remote_fp;
	dtls->rtp_session = rtp_session;
	dtls->mtu = 1200;

	if (rtp_session->session) {
		switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
		if ((var = switch_channel_get_variable(channel, "rtp_dtls_mtu"))) {
			int mtu = atoi(var);

			if (mtu > 0 && mtu < MAX_DTLS_MTU) {
				dtls->mtu = mtu;
			}

		}
	}
	
	BIO_ctrl(dtls->filter_bio, BIO_CTRL_DGRAM_SET_MTU, dtls->mtu, NULL);
	
	switch_core_cert_expand_fingerprint(remote_fp, remote_fp->str);

	if ((type & DTLS_TYPE_RTP)) {
		rtp_session->dtls = dtls;
		dtls->sock_output = rtp_session->sock_output;
		dtls->remote_addr = rtp_session->remote_addr;
	}

	if ((type & DTLS_TYPE_RTCP)) {
		rtp_session->rtcp_dtls = dtls;
		if (!(type & DTLS_TYPE_RTP)) {
			dtls->sock_output = rtp_session->rtcp_sock_output;
			dtls->remote_addr = rtp_session->rtcp_remote_addr;
		}
	}

	if ((type & DTLS_TYPE_SERVER)) {
		SSL_set_accept_state(dtls->ssl);
	} else {
		SSL_set_connect_state(dtls->ssl);
	}

	dtls_set_state(dtls, DS_HANDSHAKE);

	rtp_session->flags[SWITCH_RTP_FLAG_VIDEO_BREAK] = 1;
	switch_rtp_break(rtp_session);

 done:

	switch_mutex_unlock(rtp_session->ice_mutex);

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_rtp_add_crypto_key(switch_rtp_t *rtp_session, switch_rtp_crypto_direction_t direction, uint32_t index, switch_secure_settings_t *ssec)
{
#ifndef ENABLE_SRTP
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_CRIT, "SRTP NOT SUPPORTED IN THIS BUILD!\n");
	return SWITCH_STATUS_FALSE;
#else

	switch_rtp_crypto_key_t *crypto_key;
	srtp_policy_t *policy;
	srtp_err_status_t stat;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
	switch_event_t *fsevent = NULL;
	int idx = 0;
	const char *var;
	unsigned char b64_key[512] = "";
	
	unsigned char	*keysalt = NULL;
	switch_size_t	keysalt_len = 0;
	
	switch_crypto_key_material_t	*key_material = NULL;
	unsigned long					*key_material_n = NULL;
	srtp_master_key_t		**mkis = NULL;
	srtp_master_key_t		*mki = NULL;
	int mki_idx = 0;

	keysalt_len = switch_core_media_crypto_keysalt_len(ssec->crypto_type);

	if (direction >= SWITCH_RTP_CRYPTO_MAX || keysalt_len > SWITCH_RTP_MAX_CRYPTO_LEN) {
		return SWITCH_STATUS_FALSE;
	}

	if (direction == SWITCH_RTP_CRYPTO_RECV_RTCP) {
		direction = SWITCH_RTP_CRYPTO_RECV;
		rtp_session->srtp_idx_rtcp = idx = 1;
	} else if (direction == SWITCH_RTP_CRYPTO_SEND_RTCP) {
		direction = SWITCH_RTP_CRYPTO_SEND;
		rtp_session->srtp_idx_rtcp = idx = 1;
	}

	if (direction == SWITCH_RTP_CRYPTO_RECV) {
		policy = &rtp_session->recv_policy[idx];
		keysalt = ssec->remote_raw_key;
		key_material = ssec->remote_key_material_next;
		key_material_n = &ssec->remote_key_material_n;
	} else {
		policy = &rtp_session->send_policy[idx];
		keysalt = ssec->local_raw_key;
		key_material = ssec->local_key_material_next;
		key_material_n = &ssec->local_key_material_n;
	}

	switch_b64_encode(keysalt, keysalt_len, b64_key, sizeof(b64_key));

	if (switch_true(switch_core_get_variable("rtp_retain_crypto_keys"))) {
		switch(direction) {
			case SWITCH_RTP_CRYPTO_SEND:
				switch_channel_set_variable(channel, "srtp_local_crypto_key", (const char *)b64_key);
				break;
			case SWITCH_RTP_CRYPTO_RECV:
				switch_channel_set_variable(channel, "srtp_remote_crypto_key", (const char *)b64_key);
				break;
			case SWITCH_RTP_CRYPTO_SEND_RTCP:
				switch_channel_set_variable(channel, "srtcp_local_crypto_key", (const char *)b64_key);
				break;
			case SWITCH_RTP_CRYPTO_RECV_RTCP:
				switch_channel_set_variable(channel, "srtcp_remote_crypto_key", (const char *)b64_key);
				break;
			default:
				break;
		}
	}

	crypto_key = switch_core_alloc(rtp_session->pool, sizeof(*crypto_key));

	crypto_key->type = ssec->crypto_type;
	crypto_key->index = index;
	memcpy(crypto_key->keysalt, keysalt, keysalt_len);
	crypto_key->next = rtp_session->crypto_keys[direction];
	rtp_session->crypto_keys[direction] = crypto_key;

	memset(policy, 0, sizeof(*policy));

	/* many devices can't handle gaps in SRTP streams */
	if (!((var = switch_channel_get_variable(channel, "srtp_allow_idle_gaps"))
		  && switch_true(var))
		&& (!(var = switch_channel_get_variable(channel, "send_silence_when_idle"))
			|| !(atoi(var)))) {
		switch_channel_set_variable(channel, "send_silence_when_idle", "-1");
	}

	switch (crypto_key->type) {
	case AES_CM_128_HMAC_SHA1_80:
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy->rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy->rtcp);

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "rtp_has_crypto", "AES_CM_128_HMAC_SHA1_80");
		}
		break;
	case AES_CM_128_HMAC_SHA1_32:
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy->rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy->rtcp);


		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "rtp_has_crypto", "AES_CM_128_HMAC_SHA1_32");
		}
		break;

	case AEAD_AES_256_GCM_8:
		srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy->rtp);
		srtp_crypto_policy_set_aes_gcm_256_8_auth(&policy->rtcp);

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "rtp_has_crypto", "AEAD_AES_256_GCM_8");
		}
		break;

	case AEAD_AES_128_GCM_8:
		srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy->rtp);
		srtp_crypto_policy_set_aes_gcm_128_8_auth(&policy->rtcp);

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "rtp_has_crypto", "AEAD_AES_128_GCM_8");
		}
		break;

	case AES_CM_256_HMAC_SHA1_80:
		srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy->rtp);
		srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy->rtcp);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "rtp_has_crypto", "AES_CM_256_HMAC_SHA1_80");
		}
		break;
	case AES_CM_256_HMAC_SHA1_32:
		srtp_crypto_policy_set_aes_cm_256_hmac_sha1_32(&policy->rtp);
		srtp_crypto_policy_set_aes_cm_256_hmac_sha1_32(&policy->rtcp);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "rtp_has_crypto", "AES_CM_256_HMAC_SHA1_32");
		}
		break;
	case AES_CM_128_NULL_AUTH:
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy->rtp);
		srtp_crypto_policy_set_aes_cm_128_null_auth(&policy->rtcp);

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "rtp_has_crypto", "AES_CM_128_NULL_AUTH");
		}
		break;
	default:
		break;
	}

	/* Setup the policy with MKI if they are used. Number of key materials must be positive to use MKI. */
	if (key_material && (*key_material_n > 0)) {

		if (direction == SWITCH_RTP_CRYPTO_RECV) {
			rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV_MKI] = 1;	/* tell the rest of the environment MKI is used */
		} else {
			rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_MKI] = 1;	/* tell the rest of the environment MKI is used */
		}

		/* key must be NULL for libsrtp to work correctly with MKI. */
		policy->key = NULL;

		/* Allocate array for MKIs. */
		mkis = switch_core_alloc(rtp_session->pool, *key_material_n * sizeof(srtp_master_key_t*));
		if (!mkis) {
			return SWITCH_STATUS_FALSE;
		}

		/* Build array of MKIs. */
		mki_idx = 0;

		while (key_material && (mki_idx < *key_material_n)) {

			if (key_material->mki_size < 1) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "MKI bad key size at MKI %d\n", mki_idx);
				return SWITCH_STATUS_FALSE;
			}

			mki = switch_core_alloc(rtp_session->pool, sizeof(srtp_master_key_t));
			if (!mki) {
				return SWITCH_STATUS_FALSE;
			}

			/* Setup MKI. */
			mki->mki_id = switch_core_alloc(rtp_session->pool, sizeof(key_material->mki_size));
			if (!mki->mki_id) {
				return SWITCH_STATUS_FALSE;
			}

			mki->key = switch_core_alloc(rtp_session->pool, keysalt_len);
			if (!mki->key) {
				return SWITCH_STATUS_FALSE;
			}

			memcpy(mki->mki_id, &key_material->mki_id, key_material->mki_size);
			mki->mki_size = key_material->mki_size;
			memcpy(mki->key, key_material->raw_key, keysalt_len);

			mkis[mki_idx] = mki;

			key_material = key_material->next;
			++mki_idx;
		}

		/* And pass the array of MKIs to libsrtp. */
		policy->keys = mkis;
		policy->num_master_keys = mki_idx;

	} else {
		policy->key = (uint8_t *) crypto_key->keysalt;
	}

	policy->next = NULL;

	policy->window_size = 1024;
	policy->allow_repeat_tx = 1;

	//policy->rtp.sec_serv = sec_serv_conf_and_auth;
	//policy->rtcp.sec_serv = sec_serv_conf_and_auth;

	switch (direction) {
	case SWITCH_RTP_CRYPTO_RECV:
		policy->ssrc.type = ssrc_any_inbound;

		if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] && idx == 0 && rtp_session->recv_ctx[idx]) {
			rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV_RESET] = 1;
		} else {
			if ((stat = srtp_create(&rtp_session->recv_ctx[idx], policy)) || !rtp_session->recv_ctx[idx]) {
				status = SWITCH_STATUS_FALSE;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO, "Activating %s Secure %s RECV%s\n",
								  rtp_type(rtp_session), idx ? "RTCP" : "RTP", rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV_MKI] ? " (with MKI)" : "");
				rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] = 1;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error allocating srtp [%d]\n", stat);
				return status;
			}
		}
		break;
	case SWITCH_RTP_CRYPTO_SEND:
		policy->ssrc.type = ssrc_any_outbound;
		//policy->ssrc.type = ssrc_specific;
		//policy->ssrc.value = rtp_session->ssrc;

		if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND] && idx == 0 && rtp_session->send_ctx[idx]) {
			rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_RESET] = 1;
		} else {
			if ((stat = srtp_create(&rtp_session->send_ctx[idx], policy)) || !rtp_session->send_ctx[idx]) {
				status = SWITCH_STATUS_FALSE;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO, "Activating %s Secure %s SEND%s\n",
								  rtp_type(rtp_session), idx ? "RTCP" : "RTP", rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_MKI] ? " (with MKI)" : "");
				rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND] = 1;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error allocating SRTP [%d]\n", stat);
				return status;
			}
		}

		break;
	default:
		abort();
		break;
	}

	if (switch_event_create(&fsevent, SWITCH_EVENT_CALL_SECURE) == SWITCH_STATUS_SUCCESS) {
		if (rtp_session->dtls) {
			switch_event_add_header(fsevent, SWITCH_STACK_BOTTOM, "secure_type", "srtp:dtls:AES_CM_128_HMAC_SHA1_80");
			switch_channel_set_variable(channel, "rtp_has_crypto", "srtp:dtls:AES_CM_128_HMAC_SHA1_80");
		} else {
			switch_event_add_header(fsevent, SWITCH_STACK_BOTTOM, "secure_type", "srtp:sdes:%s", switch_channel_get_variable(channel, "rtp_has_crypto"));
		}
		switch_event_add_header_string(fsevent, SWITCH_STACK_BOTTOM, "caller-unique-id", switch_channel_get_uuid(channel));
		switch_event_fire(&fsevent);
	}


	return SWITCH_STATUS_SUCCESS;
#endif
}

SWITCH_DECLARE(switch_status_t) switch_rtp_set_interval(switch_rtp_t *rtp_session, uint32_t ms_per_packet, uint32_t samples_per_interval)
{
	rtp_session->ms_per_packet = ms_per_packet;
	rtp_session->samples_per_interval = rtp_session->conf_samples_per_interval = samples_per_interval;
	rtp_session->missed_count = 0;
	rtp_session->samples_per_second =
		(uint32_t) ((double) (1000.0f / (double) (rtp_session->ms_per_packet / 1000)) * (double) rtp_session->samples_per_interval);

	rtp_session->one_second = (rtp_session->samples_per_second / rtp_session->samples_per_interval);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_change_interval(switch_rtp_t *rtp_session, uint32_t ms_per_packet, uint32_t samples_per_interval)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int change_timer = 0;

	if (rtp_session->ms_per_packet != ms_per_packet || rtp_session->samples_per_interval != samples_per_interval) {
		change_timer = 1;
	}

	switch_rtp_set_interval(rtp_session, ms_per_packet, samples_per_interval);

	if (change_timer && rtp_session->timer_name) {
		READ_INC(rtp_session);
		WRITE_INC(rtp_session);

		if (rtp_session->timer.timer_interface) {
			switch_core_timer_destroy(&rtp_session->timer);
		}

		if (rtp_session->write_timer.timer_interface) {
			switch_core_timer_destroy(&rtp_session->write_timer);
		}

		if ((status = switch_core_timer_init(&rtp_session->timer,
											 rtp_session->timer_name, ms_per_packet / 1000,
											 samples_per_interval, rtp_session->pool)) == SWITCH_STATUS_SUCCESS) {

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG,
							  "RE-Starting timer [%s] %d bytes per %dms\n", rtp_session->timer_name, samples_per_interval, ms_per_packet / 1000);
			switch_core_timer_init(&rtp_session->write_timer, rtp_session->timer_name, (ms_per_packet / 1000), samples_per_interval, rtp_session->pool);
		} else {

			memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR,
							  "Problem RE-Starting timer [%s] %d bytes per %dms\n", rtp_session->timer_name, samples_per_interval, ms_per_packet / 1000);
		}

		WRITE_DEC(rtp_session);
		READ_DEC(rtp_session);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_set_ssrc(switch_rtp_t *rtp_session, uint32_t ssrc)
{
	rtp_session->ssrc = ssrc;
	rtp_session->send_msg.header.ssrc = htonl(rtp_session->ssrc);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_set_remote_ssrc(switch_rtp_t *rtp_session, uint32_t ssrc)
{
	rtp_session->remote_ssrc = ssrc;
	rtp_session->flags[SWITCH_RTP_FLAG_DETECT_SSRC] = 0;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_create(switch_rtp_t **new_rtp_session,
												  switch_payload_t payload,
												  uint32_t samples_per_interval,
												  uint32_t ms_per_packet,
												  switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID], char *timer_name, const char **err, switch_memory_pool_t *pool)
{
	switch_rtp_t *rtp_session = NULL;
	switch_core_session_t *session = switch_core_memory_pool_get_data(pool, "__session");
	switch_channel_t *channel = NULL;

	if (session) channel = switch_core_session_get_channel(session);

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
	rtp_session->te = INVALID_PT;
	rtp_session->recv_te = INVALID_PT;
	rtp_session->cng_pt = INVALID_PT;
	rtp_session->session = session;

	switch_mutex_init(&rtp_session->flag_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->read_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->write_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->ice_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->dtmf_data.dtmf_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_queue_create(&rtp_session->dtmf_data.dtmf_queue, 100, rtp_session->pool);
	switch_queue_create(&rtp_session->dtmf_data.dtmf_inqueue, 100, rtp_session->pool);

	switch_rtp_set_flags(rtp_session, flags);

	/* for from address on recvfrom calls */
	switch_sockaddr_create(&rtp_session->from_addr, pool);
	switch_sockaddr_create(&rtp_session->rtp_from_addr, pool);

	if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
		switch_sockaddr_create(&rtp_session->rtcp_from_addr, pool);
	}
	rtp_session->seq = (uint16_t) rand();
	rtp_session->ssrc = (uint32_t) ((intptr_t) rtp_session + (uint32_t) switch_epoch_time_now(NULL));
#ifdef DEBUG_TS_ROLLOVER
	rtp_session->last_write_ts = TS_ROLLOVER_START;
#endif
	rtp_session->stats.inbound.R = 100.0;
	rtp_session->stats.inbound.mos = 4.5;
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
	rtp_session->rtcp_last_sent = switch_micro_time_now();

	switch_rtp_set_interval(rtp_session, ms_per_packet, samples_per_interval);
	rtp_session->conf_samples_per_interval = samples_per_interval;

	if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] && zstr(timer_name)) {
		timer_name = "soft";
	}

	if (!zstr(timer_name) && !strcasecmp(timer_name, "none")) {
		timer_name = NULL;
	}

	if (!zstr(timer_name)) {
		rtp_session->timer_name = switch_core_strdup(pool, timer_name);
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);

		if (switch_core_timer_init(&rtp_session->timer, timer_name, ms_per_packet / 1000, samples_per_interval, pool) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG,
							  "Starting timer [%s] %d bytes per %dms\n", timer_name, samples_per_interval, ms_per_packet / 1000);
			switch_core_timer_init(&rtp_session->write_timer, timer_name, (ms_per_packet / 1000), samples_per_interval, pool);
#ifdef DEBUG_TS_ROLLOVER
			rtp_session->timer.tick = TS_ROLLOVER_START / samples_per_interval;
#endif
		} else {
			memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR,
							  "Error Starting timer [%s] %d bytes per %dms, async RTP disabled\n", timer_name, samples_per_interval, ms_per_packet / 1000);
			switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		}
	} else {
		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
			if (switch_core_timer_init(&rtp_session->timer, "soft", 1, 90, pool) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Starting video timer.\n");
			}

			//switch_jb_create(&rtp_session->vb, 3, 10, rtp_session->pool);
			//switch_jb_debug_level(rtp_session->vb, 10);

		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Not using a timer\n");
		}

		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
	}


	if (channel) {
		switch_channel_set_private(channel, "__rtcp_audio_rtp_session", rtp_session);
	}

#ifdef ENABLE_ZRTP
	if (zrtp_on && session && channel && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA]) {
		switch_rtp_t *master_rtp_session = NULL;

		int initiator = 0;
		const char *zrtp_enabled = switch_channel_get_variable(channel, "zrtp_secure_media");
		int srtp_enabled = switch_channel_test_flag(channel, CF_SECURE);

		if (srtp_enabled && switch_true(zrtp_enabled)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING,
							  "You can not have ZRTP and SRTP enabled simultaneously, ZRTP will be disabled for this call!\n");
			switch_channel_set_variable(channel, "zrtp_secure_media", NULL);
			zrtp_enabled = NULL;
		}


		if (switch_true(zrtp_enabled)) {
			if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
				switch_channel_set_private(channel, "__zrtp_video_rtp_session", rtp_session);
				master_rtp_session = switch_channel_get_private(channel, "__zrtp_audio_rtp_session");
			} else {
				switch_channel_set_private(channel, "__zrtp_audio_rtp_session", rtp_session);
				master_rtp_session = rtp_session;
			}


			if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				initiator = 1;
			}

			if (rtp_session == master_rtp_session) {
				rtp_session->zrtp_profile = switch_core_alloc(rtp_session->pool, sizeof(*rtp_session->zrtp_profile));
				zrtp_profile_defaults(rtp_session->zrtp_profile, zrtp_global);

				rtp_session->zrtp_profile->allowclear = 0;
				rtp_session->zrtp_profile->disclose_bit = 0;
				rtp_session->zrtp_profile->cache_ttl = (uint32_t) -1;

				if (zrtp_status_ok != zrtp_session_init(zrtp_global, rtp_session->zrtp_profile, zid, initiator, &rtp_session->zrtp_session)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error! zRTP INIT Failed\n");
					zrtp_session_down(rtp_session->zrtp_session);
					rtp_session->zrtp_session = NULL;
					goto end;
				}

				zrtp_session_set_userdata(rtp_session->zrtp_session, session);


				if (zrtp_status_ok != zrtp_stream_attach(master_rtp_session->zrtp_session, &rtp_session->zrtp_stream)) {
					abort();
				}

				zrtp_stream_set_userdata(rtp_session->zrtp_stream, rtp_session);

				if (switch_true(switch_channel_get_variable(channel, "zrtp_enrollment"))) {
					zrtp_stream_registration_start(rtp_session->zrtp_stream, rtp_session->ssrc);
				} else {
					zrtp_stream_start(rtp_session->zrtp_stream, rtp_session->ssrc);
				}
			}

		}
	}

 end:

#endif

	/* Jitter */
	rtp_session->stats.inbound.last_proc_time = switch_micro_time_now() / 1000;
	rtp_session->stats.inbound.jitter_n = 0;
	rtp_session->stats.inbound.jitter_add = 0;
	rtp_session->stats.inbound.jitter_addsq = 0;
	rtp_session->stats.inbound.min_variance = 0;
	rtp_session->stats.inbound.max_variance = 0;

	/* Burst and Packet Loss */
	rtp_session->stats.inbound.lossrate = 0;
	rtp_session->stats.inbound.burstrate = 0;
	memset(rtp_session->stats.inbound.loss, 0, sizeof(rtp_session->stats.inbound.loss));
	rtp_session->stats.inbound.last_loss = 0;
	rtp_session->stats.inbound.last_processed_seq = -1;

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
											  switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID], char *timer_name, const char **err, switch_memory_pool_t *pool)
{
	switch_rtp_t *rtp_session = NULL;

	if (zstr(rx_host)) {
		*err = "Missing local host";
		goto end;
	}

	if (!rx_port) {
		*err = "Missing local port";
		goto end;
	}

	if (zstr(tx_host)) {
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

	if (switch_rtp_set_remote_address(rtp_session, tx_host, tx_port, 0, SWITCH_TRUE, err) != SWITCH_STATUS_SUCCESS) {
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
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH);
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_DETECT_SSRC);
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


SWITCH_DECLARE(void) switch_rtp_set_telephony_recv_event(switch_rtp_t *rtp_session, switch_payload_t te)
{
	if (te > 95) {
		rtp_session->recv_te = te;
	}
}


SWITCH_DECLARE(void) switch_rtp_set_cng_pt(switch_rtp_t *rtp_session, switch_payload_t pt)
{
	rtp_session->cng_pt = pt;
	rtp_session->flags[SWITCH_RTP_FLAG_AUTO_CNG] = 1;
}

SWITCH_DECLARE(switch_timer_t *) switch_rtp_get_media_timer(switch_rtp_t *rtp_session)
{

	if (rtp_session && rtp_session->timer.timer_interface) {
		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
			switch_core_timer_sync(&rtp_session->timer);
		}
		return &rtp_session->timer;
	}

	return NULL;
}


SWITCH_DECLARE(switch_jb_t *) switch_rtp_get_jitter_buffer(switch_rtp_t *rtp_session)
{
	if (!switch_rtp_ready(rtp_session)) {
		return NULL;
	}

	return rtp_session->jb ? rtp_session->jb : rtp_session->vb;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_pause_jitter_buffer(switch_rtp_t *rtp_session, switch_bool_t pause)
{
	int new_val;

	if (rtp_session->pause_jb && !pause) {
		if (rtp_session->jb) {
			switch_jb_reset(rtp_session->jb);
		}

		if (rtp_session->vb) {
			switch_jb_reset(rtp_session->vb);
		}
	}

	new_val = pause ? 1 : -1;

	if (rtp_session->pause_jb + new_val > -1) {
		rtp_session->pause_jb += new_val;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1,
					  "Jitterbuffer %s is %s\n", rtp_type(rtp_session), rtp_session->pause_jb ? "paused" : "enabled");
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_deactivate_jitter_buffer(switch_rtp_t *rtp_session)
{

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	rtp_session->flags[SWITCH_RTP_FLAG_KILL_JB]++;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_get_video_buffer_size(switch_rtp_t *rtp_session, uint32_t *min_frame_len, uint32_t *max_frame_len, uint32_t *cur_frame_len, uint32_t *highest_frame_len)
{

	if (rtp_session->vb) {
		return switch_jb_get_frames(rtp_session->vb, min_frame_len, max_frame_len, cur_frame_len, highest_frame_len);
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_set_video_buffer_size(switch_rtp_t *rtp_session, uint32_t frames, uint32_t max_frames)
{
	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!max_frames) {
		max_frames = rtp_session->last_max_vb_frames;
	}

	if (!max_frames || frames >= max_frames) {
		max_frames = frames * 10;
	}

	rtp_session->last_max_vb_frames = max_frames;

	if (!rtp_session->vb) {
		switch_jb_create(&rtp_session->vb, rtp_session->flags[SWITCH_RTP_FLAG_TEXT] ? SJB_TEXT : SJB_VIDEO, frames, max_frames, rtp_session->pool);
		switch_jb_set_session(rtp_session->vb, rtp_session->session);
	} else {
		switch_jb_set_frames(rtp_session->vb, frames, max_frames);
	}

	//switch_rtp_flush(rtp_session);
	switch_core_session_request_video_refresh(rtp_session->session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Setting video buffer %u Frames.\n", frames);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_debug_jitter_buffer(switch_rtp_t *rtp_session, const char *name)
{
	int x = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (name) x = atoi(name);
	if (x < 0) x = 0;

	if (rtp_session->jb) {
		switch_jb_debug_level(rtp_session->jb, x);
	} else if (rtp_session->vb) {
		switch_jb_debug_level(rtp_session->vb, x);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_jitter_buffer(switch_rtp_t *rtp_session,
																  uint32_t queue_frames,
																  uint32_t max_queue_frames,
																  uint32_t samples_per_packet,
																  uint32_t samples_per_second)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (queue_frames < 1) {
		queue_frames = 3;
	}

	if (max_queue_frames < queue_frames) {
		max_queue_frames = queue_frames * 3;
	}



	if (rtp_session->jb) {
		status = switch_jb_set_frames(rtp_session->jb, queue_frames, max_queue_frames);
	} else {
		READ_INC(rtp_session);
		status = switch_jb_create(&rtp_session->jb, SJB_AUDIO, queue_frames, max_queue_frames, rtp_session->pool);
		switch_jb_set_session(rtp_session->jb, rtp_session->session);
		if (switch_true(switch_channel_get_variable_dup(switch_core_session_get_channel(rtp_session->session), "jb_use_timestamps", SWITCH_FALSE, -1))) {
			switch_jb_ts_mode(rtp_session->jb, samples_per_packet, samples_per_second);
		}
		//switch_jb_debug_level(rtp_session->jb, 10);
		READ_DEC(rtp_session);
	}



	return status;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_rtcp(switch_rtp_t *rtp_session, int send_rate, switch_port_t remote_port, switch_bool_t mux)
{
	const char *err = NULL;

	if (!rtp_session->ms_per_packet) {
		return SWITCH_STATUS_FALSE;
	}

	rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP] = 1;

	if (!(rtp_session->remote_rtcp_port = remote_port)) {
		rtp_session->remote_rtcp_port = rtp_session->remote_port + 1;
	}

	if (mux) {
		rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]++;
	}


	if (send_rate == -1) {

		rtp_session->flags[SWITCH_RTP_FLAG_RTCP_PASSTHRU] = 1;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "RTCP passthru enabled. Remote Port: %d\n", rtp_session->remote_rtcp_port);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "RTCP send rate is: %d and packet rate is: %d Remote Port: %d\n", 						  send_rate, rtp_session->ms_per_packet, rtp_session->remote_rtcp_port);

		rtp_session->rtcp_interval = send_rate;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {

		if (switch_sockaddr_info_get(&rtp_session->rtcp_remote_addr, rtp_session->eff_remote_host_str, SWITCH_UNSPEC,
									 rtp_session->remote_rtcp_port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS || !rtp_session->rtcp_remote_addr) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "RTCP MUX Remote Address Error!");
			return SWITCH_STATUS_FALSE;
		}

		rtp_session->rtcp_local_addr = rtp_session->local_addr;
		rtp_session->rtcp_from_addr = rtp_session->from_addr;
		rtp_session->rtcp_sock_input = rtp_session->sock_input;
		rtp_session->rtcp_sock_output = rtp_session->sock_output;

		rtp_session->rtcp_recv_msg_p = (rtcp_msg_t *) &rtp_session->recv_msg;

		return SWITCH_STATUS_SUCCESS;

		//return enable_remote_rtcp_socket(rtp_session, &err);
	} else {
		rtp_session->rtcp_recv_msg_p = (rtcp_msg_t *) &rtp_session->rtcp_recv_msg;
	}

	return enable_local_rtcp_socket(rtp_session, &err) || enable_remote_rtcp_socket(rtp_session, &err);

}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_ice(switch_rtp_t *rtp_session, char *login, char *rlogin,
														const char *password, const char *rpassword, ice_proto_t proto,
														switch_core_media_ice_type_t type, ice_t *ice_params)
{
	char ice_user[80];
	char user_ice[80];
	char luser_ice[80];
	switch_rtp_ice_t *ice;
	char *host = NULL;
	switch_port_t port = 0;
	char bufc[50];


	switch_mutex_lock(rtp_session->ice_mutex);

	if (proto == IPR_RTP) {
		ice = &rtp_session->ice;
		rtp_session->flags[SWITCH_RTP_FLAG_PAUSE] = 0;
		rtp_session->flags[SWITCH_RTP_FLAG_MUTE] = 0;
	} else {
		ice = &rtp_session->rtcp_ice;
	}

	ice->proto = proto;

	if ((type & ICE_VANILLA)) {
		switch_snprintf(ice_user, sizeof(ice_user), "%s:%s", login, rlogin);
		switch_snprintf(user_ice, sizeof(user_ice), "%s:%s", rlogin, login);
		switch_snprintf(luser_ice, sizeof(user_ice), "%s%s", rlogin, login);
		ice->ready = ice->rready = 0;
	} else {
		switch_snprintf(ice_user, sizeof(ice_user), "%s%s", login, rlogin);
		switch_snprintf(user_ice, sizeof(user_ice), "%s%s", rlogin, login);
		ice->ready = ice->rready = 1;
	}

	ice->ice_user = switch_core_strdup(rtp_session->pool, ice_user);
	ice->user_ice = switch_core_strdup(rtp_session->pool, user_ice);
	ice->luser_ice = switch_core_strdup(rtp_session->pool, luser_ice);
	ice->type = type;
	ice->ice_params = ice_params;
	ice->pass = "";
	ice->rpass = "";
	ice->next_run = switch_micro_time_now();

	if (password) {
		ice->pass = switch_core_strdup(rtp_session->pool, password);
	}

	if (rpassword) {
		ice->rpass = switch_core_strdup(rtp_session->pool, rpassword);
	}

	if ((ice->type & ICE_VANILLA) && ice->ice_params) {
		host = ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].con_addr;
		port = ice->ice_params->cands[ice->ice_params->chosen[ice->proto]][ice->proto].con_port;

		if (!host || !port || switch_sockaddr_info_get(&ice->addr, host, SWITCH_UNSPEC, port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS || !ice->addr) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error setting remote host!\n");
			return SWITCH_STATUS_FALSE;
		}
	} else {
		if (proto == IPR_RTP) {
			ice->addr = rtp_session->remote_addr;
		} else {
			ice->addr = rtp_session->rtcp_remote_addr;
		}

		host = (char *)switch_get_addr(bufc, sizeof(bufc), ice->addr);
		port = switch_sockaddr_get_port(ice->addr);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_NOTICE, "Activating %s %s ICE: %s %s:%d\n",
					  proto == IPR_RTP ? "RTP" : "RTCP", rtp_type(rtp_session), ice_user, host, port);


	rtp_session->rtp_bugs |= RTP_BUG_ACCEPT_ANY_PACKETS;


	if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
		rtp_session->flags[SWITCH_RTP_FLAG_VIDEO_BREAK] = 1;
		switch_rtp_break(rtp_session);
	}

	switch_mutex_unlock(rtp_session->ice_mutex);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(void) switch_rtp_flush(switch_rtp_t *rtp_session)
{
	if (!switch_rtp_ready(rtp_session)) {
		return;
	}

	switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH);
}

SWITCH_DECLARE(switch_status_t) switch_rtp_req_bitrate(switch_rtp_t *rtp_session, uint32_t bps)
{
	if (!rtp_write_ready(rtp_session, 0, __LINE__) || rtp_session->tmmbr) {
		return SWITCH_STATUS_FALSE;
	}

	rtp_session->tmmbr = bps;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_ack_bitrate(switch_rtp_t *rtp_session, uint32_t bps)
{
	if (!rtp_write_ready(rtp_session, 0, __LINE__) || rtp_session->tmmbn) {
		return SWITCH_STATUS_FALSE;
	}

	rtp_session->tmmbn = bps;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_rtp_video_refresh(switch_rtp_t *rtp_session)
{

	if (!rtp_write_ready(rtp_session, 0, __LINE__)) {
		return;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && (rtp_session->ice.ice_user || rtp_session->flags[SWITCH_RTP_FLAG_FIR] || rtp_session->flags[SWITCH_RTP_FLAG_OLD_FIR])) {
		rtp_session->fir_count = 1;
	}
}

SWITCH_DECLARE(void) switch_rtp_video_loss(switch_rtp_t *rtp_session)
{

	if (!rtp_write_ready(rtp_session, 0, __LINE__)) {
		return;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && (rtp_session->ice.ice_user || rtp_session->flags[SWITCH_RTP_FLAG_PLI])) {
		rtp_session->pli_count = 1;
	}
}

SWITCH_DECLARE(void) switch_rtp_break(switch_rtp_t *rtp_session)
{
	if (!switch_rtp_ready(rtp_session)) {
		return;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
		int ret = 1;

		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO_BREAK]) {
			rtp_session->flags[SWITCH_RTP_FLAG_VIDEO_BREAK] = 0;
			ret = 0;
		} else if (rtp_session->session) {
			switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
			if (switch_channel_test_flag(channel, CF_VIDEO_BREAK)) {
				switch_channel_clear_flag(channel, CF_VIDEO_BREAK);
				ret = 0;
			}
		}

		if (ret) return;

		switch_rtp_video_refresh(rtp_session);
	}

	switch_mutex_lock(rtp_session->flag_mutex);
	rtp_session->flags[SWITCH_RTP_FLAG_BREAK] = 1;

	if (rtp_session->flags[SWITCH_RTP_FLAG_NOBLOCK]) {
		switch_mutex_unlock(rtp_session->flag_mutex);
		return;
	}

	if (rtp_session->sock_input) {
		ping_socket(rtp_session);
	}

	switch_mutex_unlock(rtp_session->flag_mutex);
}

SWITCH_DECLARE(void) switch_rtp_kill_socket(switch_rtp_t *rtp_session)
{
	switch_assert(rtp_session != NULL);
	switch_mutex_lock(rtp_session->flag_mutex);
	if (rtp_session->flags[SWITCH_RTP_FLAG_IO]) {
		rtp_session->flags[SWITCH_RTP_FLAG_IO] = 0;
		if (rtp_session->sock_input) {
			ping_socket(rtp_session);
			switch_socket_shutdown(rtp_session->sock_input, SWITCH_SHUTDOWN_READWRITE);
		}
		if (rtp_session->sock_output && rtp_session->sock_output != rtp_session->sock_input) {
			switch_socket_shutdown(rtp_session->sock_output, SWITCH_SHUTDOWN_READWRITE);
		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
			if (rtp_session->rtcp_sock_input) {
				ping_socket(rtp_session);
				switch_socket_shutdown(rtp_session->rtcp_sock_input, SWITCH_SHUTDOWN_READWRITE);
			}
			if (rtp_session->rtcp_sock_output && rtp_session->rtcp_sock_output != rtp_session->rtcp_sock_input) {
				switch_socket_shutdown(rtp_session->rtcp_sock_output, SWITCH_SHUTDOWN_READWRITE);
			}
		}
	}
	switch_mutex_unlock(rtp_session->flag_mutex);
}

SWITCH_DECLARE(uint8_t) switch_rtp_ready(switch_rtp_t *rtp_session)
{
	uint8_t ret;

	if (!rtp_session || !rtp_session->flag_mutex || rtp_session->flags[SWITCH_RTP_FLAG_SHUTDOWN]) {
		return 0;
	}

	switch_mutex_lock(rtp_session->flag_mutex);
	ret = (rtp_session->flags[SWITCH_RTP_FLAG_IO] && rtp_session->sock_input && rtp_session->sock_output && rtp_session->remote_addr
		   && rtp_session->ready == 2) ? 1 : 0;
	switch_mutex_unlock(rtp_session->flag_mutex);

	return ret;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_sync_stats(switch_rtp_t *rtp_session)
{
	if (!rtp_session) {
		return SWITCH_STATUS_FALSE;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_VAD]) {
		switch_channel_t *channel = switch_core_session_get_channel(rtp_session->vad_data.session);

		switch_channel_set_variable_printf(channel, "vad_total_talk_time_ms", "%u", (uint32_t)rtp_session->vad_data.total_talk_time / 1000);
		switch_channel_set_variable_printf(channel, "vad_total_talk_time_sec", "%u", (uint32_t)rtp_session->vad_data.total_talk_time / 1000000);
	}

	do_mos(rtp_session, SWITCH_TRUE);

	if (rtp_session->stats.inbound.error_log && !rtp_session->stats.inbound.error_log->stop) {
		rtp_session->stats.inbound.error_log->stop = switch_micro_time_now();
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(void) switch_rtp_destroy(switch_rtp_t **rtp_session)
{
	void *pop;
	switch_socket_t *sock;
#ifdef ENABLE_SRTP
	int x;
#endif

	if (!rtp_session || !*rtp_session || !(*rtp_session)->ready) {
		return;
	}

	(*rtp_session)->flags[SWITCH_RTP_FLAG_SHUTDOWN] = 1;

	READ_INC((*rtp_session));
	WRITE_INC((*rtp_session));

	(*rtp_session)->ready = 0;

	WRITE_DEC((*rtp_session));
	READ_DEC((*rtp_session));

	if ((*rtp_session)->flags[SWITCH_RTP_FLAG_VAD]) {
		switch_rtp_disable_vad(*rtp_session);
	}

	switch_mutex_lock((*rtp_session)->flag_mutex);

	switch_rtp_kill_socket(*rtp_session);

	while (switch_queue_trypop((*rtp_session)->dtmf_data.dtmf_inqueue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_safe_free(pop);
	}

	while (switch_queue_trypop((*rtp_session)->dtmf_data.dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		switch_safe_free(pop);
	}

	if ((*rtp_session)->jb) {
		switch_jb_destroy(&(*rtp_session)->jb);
	}

	if ((*rtp_session)->vb) {
		switch_jb_destroy(&(*rtp_session)->vb);
	}

	if ((*rtp_session)->vbw) {
		switch_jb_destroy(&(*rtp_session)->vbw);
	}

	if ((*rtp_session)->dtls && (*rtp_session)->dtls == (*rtp_session)->rtcp_dtls) {
		(*rtp_session)->rtcp_dtls = NULL;
	}

	if ((*rtp_session)->dtls) {
		free_dtls(&(*rtp_session)->dtls);
	}

	if ((*rtp_session)->rtcp_dtls) {
		free_dtls(&(*rtp_session)->rtcp_dtls);
	}


	sock = (*rtp_session)->sock_input;
	(*rtp_session)->sock_input = NULL;
	switch_socket_close(sock);

	if ((*rtp_session)->sock_output != sock) {
		sock = (*rtp_session)->sock_output;
		(*rtp_session)->sock_output = NULL;
		switch_socket_close(sock);
	}

	if ((sock = (*rtp_session)->rtcp_sock_input)) {
		(*rtp_session)->rtcp_sock_input = NULL;
		switch_socket_close(sock);

		if ((*rtp_session)->rtcp_sock_output && (*rtp_session)->rtcp_sock_output != sock) {
			if ((sock = (*rtp_session)->rtcp_sock_output)) {
				(*rtp_session)->rtcp_sock_output = NULL;
				switch_socket_close(sock);
			}
		}
	}

#ifdef ENABLE_SRTP
	if ((*rtp_session)->flags[SWITCH_RTP_FLAG_SECURE_SEND]) {
		for(x = 0; x < 2; x++) {
			if ((*rtp_session)->send_ctx[x]) {
				srtp_dealloc((*rtp_session)->send_ctx[x]);
				(*rtp_session)->send_ctx[x] = NULL;
			}
		}
		(*rtp_session)->flags[SWITCH_RTP_FLAG_SECURE_SEND] = 0;
	}

	if ((*rtp_session)->flags[SWITCH_RTP_FLAG_SECURE_RECV]) {
		for (x = 0; x < 2; x++) {
			if ((*rtp_session)->recv_ctx[x]) {
				srtp_dealloc((*rtp_session)->recv_ctx[x]);
				(*rtp_session)->recv_ctx[x] = NULL;
			}
		}
		(*rtp_session)->flags[SWITCH_RTP_FLAG_SECURE_RECV] = 0;
	}
#endif

#ifdef ENABLE_ZRTP
	/* ZRTP */
	if (zrtp_on && !(*rtp_session)->flags[SWITCH_RTP_FLAG_PROXY_MEDIA]) {

		if ((*rtp_session)->zrtp_stream != NULL) {
			zrtp_stream_stop((*rtp_session)->zrtp_stream);
		}

		if ((*rtp_session)->flags[SWITCH_ZRTP_FLAG_SECURE_SEND]) {
			(*rtp_session)->flags[SWITCH_ZRTP_FLAG_SECURE_SEND] = 0;
		}

		if ((*rtp_session)->flags[SWITCH_ZRTP_FLAG_SECURE_RECV]) {
			(*rtp_session)->flags[SWITCH_ZRTP_FLAG_SECURE_RECV] = 0;
		}

		if ((*rtp_session)->zrtp_session) {
			zrtp_session_down((*rtp_session)->zrtp_session);
			(*rtp_session)->zrtp_session = NULL;
		}
	}
#endif
	if ((*rtp_session)->timer.timer_interface) {
		switch_core_timer_destroy(&(*rtp_session)->timer);
	}

	if ((*rtp_session)->write_timer.timer_interface) {
		switch_core_timer_destroy(&(*rtp_session)->write_timer);
	}

	switch_rtp_release_port((*rtp_session)->rx_host, (*rtp_session)->rx_port);
	switch_mutex_unlock((*rtp_session)->flag_mutex);

	return;
}

SWITCH_DECLARE(void) switch_rtp_set_interdigit_delay(switch_rtp_t *rtp_session, uint32_t delay)
{
	rtp_session->interdigit_delay = delay;
}

SWITCH_DECLARE(switch_socket_t *) switch_rtp_get_rtp_socket(switch_rtp_t *rtp_session)
{
	return rtp_session->sock_input;
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

SWITCH_DECLARE(void) switch_rtp_set_invalid_handler(switch_rtp_t *rtp_session, switch_rtp_invalid_handler_t on_invalid)
{
	rtp_session->invalid_handler = on_invalid;
}

SWITCH_DECLARE(void) switch_rtp_set_flags(switch_rtp_t *rtp_session, switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID])
{
	int i;

	for(i = 0; i < SWITCH_RTP_FLAG_INVALID; i++) {
		if (flags[i]) {
			switch_rtp_set_flag(rtp_session, i);
		}
	}
}

SWITCH_DECLARE(void) switch_rtp_clear_flags(switch_rtp_t *rtp_session, switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID])
{
	int i;

	for(i = 0; i < SWITCH_RTP_FLAG_INVALID; i++) {
		if (flags[i]) {
			switch_rtp_clear_flag(rtp_session, i);
		}
	}
}

SWITCH_DECLARE(void) switch_rtp_set_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flag)
{
	int old_flag = rtp_session->flags[flag];

	switch_mutex_lock(rtp_session->flag_mutex);
	rtp_session->flags[flag] = 1;
	switch_mutex_unlock(rtp_session->flag_mutex);

	if (flag == SWITCH_RTP_FLAG_PASSTHRU) {
		if (!old_flag) {
			switch_rtp_pause_jitter_buffer(rtp_session, SWITCH_TRUE);
		}
	} else if (flag == SWITCH_RTP_FLAG_DTMF_ON) {
		rtp_session->stats.inbound.last_processed_seq = 0;
	} else if (flag == SWITCH_RTP_FLAG_FLUSH) {
		reset_jitter_seq(rtp_session);
	} else if (flag == SWITCH_RTP_FLAG_AUTOADJ) {
		rtp_session->autoadj_window = 20;
		rtp_session->autoadj_threshold = 10;
		rtp_session->autoadj_tally = 0;

		switch_mutex_lock(rtp_session->flag_mutex);
		rtp_session->flags[SWITCH_RTP_FLAG_RTCP_AUTOADJ] = 1;
		switch_mutex_unlock(rtp_session->flag_mutex);

		rtp_session->rtcp_autoadj_window = 20;
		rtp_session->rtcp_autoadj_threshold = 1;
		rtp_session->rtcp_autoadj_tally = 0;

		if (rtp_session->session) {
			switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
			const char *x = switch_channel_get_variable(channel, "rtp_auto_adjust_threshold");
			if (x && *x) {
				int xn = atoi(x);
				if (xn > 0 && xn <= 65535) {
					rtp_session->autoadj_window = xn*2;
					rtp_session->autoadj_threshold = xn;
				}
			}
		}


		rtp_flush_read_buffer(rtp_session, SWITCH_RTP_FLUSH_ONCE);


		if (rtp_session->jb) {
			switch_jb_reset(rtp_session->jb);
		}
	} else if (flag == SWITCH_RTP_FLAG_NOBLOCK && rtp_session->sock_input) {
		switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, TRUE);
	}

}

SWITCH_DECLARE(uint32_t) switch_rtp_test_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags)
{
	return (uint32_t) rtp_session->flags[flags];
}

SWITCH_DECLARE(void) switch_rtp_clear_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flag)
{
	int old_flag = rtp_session->flags[flag];

	switch_mutex_lock(rtp_session->flag_mutex);
	rtp_session->flags[flag] = 0;
	switch_mutex_unlock(rtp_session->flag_mutex);

	if (flag == SWITCH_RTP_FLAG_PASSTHRU) {
		if (old_flag) {
			switch_rtp_pause_jitter_buffer(rtp_session, SWITCH_FALSE);
		}
	} else if (flag == SWITCH_RTP_FLAG_DTMF_ON) {
		rtp_session->stats.inbound.last_processed_seq = 0;
	} else if (flag == SWITCH_RTP_FLAG_PAUSE) {
		reset_jitter_seq(rtp_session);
	} else if (flag == SWITCH_RTP_FLAG_NOBLOCK && rtp_session->sock_input) {
		switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, FALSE);
	}
}

static void set_dtmf_delay(switch_rtp_t *rtp_session, uint32_t ms, uint32_t max_ms)
{
	int upsamp, max_upsamp;


	if (!max_ms) max_ms = ms;

	upsamp = ms * (rtp_session->samples_per_second / 1000);
	max_upsamp = max_ms * (rtp_session->samples_per_second / 1000);

	rtp_session->queue_delay = upsamp;

	if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
		rtp_session->max_next_write_samplecount = rtp_session->timer.samplecount + max_upsamp;
		rtp_session->next_write_samplecount = rtp_session->timer.samplecount + upsamp;
		rtp_session->last_write_ts += upsamp;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Queue digit delay of %dms\n", ms);
}

static void do_2833(switch_rtp_t *rtp_session)
{
	switch_frame_flag_t flags = 0;
	uint32_t samples = rtp_session->samples_per_interval;

	if (rtp_session->dtmf_data.out_digit_dur > 0) {
		int x, loops = 1;

		if (!rtp_session->last_write_ts) {
			if (rtp_session->timer.timer_interface) {
				//switch_core_timer_sync(&rtp_session->write_timer);
				rtp_session->last_write_ts = rtp_session->write_timer.samplecount;
			} else {
				rtp_session->last_write_ts = rtp_session->samples_per_interval;
			}
		}

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
			switch_size_t wrote = switch_rtp_write_manual(rtp_session,
														  rtp_session->dtmf_data.out_digit_packet, 4, 0,
														  rtp_session->te, rtp_session->dtmf_data.timestamp_dtmf, &flags);

			rtp_session->stats.outbound.raw_bytes += wrote;
			rtp_session->stats.outbound.dtmf_packet_count++;

			if (loops == 1) {
				rtp_session->last_write_ts += samples;

				if (rtp_session->rtp_bugs & RTP_BUG_SONUS_SEND_INVALID_TIMESTAMP_2833) {
					rtp_session->dtmf_data.timestamp_dtmf = rtp_session->last_write_ts;
				}
			}

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Send %s packet for [%c] ts=%u dur=%d/%d/%d seq=%d lw=%u\n",
							  loops == 1 ? "middle" : "end", rtp_session->dtmf_data.out_digit,
							  rtp_session->dtmf_data.timestamp_dtmf,
							  rtp_session->dtmf_data.out_digit_sofar,
							  rtp_session->dtmf_data.out_digit_sub_sofar, rtp_session->dtmf_data.out_digit_dur, rtp_session->seq, rtp_session->last_write_ts);
		}

		if (loops != 1) {
			rtp_session->sending_dtmf = 0;
			rtp_session->need_mark = 1;

			if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
				//switch_core_timer_sync(&rtp_session->write_timer);
				rtp_session->last_write_samplecount = rtp_session->write_timer.samplecount;
			}

			rtp_session->dtmf_data.out_digit_dur = 0;

			if (rtp_session->interdigit_delay) {
				set_dtmf_delay(rtp_session, rtp_session->interdigit_delay, rtp_session->interdigit_delay * 10);
			}

			return;
		}
	}

	if (!rtp_session->dtmf_data.out_digit_dur && rtp_session->dtmf_data.dtmf_queue && switch_queue_size(rtp_session->dtmf_data.dtmf_queue)) {
		void *pop;

		if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
			//switch_core_timer_sync(&rtp_session->write_timer);
			if (rtp_session->timer.samplecount < rtp_session->next_write_samplecount) {
				return;
			}

			if (rtp_session->timer.samplecount >= rtp_session->max_next_write_samplecount) {
				rtp_session->queue_delay = 0;
			}

		} else if (rtp_session->queue_delay) {
			if (rtp_session->delay_samples >= rtp_session->samples_per_interval) {
				rtp_session->delay_samples -= rtp_session->samples_per_interval;
			} else {
				rtp_session->delay_samples = 0;
			}

			if (!rtp_session->delay_samples) {
				rtp_session->queue_delay = 0;
			}
		}

		if (rtp_session->queue_delay) {
			return;
		}


		if (!rtp_session->sending_dtmf) {
			rtp_session->sending_dtmf = 1;
		}

		if (switch_queue_trypop(rtp_session->dtmf_data.dtmf_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_dtmf_t *rdigit = pop;
			switch_size_t wrote;

			if (rdigit->digit == 'w') {
				set_dtmf_delay(rtp_session, 500, 0);
				free(rdigit);
				return;
			}

			if (rdigit->digit == 'W') {
				set_dtmf_delay(rtp_session, 1000, 0);
				free(rdigit);
				return;
			}

			memset(rtp_session->dtmf_data.out_digit_packet, 0, 4);
			rtp_session->dtmf_data.out_digit_sofar = samples;
			rtp_session->dtmf_data.out_digit_sub_sofar = samples;
			rtp_session->dtmf_data.out_digit_dur = rdigit->duration;
			rtp_session->dtmf_data.out_digit = rdigit->digit;
			rtp_session->dtmf_data.out_digit_packet[0] = (unsigned char) switch_char_to_rfc2833(rdigit->digit);
			rtp_session->dtmf_data.out_digit_packet[1] = 13;
			rtp_session->dtmf_data.out_digit_packet[2] = (unsigned char) (rtp_session->dtmf_data.out_digit_sub_sofar >> 8);
			rtp_session->dtmf_data.out_digit_packet[3] = (unsigned char) rtp_session->dtmf_data.out_digit_sub_sofar;


			rtp_session->dtmf_data.timestamp_dtmf = rtp_session->last_write_ts + samples;
			rtp_session->last_write_ts = rtp_session->dtmf_data.timestamp_dtmf;
			rtp_session->flags[SWITCH_RTP_FLAG_RESET] = 0;

			wrote = switch_rtp_write_manual(rtp_session,
											rtp_session->dtmf_data.out_digit_packet,
											4,
											rtp_session->rtp_bugs & RTP_BUG_CISCO_SKIP_MARK_BIT_2833 ? 0 : 1,
											rtp_session->te, rtp_session->dtmf_data.timestamp_dtmf, &flags);


			rtp_session->stats.outbound.raw_bytes += wrote;
			rtp_session->stats.outbound.dtmf_packet_count++;

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Send start packet for [%c] ts=%u dur=%d/%d/%d seq=%d lw=%u\n",
							  rtp_session->dtmf_data.out_digit,
							  rtp_session->dtmf_data.timestamp_dtmf,
							  rtp_session->dtmf_data.out_digit_sofar,
							  rtp_session->dtmf_data.out_digit_sub_sofar, rtp_session->dtmf_data.out_digit_dur, rtp_session->seq, rtp_session->last_write_ts);

			free(rdigit);
		}
	}
}

SWITCH_DECLARE(void) rtp_flush_read_buffer(switch_rtp_t *rtp_session, switch_rtp_flush_t flush)
{

	if (rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] ||
		rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
		return;
	}


	if (switch_rtp_ready(rtp_session)) {
		rtp_session->flags[SWITCH_RTP_FLAG_RESET] = 1;
		rtp_session->flags[SWITCH_RTP_FLAG_FLUSH] = 1;
		reset_jitter_seq(rtp_session);

		switch (flush) {
		case SWITCH_RTP_FLUSH_STICK:
			switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_STICKY_FLUSH);
			break;
		case SWITCH_RTP_FLUSH_UNSTICK:
			switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_STICKY_FLUSH);
			break;
		default:
				break;
		}
	}
}

static int jb_valid(switch_rtp_t *rtp_session)
{
	if (rtp_session->ice.ice_user) {
		if (!rtp_session->ice.ready && rtp_session->ice.rready) {
			return 0;
		}
	}

	if (rtp_session->dtls && rtp_session->dtls->state != DS_READY) {
		return 0;
	}

	return 1;
}


static switch_size_t do_flush(switch_rtp_t *rtp_session, int force, switch_size_t bytes_in)
{
	int was_blocking = 0;
	switch_size_t bytes;
	uint32_t flushed = 0;
	switch_size_t bytes_out = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return 0;
	}

	reset_jitter_seq(rtp_session);

	if (!force) {
		if ((rtp_session->jb && !rtp_session->pause_jb && jb_valid(rtp_session)) ||
			rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] ||
			rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] ||
			rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON]
			) {
			return bytes_in;
		}
	}

	READ_INC(rtp_session);

	if (switch_rtp_ready(rtp_session) ) {

		if (rtp_session->jb && !rtp_session->pause_jb && jb_valid(rtp_session)) {
			//switch_jb_reset(rtp_session->jb);
			bytes_out = bytes_in;
			goto end;
		}

		if (rtp_session->vbw) {
			switch_jb_reset(rtp_session->vbw);
		}

		if (rtp_session->vb) {
			//switch_jb_reset(rtp_session->vb);
			bytes_out = bytes_in;
			goto end;
		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_READ]) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session),
							  SWITCH_LOG_CONSOLE, "%s FLUSH\n",
							  rtp_session->session ? switch_channel_get_name(switch_core_session_get_channel(rtp_session->session)) : "NoName"
							  );
		}

		if (!rtp_session->flags[SWITCH_RTP_FLAG_NOBLOCK]) {
			was_blocking = 1;
			switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
			switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, TRUE);
		}

		// before processing/flushing packets, if current packet is rfc2833, handle it (else it would be lost)
		if (bytes_in > rtp_header_len && rtp_session->last_rtp_hdr.version == 2 && rtp_session->last_rtp_hdr.pt == rtp_session->recv_te) {
		    int do_cng = 0;
#ifdef DEBUG_2833
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "*** Handling current RTP packet before flushing. seq=%u ***\n", ntohs(rtp_session->last_rtp_hdr.seq));
#endif
		    handle_rfc2833(rtp_session, bytes_in, &do_cng);
		}

		do {
			if (switch_rtp_ready(rtp_session)) {
				bytes = sizeof(rtp_msg_t);
				switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock_input, 0, (void *) &rtp_session->recv_msg, &bytes);

				if (bytes) {
					int do_cng = 0;

					/* Make sure to handle RFC2833 packets, even if we're flushing the packets */
					if (bytes > rtp_header_len && rtp_session->recv_msg.header.version == 2 && rtp_session->recv_msg.header.pt == rtp_session->recv_te) {
						rtp_session->last_rtp_hdr = rtp_session->recv_msg.header;
						handle_rfc2833(rtp_session, bytes, &do_cng);
#ifdef DEBUG_2833
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "*** RTP packet handled in flush loop %d ***\n", do_cng);
#endif
					}

					flushed++;

					rtp_session->stats.inbound.raw_bytes += bytes;
					rtp_session->stats.inbound.flush_packet_count++;
					rtp_session->stats.inbound.packet_count++;
				}
			} else {
				break;
			}
		} while (bytes > 0);

#ifdef DEBUG_2833
        if (flushed) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "*** do_flush: total flushed packets: %ld ***\n",(long)flushed);
        }
#endif


		if (was_blocking && switch_rtp_ready(rtp_session)) {
			switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
			switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, FALSE);
		}


		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && rtp_session->session) {
			//int type = 1; // sum flags: 1 encoder; 2; decoder
			//switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_VIDEO, SWITCH_IO_READ, SCC_VIDEO_RESET, SCCT_INT, (void *)&type, NULL, NULL);
			switch_core_session_request_video_refresh(rtp_session->session);
		}
	}

 end:

	READ_DEC(rtp_session);

	return bytes_out;
}

static int check_recv_payload(switch_rtp_t *rtp_session)
{
	int ok = 1;

	if (!(rtp_session->rtp_bugs & RTP_BUG_ACCEPT_ANY_PAYLOAD) && rtp_session->pmaps && *rtp_session->pmaps) {
		payload_map_t *pmap;
		ok = 0;

		switch_mutex_lock(rtp_session->flag_mutex);

		for (pmap = *rtp_session->pmaps; pmap && pmap->allocated; pmap = pmap->next) {
			if (!pmap->negotiated) {
				continue;
			}

			if (rtp_session->last_rtp_hdr.pt == pmap->pt) {
				ok = 1;
			}
		}
		switch_mutex_unlock(rtp_session->flag_mutex);
	}

	return ok;
}

static int get_recv_payload(switch_rtp_t *rtp_session)
{
	int r = -1;

	if (rtp_session->pmaps && *rtp_session->pmaps) {
		payload_map_t *pmap;

		switch_mutex_lock(rtp_session->flag_mutex);

		for (pmap = *rtp_session->pmaps; pmap && pmap->allocated; pmap = pmap->next) {
			if (pmap->negotiated) {
				r = pmap->pt;
				break;
			}
		}
		switch_mutex_unlock(rtp_session->flag_mutex);
	}

	return r;
}

#define return_cng_frame() do_cng = 1; goto timer_check

static switch_status_t read_rtp_packet(switch_rtp_t *rtp_session, switch_size_t *bytes, switch_frame_flag_t *flags,
									   payload_map_t **pmapP, switch_status_t poll_status, switch_bool_t return_jb_packet)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint32_t ts = 0;
	unsigned char *b = NULL;
	int sync = 0;
	switch_time_t now;
	switch_size_t xcheck_jitter = 0;
	int tries = 0;
	int block = 0;

	switch_assert(bytes);
 more:

	tries++;

	if (tries > 20) {
		if (rtp_session->jb && !rtp_session->pause_jb && jb_valid(rtp_session)) {
			switch_jb_reset(rtp_session->jb);
		}
		rtp_session->punts++;
		rtp_session->clean = 0;
		*bytes = 0;
		return SWITCH_STATUS_BREAK;
	}

	if (block) {
		int to = 20000;
		int fdr = 0;

		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
			to = 100000;
		} else {
			if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] && rtp_session->timer.interval) {
				to = rtp_session->timer.interval * 1000;
			}
		}

		poll_status = switch_poll(rtp_session->read_pollfd, 1, &fdr, to);

		if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] && rtp_session->timer.interval) {
			switch_core_timer_sync(&rtp_session->timer);
		}

		if (rtp_session->session) {
			switch_ivr_parse_all_messages(rtp_session->session);
		}

		block = 0;
	}

	*bytes = sizeof(rtp_msg_t);
	sync = 0;

	rtp_session->has_rtp = 0;
	rtp_session->has_ice = 0;
	rtp_session->has_rtcp = 0;
	if (rtp_session->dtls) {
		rtp_session->dtls->bytes = 0;
		rtp_session->dtls->data = NULL;
	}
	memset(&rtp_session->last_rtp_hdr, 0, sizeof(rtp_session->last_rtp_hdr));

	if (poll_status == SWITCH_STATUS_SUCCESS) {
		status = switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock_input, 0, (void *) &rtp_session->recv_msg, bytes);
	} else {
		*bytes = 0;
	}

	if (*bytes) {
		b = (unsigned char *) &rtp_session->recv_msg;

		/* version 2 probably rtp, zrtp cookie present means zrtp */
		rtp_session->has_rtp = (rtp_session->recv_msg.header.version == 2 || ntohl(*(int *)(b+4)) == ZRTP_MAGIC_COOKIE);

		if ((*b >= 20) && (*b <= 64)) {
			if (rtp_session->dtls) {
				rtp_session->dtls->bytes = *bytes;
				rtp_session->dtls->data = (void *) &rtp_session->recv_msg;
			}
			rtp_session->has_ice = 0;
			rtp_session->has_rtp = 0;
			rtp_session->has_rtcp = 0;
		} else if (*b == 0 || *b == 1) {
			rtp_session->has_ice = 1;
			rtp_session->has_rtp = 0;
			rtp_session->has_rtcp = 0;
		} else {
			if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
				switch(rtp_session->recv_msg.header.pt) {
				case 64:        //  192 Full INTRA-frame request.
				case 72:        //  200 Sender report.
				case 73:        //  201 Receiver report.
				case 74:        //  202 Source description.
				case 75:        //  203 Goodbye.
				case 76:        //  204 Application-defined.
				case 77:        //  205 Transport layer FB message.
				case 78:        //  206 Payload-specific FB message.
				case 79:        //  207 Extended report.
					rtp_session->has_rtcp = 1;
					rtp_session->has_rtp = 0;
					rtp_session->has_ice = 0;
					break;
				default:
					if (rtp_session->rtcp_recv_msg_p->header.version == 2 &&
						rtp_session->rtcp_recv_msg_p->header.type > 199 && rtp_session->rtcp_recv_msg_p->header.type < 208) {
						rtp_session->has_rtcp = 1;
						rtp_session->has_rtp = 0;
						rtp_session->has_ice = 0;
					}
					break;
				}
			}
		}

		if (rtp_session->has_rtp || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
			rtp_session->missed_count = 0;
			switch_cp_addr(rtp_session->rtp_from_addr, rtp_session->from_addr);
		}

		if (rtp_session->has_rtp) {
			rtp_session->last_rtp_hdr = rtp_session->recv_msg.header;


			if (bytes && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] && !rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] &&
				rtp_session->last_rtp_hdr.pt != 13 &&
				rtp_session->last_rtp_hdr.pt != rtp_session->recv_te &&
				rtp_session->last_rtp_hdr.pt != rtp_session->cng_pt) {
				int accept_packet = 1;


				if (!(rtp_session->rtp_bugs & RTP_BUG_ACCEPT_ANY_PAYLOAD) && 
					!(rtp_session->rtp_bugs & RTP_BUG_ACCEPT_ANY_PACKETS) && rtp_session->pmaps && *rtp_session->pmaps) {
					payload_map_t *pmap;
					accept_packet = 0;

					switch_mutex_lock(rtp_session->flag_mutex);
					for (pmap = *rtp_session->pmaps; pmap && pmap->allocated; pmap = pmap->next) {

						if (ntohl(*(int *)(b+4)) == ZRTP_MAGIC_COOKIE) {
							accept_packet = 1;
							break;
						}

						if (!pmap->negotiated) {
							continue;
						}

						if (rtp_session->last_rtp_hdr.pt == pmap->pt) {
							accept_packet = 1;
							if (pmapP) {
								*pmapP = pmap;
							}
							break;
						}
					}
					switch_mutex_unlock(rtp_session->flag_mutex);
				}

				if (!accept_packet) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1,
									  "Invalid Packet SEQ: %d TS: %d PT:%d ignored\n",
									  ntohs(rtp_session->recv_msg.header.seq), ntohl(rtp_session->last_rtp_hdr.ts), rtp_session->last_rtp_hdr.pt);
					*bytes = 0;
				}
			}

			if (rtp_session->flags[SWITCH_RTP_FLAG_DETECT_SSRC]) {
				//if (rtp_session->remote_ssrc != rtp_session->stats.rtcp.peer_ssrc && rtp_session->stats.rtcp.peer_ssrc) {
				//	rtp_session->remote_ssrc = rtp_session->stats.rtcp.peer_ssrc;
				//}

				if (rtp_session->remote_ssrc != rtp_session->last_rtp_hdr.ssrc && rtp_session->last_rtp_hdr.ssrc) {
					rtp_session->remote_ssrc = ntohl(rtp_session->last_rtp_hdr.ssrc);
				}
			}
		}
	}

	if (!rtp_session->vb && (!rtp_session->jb || rtp_session->pause_jb || !jb_valid(rtp_session))) {
		if (*bytes > rtp_header_len && (rtp_session->has_rtp && check_recv_payload(rtp_session))) {
			xcheck_jitter = *bytes;
			check_jitter(rtp_session);
		}
	}

	if (check_rtcp_and_ice(rtp_session) == -1) {
		//if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
		//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "WTF CHECK FAIL\n");
		//}
		return SWITCH_STATUS_GENERR;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
		goto udptl;
	}


	if (*bytes) {
		*flags &= ~SFF_PROXY_PACKET;

		//if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
		//	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "WTF BYTES %ld b=%d\n", *bytes, *b);
		//}


		if (rtp_session->has_ice) {
			if (rtp_session->ice.ice_user) {
				handle_ice(rtp_session, &rtp_session->ice, (void *) &rtp_session->recv_msg, *bytes);
			}
			*bytes = 0;
			sync = 1;
		}
	}

	switch_mutex_lock(rtp_session->ice_mutex);

	if (rtp_session->dtls) {

		if (rtp_session->rtcp_dtls && rtp_session->rtcp_dtls != rtp_session->dtls) {
			rtp_session->rtcp_dtls->bytes = 0;
			rtp_session->rtcp_dtls->data = NULL;
			do_dtls(rtp_session, rtp_session->rtcp_dtls);
		}

		do_dtls(rtp_session, rtp_session->dtls);

		if (rtp_session->dtls && rtp_session->dtls->bytes) {
			*bytes = 0;
			sync = 1;
		}
	}



	if (status == SWITCH_STATUS_SUCCESS && *bytes) {
		if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
			*flags &= ~SFF_RTCP;
			if (rtp_session->has_rtcp) {
				*flags |= SFF_RTCP;

#ifdef ENABLE_SRTP
				if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV]) {
					int sbytes = (int) *bytes;
					srtp_err_status_t stat = 0;

					if (!rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV_MKI]) {
						stat = srtp_unprotect_rtcp(rtp_session->recv_ctx[rtp_session->srtp_idx_rtcp], &rtp_session->rtcp_recv_msg_p->header, &sbytes);
					} else {
						stat = srtp_unprotect_rtcp_mki(rtp_session->recv_ctx[rtp_session->srtp_idx_rtcp], &rtp_session->rtcp_recv_msg_p->header, &sbytes, 1);
					}

					if (stat) {
						//++rtp_session->srtp_errs[rtp_session->srtp_idx_rtp]++;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "RTCP UNPROTECT ERR\n");
					} else {
						//rtp_session->srtp_errs[rtp_session->srtp_idx_rtp] = 0;
					}

					*bytes = sbytes;
				}
#endif
				switch_mutex_unlock(rtp_session->ice_mutex);
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	switch_mutex_unlock(rtp_session->ice_mutex);

	if ((*bytes && (!rtp_write_ready(rtp_session, *bytes, __LINE__) || !rtp_session->has_rtp || rtp_session->has_rtcp)) || sync) {
		rtp_session->hot_hits = 0;
		block = 1;
		*bytes = 0;
		goto more;
	}

	if (*bytes && rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_READ]) {
		const char *tx_host;
		const char *old_host;
		const char *my_host;

		char bufa[50], bufb[50], bufc[50];


		tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtp_from_addr);
		old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
		my_host = switch_get_addr(bufc, sizeof(bufc), rtp_session->local_addr);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(rtp_session->session), SWITCH_LOG_CONSOLE,
						  "R %s b=%4ld %s:%u %s:%u %s:%u pt=%d ts=%u seq=%u m=%d\n",
						  rtp_session->session ? switch_channel_get_name(switch_core_session_get_channel(rtp_session->session)) : "No-Name",
						  (long) *bytes,
						  my_host, switch_sockaddr_get_port(rtp_session->local_addr),
						  old_host, rtp_session->remote_port,
						  tx_host, switch_sockaddr_get_port(rtp_session->rtp_from_addr),
						  rtp_session->last_rtp_hdr.pt, ntohl(rtp_session->last_rtp_hdr.ts), ntohs(rtp_session->last_rtp_hdr.seq),
						  rtp_session->last_rtp_hdr.m);

	}

#ifdef RTP_READ_PLOSS
	{
		int r = (rand() % 10000) + 1;
		if (r <= 200) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ALERT,
							  "Simulate dropped packet ......... ts: %u seq: %u\n", ntohl(rtp_session->last_rtp_hdr.ts), ntohs(rtp_session->last_rtp_hdr.seq));
			*bytes = 0;
		}
	}
#endif



 udptl:

	ts = 0;
	rtp_session->recv_msg.ebody = NULL;
	now = switch_micro_time_now();

	if (*bytes) {
		uint16_t seq = ntohs((uint16_t) rtp_session->last_rtp_hdr.seq);
		ts = ntohl(rtp_session->last_rtp_hdr.ts);

#ifdef DEBUG_MISSED_SEQ
		if (rtp_session->last_seq && rtp_session->last_seq+1 != seq) {
			//2012-11-28 18:33:11.799070 [ERR] switch_rtp.c:2883 Missed -65536 RTP frames from sequence [65536] to [-1] (missed). Time since last read [20021]
			switch_size_t flushed_packets_diff = rtp_session->stats.inbound.flush_packet_count - rtp_session->last_flush_packet_count;
			switch_size_t num_missed = (switch_size_t)seq - (rtp_session->last_seq+1);

			if (num_missed == 1) { /* We missed one packet */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missed one RTP frame with sequence [%d]%s. Time since last read [%ld]\n",
								  rtp_session->last_seq+1, (flushed_packets_diff == 1) ? " (flushed by FS)" : " (missed)",
								  rtp_session->last_read_time ? now-rtp_session->last_read_time : 0);
			} else { /* We missed multiple packets */
				if (flushed_packets_diff == 0) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (missed). Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  rtp_session->last_read_time ? now-rtp_session->last_read_time : 0);
				} else if (flushed_packets_diff == num_missed) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (flushed by FS). Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  rtp_session->last_read_time ? now-rtp_session->last_read_time : 0);
				} else if (num_missed > flushed_packets_diff) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (%ld packets flushed by FS, %ld packets missed)."
									  " Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  flushed_packets_diff, num_missed-flushed_packets_diff,
									  rtp_session->last_read_time ? now-rtp_session->last_read_time : 0);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (%ld packets flushed by FS). Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  flushed_packets_diff, rtp_session->last_read_time ? now-rtp_session->last_read_time : 0);
				}
			}

		}
#endif
		rtp_session->last_seq = seq;


		rtp_session->last_flush_packet_count = rtp_session->stats.inbound.flush_packet_count;


		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && now - rtp_session->last_read_time > 5000000) {
			switch_rtp_video_refresh(rtp_session);
		}

		rtp_session->last_read_time = now;
	}

	if (*bytes && rtp_session->has_rtp && rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]){
		rtcp_stats(rtp_session);
	}


	if (!rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] && !rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] && !rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] &&
		*bytes && rtp_session->last_rtp_hdr.pt != rtp_session->recv_te &&
		ts && !rtp_session->jb && !rtp_session->pause_jb && jb_valid(rtp_session) && ts == rtp_session->last_cng_ts) {
		/* we already sent this frame..... */
		*bytes = 0;
		return SWITCH_STATUS_SUCCESS;
	}

	if (*bytes) {
		if (!rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] && !rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
#ifdef ENABLE_ZRTP
			/* ZRTP Recv */
			if (zrtp_on) {

				unsigned int sbytes = (int) *bytes;
				zrtp_status_t stat = 0;

				stat = zrtp_process_srtp(rtp_session->zrtp_stream, (void *) &rtp_session->recv_msg, &sbytes);

				switch (stat) {
				case zrtp_status_ok:
					*bytes = sbytes;
					break;
				case zrtp_status_drop:
					/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Error: zRTP protection drop with code %d\n", stat); */
					*bytes = 0;
					return SWITCH_STATUS_SUCCESS;
				case zrtp_status_fail:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
					return SWITCH_STATUS_FALSE;
				default:
					break;
				}
			}
#endif

#ifdef ENABLE_SRTP
			switch_mutex_lock(rtp_session->ice_mutex);
			if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] && rtp_session->has_rtp &&
				(check_recv_payload(rtp_session) ||
				 rtp_session->last_rtp_hdr.pt == rtp_session->recv_te ||
				 rtp_session->last_rtp_hdr.pt == rtp_session->cng_pt)) {
				//if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] && (!rtp_session->ice.ice_user || rtp_session->has_rtp)) {
				int sbytes = (int) *bytes;
				srtp_err_status_t stat = 0;

				if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV_RESET] || !rtp_session->recv_ctx[rtp_session->srtp_idx_rtp]) {
					switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV_RESET);
					srtp_dealloc(rtp_session->recv_ctx[rtp_session->srtp_idx_rtp]);
					rtp_session->recv_ctx[rtp_session->srtp_idx_rtp] = NULL;
					if ((stat = srtp_create(&rtp_session->recv_ctx[rtp_session->srtp_idx_rtp],
											&rtp_session->recv_policy[rtp_session->srtp_idx_rtp])) || !rtp_session->recv_ctx[rtp_session->srtp_idx_rtp]) {

						rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] = 0;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP RECV\n");
						rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] = 0;
						switch_mutex_unlock(rtp_session->ice_mutex);
						return SWITCH_STATUS_FALSE;
					} else {

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO, "RE-Activating Secure RTP RECV\n");
						rtp_session->srtp_errs[rtp_session->srtp_idx_rtp] = 0;
					}
				}

				if (!(*flags & SFF_PLC) && rtp_session->recv_ctx[rtp_session->srtp_idx_rtp]) {
					stat = 0;
					
					if (!rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV_MKI]) {
						stat = srtp_unprotect(rtp_session->recv_ctx[rtp_session->srtp_idx_rtp], &rtp_session->recv_msg.header, &sbytes);
					} else {
						stat = srtp_unprotect_mki(rtp_session->recv_ctx[rtp_session->srtp_idx_rtp], &rtp_session->recv_msg.header, &sbytes, 1);
					}

					if (rtp_session->flags[SWITCH_RTP_FLAG_NACK] && stat == srtp_err_status_replay_fail) {
						/* false alarm nack */
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "REPLAY ERR, FALSE NACK\n");
						stat = 0;
						sbytes = 0;
						*bytes = 0;
						if (rtp_session->stats.rtcp.pkt_count) {
							rtp_session->stats.rtcp.period_pkt_count--;
							rtp_session->stats.rtcp.pkt_count--;
						}
						switch_mutex_unlock(rtp_session->ice_mutex);
						goto more;
					}
				}

				if (stat && rtp_session->recv_msg.header.pt != rtp_session->recv_te && rtp_session->recv_msg.header.pt != rtp_session->cng_pt) {
					int errs = ++rtp_session->srtp_errs[rtp_session->srtp_idx_rtp];
					if (stat != 10) {
						char *msg;
						if (stat == srtp_err_status_replay_fail) msg="replay check failed";
						else if (stat == srtp_err_status_auth_fail) msg="auth check failed";
						else msg="";
						if (errs >= MAX_SRTP_ERRS) {
							switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR,
											  "SRTP %s unprotect failed with code %d (%s) %ld bytes %d errors\n",
											  rtp_type(rtp_session), stat, msg, (long)*bytes, errs);
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR,
											  "Ending call due to SRTP error\n");
							switch_channel_hangup(channel, SWITCH_CAUSE_SRTP_READ_ERROR);
						} else if (errs >= WARN_SRTP_ERRS && !(errs % WARN_SRTP_ERRS)) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING,
											  "SRTP %s unprotect failed with code %d (%s) %ld bytes %d errors\n",
											  rtp_type(rtp_session), stat, msg, (long)*bytes, errs);
						}
					}
					sbytes = 0;
				} else {
					rtp_session->srtp_errs[rtp_session->srtp_idx_rtp] = 0;
				}

				*bytes = sbytes;
			}
			switch_mutex_unlock(rtp_session->ice_mutex);
#endif
		}


		if (rtp_session->has_rtp) {
			if (rtp_session->recv_msg.header.cc > 0) { /* Contributing Source Identifiers (4 bytes = sizeof CSRC header)*/
				rtp_session->recv_msg.ebody = RTP_BODY(rtp_session) + (rtp_session->recv_msg.header.cc * 4);
			}

			/* recalculate body length in case rtp extension used */
			if (!rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] && !rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] &&
				rtp_session->has_rtp && rtp_session->recv_msg.header.x) { /* header extensions */
				uint16_t length;

				rtp_session->recv_msg.ext = (switch_rtp_hdr_ext_t *) RTP_BODY(rtp_session);
				length = ntohs((uint16_t)rtp_session->recv_msg.ext->length);

				if (length < SWITCH_RTP_MAX_BUF_LEN_WORDS) {
					rtp_session->recv_msg.ebody = (char *)rtp_session->recv_msg.ext + (length * 4) + 4;
					if (*bytes > (length * 4 + 4)) {
						*bytes -= (length * 4 + 4);
					} else {
						*bytes = 0;
					}
				}
			}


#ifdef DEBUG_CHROME

			if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && rtp_session->has_rtp) {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
								  "VIDEO: seq: %d ts: %u len: %ld %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x mark: %d\n",
								  ntohs(rtp_session->last_rtp_hdr.seq), ntohl(rtp_session->last_rtp_hdr.ts), *bytes,
								  *((uint8_t *)RTP_BODY(rtp_session)), *((uint8_t *)RTP_BODY(rtp_session) + 1),
								  *((uint8_t *)RTP_BODY(rtp_session) + 2), *((uint8_t *)RTP_BODY(rtp_session) + 3),
								  *((uint8_t *)RTP_BODY(rtp_session) + 4), *((uint8_t *)RTP_BODY(rtp_session) + 5),
								  *((uint8_t *)RTP_BODY(rtp_session) + 6), *((uint8_t *)RTP_BODY(rtp_session) + 7),
								  *((uint8_t *)RTP_BODY(rtp_session) + 8), *((uint8_t *)RTP_BODY(rtp_session) + 9),
								  *((uint8_t *)RTP_BODY(rtp_session) + 10), rtp_session->last_rtp_hdr.m);

			}
#endif



		}


		rtp_session->stats.inbound.raw_bytes += *bytes;

		if (rtp_session->last_rtp_hdr.pt == rtp_session->recv_te) {
			rtp_session->stats.inbound.dtmf_packet_count++;
		} else if (rtp_session->last_rtp_hdr.pt == rtp_session->cng_pt || rtp_session->last_rtp_hdr.pt == 13) {
			rtp_session->stats.inbound.cng_packet_count++;
		} else {
			rtp_session->stats.inbound.media_packet_count++;
			rtp_session->stats.inbound.media_bytes += *bytes;
		}

		rtp_session->stats.inbound.packet_count++;
	}

	if (!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] &&
		((rtp_session->recv_te && rtp_session->last_rtp_hdr.pt == rtp_session->recv_te) ||
		 (*bytes < rtp_header_len && *bytes > 0 && !(rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL])))) {
		return SWITCH_STATUS_BREAK;
	}

	if (ts) {
		rtp_session->last_read_ts = ts;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_BYTESWAP] && check_recv_payload(rtp_session)) {
		switch_swap_linear((int16_t *)RTP_BODY(rtp_session), (int) *bytes - rtp_header_len);
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_KILL_JB]) {
		rtp_session->flags[SWITCH_RTP_FLAG_KILL_JB] = 0;

		if (rtp_session->jb) {
			switch_jb_destroy(&rtp_session->jb);
		}

		if (rtp_session->vb) {
			switch_jb_destroy(&rtp_session->vb);
		}
	}

	if (rtp_session->has_rtp && *bytes) {
		uint32_t read_ssrc = ntohl(rtp_session->last_rtp_hdr.ssrc);

		if (rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
			return SWITCH_STATUS_SUCCESS;
		}

		if (rtp_session->vb && !rtp_session->pause_jb && jb_valid(rtp_session)) {
			status = switch_jb_put_packet(rtp_session->vb, (switch_rtp_packet_t *) &rtp_session->recv_msg, *bytes);

			if (status == SWITCH_STATUS_TOO_LATE) {
				goto more;
			}

			status = SWITCH_STATUS_FALSE;
			*bytes = 0;

			if (!return_jb_packet) {
				return status;
			}
		}

		if (rtp_session->jb && jb_valid(rtp_session)) {
			if (rtp_session->last_jb_read_ssrc && rtp_session->last_jb_read_ssrc != read_ssrc) {
				switch_jb_reset(rtp_session->jb);
			}

			rtp_session->last_jb_read_ssrc = read_ssrc;
		}

		if (rtp_session->jb && !rtp_session->pause_jb && jb_valid(rtp_session)) {

			if (!rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] && rtp_session->timer.interval) {
				switch_core_timer_sync(&rtp_session->timer);
				reset_jitter_seq(rtp_session);
			}

			status = switch_jb_put_packet(rtp_session->jb, (switch_rtp_packet_t *) &rtp_session->recv_msg, *bytes);
			if (status == SWITCH_STATUS_TOO_LATE) {
				goto more;
			}


			status = SWITCH_STATUS_FALSE;
			*bytes = 0;

			if (!return_jb_packet) {
				return status;
			}
		} else {
			if (rtp_session->last_rtp_hdr.m && rtp_session->last_rtp_hdr.pt != rtp_session->recv_te &&
				!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && !(rtp_session->rtp_bugs & RTP_BUG_IGNORE_MARK_BIT)) {
				switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH);
			} else if (rtp_session->last_jb_read_ssrc && rtp_session->last_jb_read_ssrc != read_ssrc) {
				switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH);
			}
		}
	}

	if (!*bytes || rtp_session->has_rtp) {

		if (rtp_session->jb && !rtp_session->pause_jb && jb_valid(rtp_session)) {
			switch_status_t jstatus = switch_jb_get_packet(rtp_session->jb, (switch_rtp_packet_t *) &rtp_session->recv_msg, bytes);

			status = jstatus;

			switch(jstatus) {
			case SWITCH_STATUS_MORE_DATA:
				if (rtp_session->punts < 4) {
					block = 1;
					goto more;
				}
				*bytes = 0;
				break;
			case SWITCH_STATUS_NOTFOUND:
				{
					int pt = get_recv_payload(rtp_session);
					(*flags) |= SFF_PLC;
					status = SWITCH_STATUS_SUCCESS;
					*bytes = switch_jb_get_last_read_len(rtp_session->jb);
					rtp_session->last_rtp_hdr = rtp_session->recv_msg.header;
					rtp_session->last_rtp_hdr.pt = pt;
				}
				break;
			case SWITCH_STATUS_BREAK:
				break;
			case SWITCH_STATUS_SUCCESS:
			case SWITCH_STATUS_TIMEOUT:
			default:
				{
					if (status == SWITCH_STATUS_TIMEOUT) {
						rtp_session->skip_timer = 1;
					}
					rtp_session->stats.inbound.jb_packet_count++;
					status = SWITCH_STATUS_SUCCESS;
					rtp_session->last_rtp_hdr = rtp_session->recv_msg.header;
					if (++rtp_session->clean > 200) {
						rtp_session->punts = 0;
					}
					if (!xcheck_jitter) {
						check_jitter(rtp_session);
						xcheck_jitter = *bytes;
					}
				}
				break;
			}
		}

		if (rtp_session->vb && !rtp_session->pause_jb && jb_valid(rtp_session)) {
			switch_status_t vstatus = switch_jb_get_packet(rtp_session->vb, (switch_rtp_packet_t *) &rtp_session->recv_msg, bytes);
			status = vstatus;

			switch(vstatus) {
			case SWITCH_STATUS_RESTART:
				switch_core_session_request_video_refresh(rtp_session->session);
				status = SWITCH_STATUS_BREAK;
				break;
			case SWITCH_STATUS_MORE_DATA:
				status = SWITCH_STATUS_BREAK;
				break;
			case SWITCH_STATUS_BREAK:
			default:
				break;
			}

			if (vstatus == SWITCH_STATUS_NOTFOUND && rtp_session->flags[SWITCH_RTP_FLAG_TEXT]) {
				int pt = get_recv_payload(rtp_session);
				(*flags) |= SFF_PLC;
				status = SWITCH_STATUS_SUCCESS;
				*bytes = switch_jb_get_last_read_len(rtp_session->vb);
				rtp_session->last_rtp_hdr = rtp_session->recv_msg.header;
				if (pt > -1) {
					rtp_session->last_rtp_hdr.pt = pt;
				}
			}

			if (vstatus == SWITCH_STATUS_SUCCESS) {
				rtp_session->last_rtp_hdr = rtp_session->recv_msg.header;

				if (!xcheck_jitter) {
					check_jitter(rtp_session);
					xcheck_jitter = *bytes;
				}
			}
		}
	}

	return status;
}

static void handle_nack(switch_rtp_t *rtp_session, uint32_t nack)
{
	switch_size_t bytes = 0;
	rtp_msg_t send_msg[1] = {{{0}}};
	uint16_t seq = (uint16_t) (nack & 0xFFFF);
	uint16_t blp = (uint16_t) (nack >> 16);
	int i;
	const char *tx_host = NULL;
	const char *old_host = NULL;
	const char *my_host = NULL;
	char bufa[50], bufb[50], bufc[50];

	if (!(rtp_session->flags[SWITCH_RTP_FLAG_NACK] && rtp_session->vbw)) {
		return;  /* not enabled */
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE]) {
		tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtcp_from_addr);
		old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
		my_host = switch_get_addr(bufc, sizeof(bufc), rtp_session->local_addr);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s Got NACK [%u][0x%x] for seq %u\n", 
					  switch_core_session_get_name(rtp_session->session), nack, nack, ntohs(seq));

	if (switch_jb_get_packet_by_seq(rtp_session->vbw, seq, (switch_rtp_packet_t *) send_msg, &bytes) == SWITCH_STATUS_SUCCESS) {

		if (rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE]) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(rtp_session->session), SWITCH_LOG_CONSOLE,
							  "X %s b=%4ld %s:%u %s:%u %s:%u pt=%d ts=%u seq=%u m=%d\n",
							  rtp_session->session ? switch_channel_get_name(switch_core_session_get_channel(rtp_session->session)) : "NoName",
							  (long) bytes,
							  my_host, switch_sockaddr_get_port(rtp_session->local_addr),
							  old_host, rtp_session->remote_port,
							  tx_host, switch_sockaddr_get_port(rtp_session->rtcp_from_addr),
							  send_msg->header.pt, ntohl(send_msg->header.ts), ntohs(send_msg->header.seq), send_msg->header.m);

		}
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "RE----SEND %u\n", ntohs(send_msg->header.seq));
		switch_rtp_write_raw(rtp_session, (void *) send_msg, &bytes, SWITCH_FALSE);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Cannot send NACK for seq %u\n", ntohs(seq));
	}

	blp = ntohs(blp);
	for (i = 0; i < 16; i++) {
		if (blp & (1 << i)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s Also Got NACK for seq %u\n", 
							  switch_core_session_get_name(rtp_session->session), ntohs(seq) + i + 1);
			/* If they are missing more than one, may as well gen a key frame for good measure */
			//switch_core_media_gen_key_frame(rtp_session->session);
			if (switch_jb_get_packet_by_seq(rtp_session->vbw, htons(ntohs(seq) + i + 1), (switch_rtp_packet_t *) &send_msg, &bytes) == SWITCH_STATUS_SUCCESS) {
				if (rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE]) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(rtp_session->session), SWITCH_LOG_CONSOLE,
									  "X %s b=%4ld %s:%u %s:%u %s:%u pt=%d ts=%u seq=%u m=%d\n",
									  rtp_session->session ? switch_channel_get_name(switch_core_session_get_channel(rtp_session->session)) : "NoName",
									  (long) bytes,
									  my_host, switch_sockaddr_get_port(rtp_session->local_addr),
									  old_host, rtp_session->remote_port,
									  tx_host, switch_sockaddr_get_port(rtp_session->rtcp_from_addr),
									  send_msg->header.pt, ntohl(send_msg->header.ts), ntohs(send_msg->header.seq), send_msg->header.m);

				}
				//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "RE----SEND %u\n", ntohs(send_msg->header.seq));

				switch_rtp_write_raw(rtp_session, (void *) &send_msg, &bytes, SWITCH_FALSE);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "Cannot send NACK for seq %u\n", ntohs(seq) + i);
			}
		}
	}
}

static switch_status_t process_rtcp_report(switch_rtp_t *rtp_session, rtcp_msg_t *msg, switch_size_t bytes)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int i;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,
					  "RTCP packet bytes %" SWITCH_SIZE_T_FMT " type %d pad %d\n",
					  bytes, msg->header.type, msg->header.p);

	if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && (msg->header.type == _RTCP_PT_RTPFB || msg->header.type == _RTCP_PT_PSFB || msg->header.type < 200)) {
		rtcp_ext_msg_t *extp = (rtcp_ext_msg_t *) msg;

		if (extp->header.fmt != 15) { // <---- REMOVE WHEN BRIA STOPS SENDING UNSOLICITED REMB
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s PICKED UP %s XRTCP type: %d fmt: %d\n",
							  switch_core_session_get_name(rtp_session->session), rtp_type(rtp_session), msg->header.type, extp->header.fmt);
		}

		if (msg->header.type == _RTCP_PT_FIR ||
			(msg->header.type == _RTCP_PT_PSFB && (extp->header.fmt == _RTCP_PSFB_FIR || extp->header.fmt == _RTCP_PSFB_PLI))) {
#if 0
			if (msg->header.type == _RTCP_PT_FIR) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "Ancient FIR Received. Hello from 1996!\n");

				if (!switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_OLD_FIR)) {
					switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_OLD_FIR);
					switch_core_session_request_video_refresh(rtp_session->session);
				}
			}
#endif
			
			if (switch_core_session_media_flow(rtp_session->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s Ignoring FIR/PLI from a sendonly stream.\n", 
								  switch_core_session_get_name(rtp_session->session));
			} else {
				switch_core_media_gen_key_frame(rtp_session->session);
				if (rtp_session->vbw) {
					switch_jb_reset(rtp_session->vbw);
				}
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s Got FIR/PLI\n", 
								  switch_core_session_get_name(rtp_session->session));
				switch_channel_set_flag(switch_core_session_get_channel(rtp_session->session), CF_VIDEO_REFRESH_REQ);
			}
		}

		if (msg->header.type == _RTCP_PT_RTPFB && extp->header.fmt == _RTCP_RTPFB_NACK) {
			uint32_t *nack = (uint32_t *) extp->body;
			int i;

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s Got NACK count %d\n", 
							  switch_core_session_get_name(rtp_session->session), ntohs(extp->header.length) - 2);


			for (i = 0; i < ntohs(extp->header.length) - 2; i++) {
				handle_nack(rtp_session, nack[i]);
			}

			//switch_core_media_gen_key_frame(rtp_session->session);
		}
		
	} else {
		struct switch_rtcp_report_block *report;

		if (msg->header.type == _RTCP_PT_SR || msg->header.type == _RTCP_PT_RR) {
			int i;
#ifdef DEBUG_RTCP
			switch_time_t now = switch_micro_time_now();
#endif 
			uint32_t lsr_now;
			uint32_t lsr;
			uint32_t packet_ssrc;
			double rtt_now = 0;
			uint8_t rtt_valid = 0;
			int rtt_increase = 0, packet_loss_increase=0;

			//if (msg->header.type == _RTCP_PT_SR && rtp_session->ice.ice_user) {
			//	rtp_session->send_rr = 1;
			//}

			lsr_now = calc_local_lsr_now();

			if (msg->header.type == _RTCP_PT_SR) { /* Sender report */
				struct switch_rtcp_sender_report* sr = (struct switch_rtcp_sender_report*)msg->body;

				rtp_session->stats.rtcp.packet_count += ntohl(sr->sender_info.pc);
				rtp_session->stats.rtcp.octet_count += ntohl(sr->sender_info.oc);
				packet_ssrc = sr->ssrc;
				/* Extracting LSR from NTP timestamp and save it */
				lsr = (ntohl(sr->sender_info.ntp_lsw)&0xffff0000)>>16 | (ntohl(sr->sender_info.ntp_msw)&0x0000ffff)<<16; /* The middle 32 bits out of 64 in the NTP timestamp */
				rtp_session->stats.rtcp.last_recv_lsr_peer = htonl(lsr);  /* Save it include it in the next SR */
				rtp_session->stats.rtcp.last_recv_lsr_local = lsr_now;    /* Save it to calculate DLSR when generating next SR */
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,"Received a SR with %d report blocks, " \
								  "length in words = %d, " \
								  "SSRC = 0x%X, " \
								  "NTP MSW = %u, " \
								  "NTP LSW = %u, " \
								  "RTP timestamp = %u, " \
								  "Sender Packet Count = %u, " \
								  "Sender Octet Count = %u\n",
								  msg->header.count,
								  ntohs((uint16_t)msg->header.length),
								  ntohl(sr->ssrc),
								  ntohl(sr->sender_info.ntp_msw),
								  ntohl(sr->sender_info.ntp_lsw),
								  ntohl(sr->sender_info.ts),
								  ntohl(sr->sender_info.pc),
								  ntohl(sr->sender_info.oc));


				rtp_session->rtcp_frame.ssrc = ntohl(sr->ssrc);
				rtp_session->rtcp_frame.packet_type = (uint16_t)rtp_session->rtcp_recv_msg_p->header.type;
				rtp_session->rtcp_frame.ntp_msw = ntohl(sr->sender_info.ntp_msw);
				rtp_session->rtcp_frame.ntp_lsw = ntohl(sr->sender_info.ntp_lsw);
				rtp_session->rtcp_frame.timestamp = ntohl(sr->sender_info.ts);
				rtp_session->rtcp_frame.packet_count =  ntohl(sr->sender_info.pc);
				rtp_session->rtcp_frame.octect_count = ntohl(sr->sender_info.oc);

				report = &sr->report_block;
			} else { /* Receiver report */
				struct switch_rtcp_receiver_report* rr = (struct switch_rtcp_receiver_report*)msg->body;
				packet_ssrc = rr->ssrc;
				//memset(&rtp_session->rtcp_frame, 0, sizeof(rtp_session->rtcp_frame));
				report = &rr->report_block;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,"Received a RR with %d report blocks, " \
								  "length in words = %d, " \
								  "SSRC = 0x%X, ",
								  msg->header.count,
								  ntohs((uint16_t)msg->header.length),
								  ntohl(rr->ssrc));

			}
			

			for (i = 0; i < (int)msg->header.count && i < MAX_REPORT_BLOCKS ; i++) {
				uint32_t old_avg = rtp_session->rtcp_frame.reports[i].loss_avg;
				uint8_t percent_fraction = (uint8_t)report->fraction * 100 / 256 ;
				if (!rtp_session->rtcp_frame.reports[i].loss_avg) {
					rtp_session->rtcp_frame.reports[i].loss_avg = (uint8_t)percent_fraction;
				} else {
					rtp_session->rtcp_frame.reports[i].loss_avg = (uint32_t)(((float)rtp_session->rtcp_frame.reports[i].loss_avg * .7) +
																			 ((float)(uint8_t)percent_fraction * .3));
				}

				rtp_session->rtcp_frame.reports[i].ssrc = ntohl(report->ssrc);
				rtp_session->rtcp_frame.reports[i].fraction = (uint8_t)report->fraction;
#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
				rtp_session->rtcp_frame.reports[i].lost = report->lost; // signed 24bit will extended signess to int32_t automatically
#else
				rtp_session->rtcp_frame.reports[i].lost = ntohl(report->lost)>>8; // signed 24bit casted to uint32_t need >>8 after ntohl()...
				rtp_session->rtcp_frame.reports[i].lost = rtp_session->rtcp_frame.reports[i].lost | ((rtp_session->rtcp_frame.reports[i].lost & 0x00800000) ? 0xff000000 : 0x00000000); // ...and signess compensation
#endif
				rtp_session->rtcp_frame.reports[i].highest_sequence_number_received = ntohl(report->highest_sequence_number_received);
				rtp_session->rtcp_frame.reports[i].jitter = ntohl(report->jitter);
				rtp_session->rtcp_frame.reports[i].lsr = ntohl(report->lsr);
				rtp_session->rtcp_frame.reports[i].dlsr = ntohl(report->dlsr);
				
				if (rtp_session->rtcp_frame.reports[i].lsr && !rtp_session->flags[SWITCH_RTP_FLAG_RTCP_PASSTHRU]) {

					/* Calculating RTT = A - DLSR - LSR */
					rtt_now = ((double)(((int64_t)lsr_now) - rtp_session->rtcp_frame.reports[i].dlsr - rtp_session->rtcp_frame.reports[i].lsr))/65536;

					/* Only account RTT if it didn't overflow. */
					if (lsr_now > rtp_session->rtcp_frame.reports[i].dlsr + rtp_session->rtcp_frame.reports[i].lsr) {
#ifdef DEBUG_RTCP
						switch_time_exp_t now_hr;
						switch_time_exp_gmt(&now_hr,now);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,
								"Receiving an RTCP packet\n[%04d-%02d-%02d %02d:%02d:%02d.%d] SSRC[0x%x]\n"
								"RTT[%f] = A[%u] - DLSR[%u] - LSR[%u]\n",
								1900 + now_hr.tm_year, now_hr.tm_mday, now_hr.tm_mon, now_hr.tm_hour, now_hr.tm_min, now_hr.tm_sec, now_hr.tm_usec,
								rtp_session->rtcp_frame.reports[i].ssrc, rtt_now,
								lsr_now, rtp_session->rtcp_frame.reports[i].dlsr, rtp_session->rtcp_frame.reports[i].lsr);
#endif 
						rtt_valid = 1;
						if (!rtp_session->rtcp_frame.reports[i].rtt_avg) {
							rtp_session->rtcp_frame.reports[i].rtt_avg = rtt_now;
						} else {
							rtp_session->rtcp_frame.reports[i].rtt_avg = (double)((rtp_session->rtcp_frame.reports[i].rtt_avg * .7) + (rtt_now * .3 ));
						}
					} else {
#ifdef DEBUG_RTCP
						switch_time_exp_t now_hr;
						switch_time_exp_gmt(&now_hr,now);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING,
								"Receiving RTCP packet\n[%04d-%02d-%02d %02d:%02d:%02d.%d] SSRC[0x%x]\n"
								"Ignoring erroneous RTT[%f] = A[%u] - DLSR[%u] - LSR[%u]\n",
								1900 + now_hr.tm_year, now_hr.tm_mday, now_hr.tm_mon, now_hr.tm_hour, now_hr.tm_min, now_hr.tm_sec, now_hr.tm_usec,
								rtp_session->rtcp_frame.reports[i].ssrc, rtt_now,
								lsr_now, rtp_session->rtcp_frame.reports[i].dlsr, rtp_session->rtcp_frame.reports[i].lsr);
#endif
						rtt_valid = 0;
						rtt_now = 0;
					}


					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "RTT average %f\n",
							rtp_session->rtcp_frame.reports[i].rtt_avg);
				}

				if (rtp_session->flags[SWITCH_RTP_FLAG_ADJ_BITRATE_CAP] && rtp_session->flags[SWITCH_RTP_FLAG_ESTIMATORS] && !rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {

					/* SWITCH_RTP_FLAG_ADJ_BITRATE_CAP : Can the codec change its bitrate on the fly per API command ? */
#ifdef DEBUG_ESTIMATORS_
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "Current packet loss: [%d %%] Current RTT: [%f ms]\n", percent_fraction, rtt_now);
#endif

					if (rtt_valid) {

						switch_kalman_estimate(rtp_session->estimators[EST_RTT], rtt_now, EST_RTT);

						if (switch_kalman_cusum_detect_change(rtp_session->detectors[EST_RTT], rtt_now, rtp_session->estimators[EST_RTT]->val_estimate_last)) {
							/* sudden change in the mean value of RTT */
#ifdef DEBUG_ESTIMATORS_
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,"Sudden change in the mean value of RTT !\n");
#endif
							rtt_increase = 1;
						}
					}

					switch_kalman_estimate(rtp_session->estimators[EST_LOSS], percent_fraction, EST_LOSS);

					if (switch_kalman_cusum_detect_change(rtp_session->detectors[EST_LOSS], percent_fraction, rtp_session->estimators[EST_LOSS]->val_estimate_last)){
						/* sudden change in the mean value of packet loss */
#ifdef DEBUG_ESTIMATORS_
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,"Sudden change in the mean value of packet loss!\n");
#endif
						packet_loss_increase = 1;
					}
#ifdef DEBUG_ESTIMATORS_
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "ESTIMATORS: Packet loss will be: [%f] RTT will be: [%f ms]\n",
									  rtp_session->estimators[EST_LOSS]->val_estimate_last, rtp_session->estimators[EST_RTT]->val_estimate_last);
#endif

					if (rtp_session->rtcp_frame.reports[i].loss_avg != old_avg) {
						/*getting bad*/
						if (switch_kalman_is_slow_link(rtp_session->estimators[EST_LOSS],
													   rtp_session->estimators[EST_RTT])) {
							/* going to minimum bitrate */
#ifdef DEBUG_ESTIMATORS_
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "Slow link conditions: Loss average: [%d %%], Previous loss: [%d %%]. \
																	Going to minimum bitrate!",rtp_session->rtcp_frame.reports[i].loss_avg, old_avg);
#endif
							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_ADJUST_BITRATE, SCCT_STRING, "minimum", SCCT_NONE, NULL, NULL, NULL);
							/* if after going to minimum bitrate we still have packet loss then we increase ptime. TODO */

						} else if (packet_loss_increase && (rtp_session->estimators[EST_LOSS]->val_estimate_last >= 5)) {
							/* sudden change in the mean value of packet loss percentage */
							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_ADJUST_BITRATE,
															SCCT_STRING, "decrease",
															SCCT_NONE, NULL, NULL, NULL);
#ifdef DEBUG_ESTIMATORS_
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,"Sudden change in the mean value of packet loss percentage !\n");
#endif
							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_PACKET_LOSS, SCCT_INT,
															(void *)&rtp_session->rtcp_frame.reports[i].loss_avg,
															SCCT_NONE, NULL, NULL, NULL);

						} else if (rtt_valid && !rtt_increase && rtp_session->estimators[EST_LOSS]->val_estimate_last >= rtp_session->rtcp_frame.reports[i].loss_avg ) {
							/* lossy because of congestion (queues full somewhere -> some packets are dropped , but RTT is good ), packet loss with many small gaps */
#ifdef DEBUG_ESTIMATORS_
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "packet loss, but RTT is not bad\n");
#endif
							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_PACKET_LOSS, SCCT_INT,
															(void *)&rtp_session->rtcp_frame.reports[i].loss_avg,
															SCCT_NONE, NULL, NULL, NULL);

						} else if ((rtp_session->estimators[EST_LOSS]->val_estimate_last < 1) && packet_loss_increase) {
#ifdef DEBUG_ESTIMATORS_
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "small packet loss average\n");
#endif
							/*small loss_avg*/
							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_ADJUST_BITRATE,
															SCCT_STRING, "default",
															SCCT_NONE, NULL, NULL, NULL);

							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_PACKET_LOSS, SCCT_INT,
															(void *)&rtp_session->rtcp_frame.reports[i].loss_avg,
															SCCT_NONE, NULL, NULL, NULL);

						} else if ((rtp_session->estimators[EST_LOSS]->val_estimate_last < 5) &&
								   (rtp_session->rtcp_frame.reports[i].rtt_avg < rtp_session->estimators[EST_RTT]->val_estimate_last))  {

							/* estimate that packet loss will decrease, we can increase the bitrate */
							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_ADJUST_BITRATE,
															SCCT_STRING, "increase",
															SCCT_NONE, NULL, NULL, NULL);

							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_PACKET_LOSS, SCCT_INT,
															(void *)&rtp_session->rtcp_frame.reports[i].loss_avg,
															SCCT_NONE, NULL, NULL, NULL);

						} else {
							/* *do nothing about bitrate, just pass the packet loss to the codec */
#ifdef DEBUG_ESTIMATORS_
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3,"do nothing about bitrate, just pass the packet loss to the codec\n");
#endif
							switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
															SWITCH_IO_WRITE, SCC_AUDIO_PACKET_LOSS, SCCT_INT,
															(void *)&rtp_session->rtcp_frame.reports[i].loss_avg,
															SCCT_NONE, NULL, NULL, NULL);
						}
					}
				} else {
					if (!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && rtp_session->rtcp_frame.reports[i].loss_avg != old_avg) {
						switch_core_media_codec_control(rtp_session->session, SWITCH_MEDIA_TYPE_AUDIO,
														SWITCH_IO_WRITE, SCC_AUDIO_PACKET_LOSS, SCCT_INT,
														(void *)&rtp_session->rtcp_frame.reports[i].loss_avg,
														SCCT_NONE, NULL, NULL, NULL);
					}
				}

				report++;
			}
			rtp_session->rtcp_frame.report_count = (uint16_t)i;
				




			rtp_session->rtcp_fresh_frame = 1;
			rtp_session->stats.rtcp.peer_ssrc = ntohl(packet_ssrc);
		}
	}

	if (msg->header.type > 194 && msg->header.type < 255) {
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}


static switch_status_t process_rtcp_packet(switch_rtp_t *rtp_session, switch_size_t *bytes)
{
	switch_size_t len;
	switch_size_t remain = *bytes;
	switch_status_t status = SWITCH_STATUS_FALSE;
	rtcp_msg_t *msg = rtp_session->rtcp_recv_msg_p;

	if (msg->header.version != 2) {
		if (msg->header.version == 0) {
			if (rtp_session->ice.ice_user) {
				handle_ice(rtp_session, &rtp_session->rtcp_ice, (void *) msg, *bytes);
			}
			return SWITCH_STATUS_SUCCESS;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session),
							  SWITCH_LOG_WARNING, "Received an unsupported RTCP packet version %d\n", msg->header.version);
			return SWITCH_STATUS_FALSE;
		}
	}

	do {
		len = ((switch_size_t)ntohs(msg->header.length) * 4) + 4;

		if (msg->header.version != 2 || !(msg->header.type > 191 && msg->header.type < 210)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING,
							  "INVALID RTCP PACKET TYPE %d VER %d LEN %" SWITCH_SIZE_T_FMT "\n", msg->header.type,
							  msg->header.version, len);
			status = SWITCH_STATUS_BREAK;
			break;
		}

		//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_CRIT,
		//"WTF BYTES %ld REMAIN %ld PACKET TYPE %d LEN %ld\n", *bytes, remain, msg->header.type, len);

		if (len > remain) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING,
							  "RTCP INVALID LENGTH %" SWITCH_SIZE_T_FMT "\n", len);
			len = remain;
		}

		status = process_rtcp_report(rtp_session, msg, len);

		if (remain > len) {
			unsigned char *p = (unsigned char *) msg;
			p += len;
			msg = (rtcp_msg_t *) p;
		}

		remain -= len;

	} while (remain >= 4);

	return status;
}

static switch_status_t read_rtcp_packet(switch_rtp_t *rtp_session, switch_size_t *bytes, switch_frame_flag_t *flags)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(bytes);

	*bytes = sizeof(rtcp_msg_t);

	if ((status = switch_socket_recvfrom(rtp_session->rtcp_from_addr, rtp_session->rtcp_sock_input, 0, (void *) rtp_session->rtcp_recv_msg_p, bytes))
		!= SWITCH_STATUS_SUCCESS) {
		*bytes = 0;
	}

	switch_mutex_lock(rtp_session->ice_mutex);
	if (rtp_session->rtcp_dtls) {
		char *b = (char *) rtp_session->rtcp_recv_msg_p;

		if (*b == 0 || *b == 1) {
			if (rtp_session->rtcp_ice.ice_user) {
				handle_ice(rtp_session, &rtp_session->rtcp_ice, (void *) rtp_session->rtcp_recv_msg_p, *bytes);
			}
			*bytes = 0;
		}

		if (*bytes && (*b >= 20) && (*b <= 64)) {
			rtp_session->rtcp_dtls->bytes = *bytes;
			rtp_session->rtcp_dtls->data = (void *) rtp_session->rtcp_recv_msg_p;
		} else {
			rtp_session->rtcp_dtls->bytes = 0;
			rtp_session->rtcp_dtls->data = NULL;
		}

		do_dtls(rtp_session, rtp_session->rtcp_dtls);


		if (rtp_session->rtcp_dtls->bytes) {
			*bytes = 0;
		}
	}
	switch_mutex_unlock(rtp_session->ice_mutex);



#ifdef ENABLE_SRTP
	if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] && rtp_session->rtcp_recv_msg_p->header.version == 2) {
		//if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV] && (!rtp_session->ice.ice_user || rtp_session->rtcp_recv_msg_p->header.version == 2)) {
		int sbytes = (int) *bytes;
		srtp_err_status_t stat = 0;


		if (!rtp_session->flags[SWITCH_RTP_FLAG_SECURE_RECV_MKI]) {
			stat = srtp_unprotect_rtcp(rtp_session->recv_ctx[rtp_session->srtp_idx_rtcp], &rtp_session->rtcp_recv_msg_p->header, &sbytes);
		} else {
			stat = srtp_unprotect_rtcp_mki(rtp_session->recv_ctx[rtp_session->srtp_idx_rtcp], &rtp_session->rtcp_recv_msg_p->header, &sbytes, 1);
		}

		if (stat) {
			//++rtp_session->srtp_errs[rtp_session->srtp_idx_rtp]++;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "RTCP UNPROTECT ERR\n");
		} else {
			//rtp_session->srtp_errs[rtp_session->srtp_idx_rtp] = 0;
		}

		*bytes = sbytes;

	}
#endif


#ifdef ENABLE_ZRTP
	if (zrtp_on && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] && rtp_session->rtcp_recv_msg_p->header.version == 2) {
		/* ZRTP Recv */
		if (bytes) {
			unsigned int sbytes = (int) *bytes;
			zrtp_status_t stat = 0;

			stat = zrtp_process_srtcp(rtp_session->zrtp_stream, (void *) rtp_session->rtcp_recv_msg_p, &sbytes);

			switch (stat) {
			case zrtp_status_ok:
				*bytes = sbytes;
				break;
			case zrtp_status_drop:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection drop with code %d\n", stat);
				*bytes = 0;
				break;
			case zrtp_status_fail:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
				*bytes = 0;
				break;
			default:
				break;
			}
		}
	}
#endif

	/* RTCP Auto ADJ */
	if (*bytes && rtp_session->flags[SWITCH_RTP_FLAG_RTCP_AUTOADJ] &&  switch_sockaddr_get_port(rtp_session->rtcp_from_addr)) {
			if (!switch_cmp_addr(rtp_session->rtcp_from_addr, rtp_session->rtcp_remote_addr)) {
				if (++rtp_session->rtcp_autoadj_tally >= rtp_session->rtcp_autoadj_threshold) {
					const char *err;
					uint32_t old = rtp_session->remote_rtcp_port;
					const char *tx_host;
					const char *old_host;
					char bufa[50], bufb[50];

					tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtcp_from_addr);
					old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->rtcp_remote_addr);

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO,
									  "Auto Changing %s RTCP port from %s:%u to %s:%u\n", rtp_type(rtp_session), old_host, old, tx_host,
									  switch_sockaddr_get_port(rtp_session->rtcp_from_addr));


					rtp_session->eff_remote_host_str = switch_core_strdup(rtp_session->pool, tx_host);
					rtp_session->remote_rtcp_port = switch_sockaddr_get_port(rtp_session->rtcp_from_addr);
					status = enable_remote_rtcp_socket(rtp_session, &err);
					rtp_session->rtcp_auto_adj_used = 1;

					if ((rtp_session->rtp_bugs & RTP_BUG_ALWAYS_AUTO_ADJUST)) {
						switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_AUTOADJ);
					} else {
						switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_AUTOADJ);
					}
				}
			} else {

				if ((rtp_session->rtp_bugs & RTP_BUG_ALWAYS_AUTO_ADJUST)) {
					switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_AUTOADJ);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session),
									  SWITCH_LOG_DEBUG, "Correct %s RTCP ip/port confirmed.\n", rtp_type(rtp_session));
					switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_AUTOADJ);
				}
				rtp_session->rtcp_auto_adj_used = 0;

			}
	}

	if (*bytes) {
		return process_rtcp_packet(rtp_session, bytes);
	}

	return status;
}

static int rtp_common_read(switch_rtp_t *rtp_session, switch_payload_t *payload_type,
						   payload_map_t **pmapP, switch_frame_flag_t *flags, switch_io_flag_t io_flags)
{

	switch_channel_t *channel = NULL;
	switch_size_t bytes = 0;
	switch_size_t rtcp_bytes = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS, poll_status = SWITCH_STATUS_SUCCESS;
	switch_status_t rtcp_status = SWITCH_STATUS_SUCCESS, rtcp_poll_status = SWITCH_STATUS_SUCCESS;
	int check = 0;
	int ret = -1;
	int sleep_mss = 1000;
	int poll_sec = 5;
	int poll_loop = 0;
	int fdr = 0;
	int rtcp_fdr = 0;
	int hot_socket = 0;
	int read_loops = 0;
	int slept = 0;
	switch_bool_t got_jb = SWITCH_FALSE;

	if (!switch_rtp_ready(rtp_session)) {
		return -1;
	}

	if (rtp_session->session) {
		channel = switch_core_session_get_channel(rtp_session->session);
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
		sleep_mss = rtp_session->timer.interval * 1000;
	}

	READ_INC(rtp_session);



	while (switch_rtp_ready(rtp_session)) {
		int do_cng = 0;
		int read_pretriggered = 0;
		int has_rtcp = 0;
		int got_rtp_poll = 0;

		bytes = 0;

		if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] &&
			!rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] &&
			!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] &&
			!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] &&
			rtp_session->read_pollfd) {

			if (rtp_session->jb && !rtp_session->pause_jb && jb_valid(rtp_session)) {
				while (switch_poll(rtp_session->read_pollfd, 1, &fdr, 0) == SWITCH_STATUS_SUCCESS) {
					status = read_rtp_packet(rtp_session, &bytes, flags, pmapP, SWITCH_STATUS_SUCCESS, SWITCH_FALSE);

					if (status == SWITCH_STATUS_GENERR) {
						ret = -1;
						goto end;
					}

					if ((*flags & SFF_RTCP)) {
						*flags &= ~SFF_RTCP;
						has_rtcp = 1;
						read_pretriggered = 0;
						goto rtcp;
					}

					if (status == SWITCH_STATUS_BREAK) {
						read_pretriggered = 1;
						break;
					}
				}

			} else if ((rtp_session->flags[SWITCH_RTP_FLAG_AUTOFLUSH] || rtp_session->flags[SWITCH_RTP_FLAG_STICKY_FLUSH])) {

				if (switch_poll(rtp_session->read_pollfd, 1, &fdr, 0) == SWITCH_STATUS_SUCCESS) {
					status = read_rtp_packet(rtp_session, &bytes, flags, pmapP, SWITCH_STATUS_SUCCESS, SWITCH_FALSE);
					if (status == SWITCH_STATUS_GENERR) {
						ret = -1;
						goto end;
					}
					if ((*flags & SFF_RTCP)) {
						*flags &= ~SFF_RTCP;
						has_rtcp = 1;
						read_pretriggered = 0;
						goto rtcp;
					}

					/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Initial (%i) %d\n", status, bytes); */
					if (status != SWITCH_STATUS_FALSE) {
						read_pretriggered = 1;
					}

					if (bytes) {
						if (switch_poll(rtp_session->read_pollfd, 1, &fdr, 0) == SWITCH_STATUS_SUCCESS) {
							rtp_session->hot_hits++;//+= rtp_session->samples_per_interval;

							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG10, "%s Hot Hit %d\n",
											  rtp_session_name(rtp_session),
											  rtp_session->hot_hits);
						} else {
							rtp_session->hot_hits = 0;
						}
					}

					if (rtp_session->hot_hits > 1 && !rtp_session->sync_packets) {// >= (rtp_session->samples_per_second * 30)) {
						hot_socket = 1;
					}
				} else {
					rtp_session->hot_hits = 0;
				}
			}

			if (rtp_session->flags[SWITCH_RTP_FLAG_TEXT]) {
				///NOOP
			} else if (hot_socket && (rtp_session->hot_hits % 10) != 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG10, "%s timer while HOT\n", rtp_session_name(rtp_session));
				switch_core_timer_next(&rtp_session->timer);
			} else if (hot_socket) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG10, "%s skip timer once\n", rtp_session_name(rtp_session));
				rtp_session->sync_packets++;
				switch_core_timer_sync(&rtp_session->timer);
				reset_jitter_seq(rtp_session);
			} else {

				if (rtp_session->sync_packets) {

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG10,
									  "%s Auto-Flush catching up %d packets (%d)ms.\n",
									  rtp_session_name(rtp_session),
									  rtp_session->sync_packets, (rtp_session->ms_per_packet * rtp_session->sync_packets) / 1000);
					if (!rtp_session->flags[SWITCH_RTP_FLAG_PAUSE]) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "%s syncing %d %s packet(s)\n",
										 rtp_session_name(rtp_session),
										  rtp_session->sync_packets, rtp_type(rtp_session));

						rtp_session->bad_stream++;
						rtp_session->stats.inbound.flaws += rtp_session->sync_packets;

						if (rtp_session->stats.inbound.error_log) {
							rtp_session->stats.inbound.error_log->flaws += rtp_session->sync_packets;
						}
					}

					switch_core_timer_sync(&rtp_session->timer);
					reset_jitter_seq(rtp_session);
					rtp_session->hot_hits = 0;
				} else {
					if (slept) {
						switch_cond_next();
					} else {
						if (rtp_session->skip_timer) {
							rtp_session->skip_timer = 0;
							switch_cond_next();
						} else {
							switch_core_timer_next(&rtp_session->timer);
						}
						slept++;
					}

				}

				rtp_session->sync_packets = 0;
			}
		}

		rtp_session->stats.read_count++;

	recvfrom:

		if (!read_pretriggered) {
			bytes = 0;
		}
		read_loops++;
		//poll_loop = 0;

		if (!switch_rtp_ready(rtp_session)) {
			break;
		}

		if (!rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] && rtp_session->read_pollfd) {
			int pt = poll_sec * 1000000;

			do_2833(rtp_session);

			if (rtp_session->dtmf_data.out_digit_dur > 0 || rtp_session->dtmf_data.in_digit_sanity || rtp_session->sending_dtmf ||
				switch_queue_size(rtp_session->dtmf_data.dtmf_queue) || switch_queue_size(rtp_session->dtmf_data.dtmf_inqueue)) {
				pt = 20000;
			}

			if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA]) {
				pt = 100000;
			}

			if (rtp_session->vb && !rtp_session->pause_jb) {
				if (switch_jb_poll(rtp_session->vb)) {
					pt = 1000;
				}
			}

			if ((io_flags & SWITCH_IO_FLAG_NOBLOCK)) {
				pt = 0;
			}

			poll_status = switch_poll(rtp_session->read_pollfd, 1, &fdr, pt);

			if (!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && rtp_session->dtmf_data.out_digit_dur > 0) {
				return_cng_frame();
			}

			if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && rtp_session->flags[SWITCH_RTP_FLAG_BREAK]) {
				switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_BREAK);
				bytes = 0;
				reset_jitter_seq(rtp_session);
				return_cng_frame();
			}

		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
			got_jb = (rtp_session->vb && !rtp_session->pause_jb && switch_jb_poll(rtp_session->vb));
		} else {
			got_jb = SWITCH_TRUE;
		}

		if (poll_status == SWITCH_STATUS_SUCCESS || got_jb) {

			got_rtp_poll = 1;

			if (read_pretriggered) {
				read_pretriggered = 0;
			} else {
				
				
				status = read_rtp_packet(rtp_session, &bytes, flags, pmapP, poll_status, got_jb);

				if (status == SWITCH_STATUS_GENERR) {
					ret = -1;
					goto end;
				}

				if (rtp_session->max_missed_packets && read_loops == 1 && !rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] &&
					!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
					if (bytes && status == SWITCH_STATUS_SUCCESS) {
						rtp_session->missed_count = 0;
					} else if (++rtp_session->missed_count >= rtp_session->max_missed_packets) {
						ret = -2;
						goto end;
					}
				}

				if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
					//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_CRIT, "Read bytes (%i) %ld\n", status, bytes);

					if (bytes == 0) {
						if (check_rtcp_and_ice(rtp_session) == -1) {
							ret = -1;
							goto end;
						}
						// This is dumb
						//switch_rtp_video_refresh(rtp_session);
						goto  rtcp;
					}
				}

				if ((*flags & SFF_PROXY_PACKET)) {
					ret = (int) bytes;
					goto end;
				}

				if ((*flags & SFF_RTCP)) {
					*flags &= ~SFF_RTCP;
					has_rtcp = 1;
					goto rtcp;
				}


			}
			poll_loop = 0;
		} else {

			if (!switch_rtp_ready(rtp_session)) {
				ret = -1;
				goto end;
			}

			if (!SWITCH_STATUS_IS_BREAK(poll_status) && poll_status != SWITCH_STATUS_TIMEOUT) {
				char tmp[128] = "";
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Poll failed with error: %d [%s]\n",
								  poll_status, switch_strerror_r(poll_status, tmp, sizeof(tmp)));
				ret = -1;
				goto end;
			}

			if (!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] && !rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
				rtp_session->missed_count += (poll_sec * 1000) / (rtp_session->ms_per_packet ? rtp_session->ms_per_packet / 1000 : 20);
				bytes = 0;

				if (rtp_session->max_missed_packets) {
					if (rtp_session->missed_count >= rtp_session->max_missed_packets) {
						ret = -2;
						goto end;
					}
				}
			}


			if (check_rtcp_and_ice(rtp_session) == -1) {
				ret = -1;
				goto end;
			}


			if ((!(io_flags & SWITCH_IO_FLAG_NOBLOCK)) &&
				(rtp_session->dtmf_data.out_digit_dur == 0) && !rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
				return_cng_frame();
			}
		}

	rtcp:

		if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
			rtcp_poll_status = SWITCH_STATUS_FALSE;

			if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX] && has_rtcp) {
				if (rtp_session->rtcp_recv_msg_p->header.version == 2) { //rtcp muxed
					rtp_session->rtcp_from_addr = rtp_session->from_addr;
					rtcp_status = rtcp_poll_status = SWITCH_STATUS_SUCCESS;
					rtcp_bytes = bytes;
				}

				has_rtcp = 0;

			} else if (rtp_session->rtcp_read_pollfd) {
				rtcp_poll_status = switch_poll(rtp_session->rtcp_read_pollfd, 1, &rtcp_fdr, 0);
			}

			if (rtcp_poll_status == SWITCH_STATUS_SUCCESS) {

				if (!rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
					rtcp_status = read_rtcp_packet(rtp_session, &rtcp_bytes, flags);
				}

				if (rtcp_status == SWITCH_STATUS_SUCCESS) {
					switch_rtp_reset_media_timer(rtp_session);
					
					if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_PASSTHRU]) {
						switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);
						const char *uuid = switch_channel_get_partner_uuid(channel);

						if (uuid) {
							switch_core_session_t *other_session;
							switch_rtp_t *other_rtp_session = NULL;

							if ((other_session = switch_core_session_locate(uuid))) {
								switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
								if ((other_rtp_session = switch_channel_get_private(other_channel, "__rtcp_audio_rtp_session")) &&
									other_rtp_session->rtcp_sock_output &&
									switch_rtp_test_flag(other_rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
									other_rtp_session->rtcp_send_msg = rtp_session->rtcp_recv_msg;

#ifdef ENABLE_SRTP
									if (switch_rtp_test_flag(other_rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
										int stat = 0;
										int sbytes = (int) rtcp_bytes;
										
										if (!other_rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_MKI]) {
											stat = srtp_protect_rtcp(other_rtp_session->send_ctx[rtp_session->srtp_idx_rtcp], &other_rtp_session->rtcp_send_msg.header, &sbytes);
										} else {
											stat = srtp_protect_rtcp_mki(other_rtp_session->send_ctx[other_rtp_session->srtp_idx_rtcp], &other_rtp_session->rtcp_send_msg.header, &sbytes, 1, SWITCH_CRYPTO_MKI_INDEX);
										}

										if (stat) {
											switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error: SRTP RTCP protection failed with code %d\n", stat);
										}
										rtcp_bytes = sbytes;

									}
#endif

#ifdef ENABLE_ZRTP
									/* ZRTP Send */
									if (zrtp_on && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA]) {
										unsigned int sbytes = (unsigned int) bytes;
										zrtp_status_t stat = zrtp_status_fail;

										stat = zrtp_process_rtcp(other_rtp_session->zrtp_stream, (void *) &other_rtp_session->rtcp_send_msg, &sbytes);

										switch (stat) {
										case zrtp_status_ok:
											break;
										case zrtp_status_drop:
											switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error: zRTP protection drop with code %d\n", stat);
											ret = (int) bytes;
											goto end;
											break;
										case zrtp_status_fail:
											switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
											break;
										default:
											break;
										}

										bytes = sbytes;
									}
#endif
									if (switch_socket_sendto(other_rtp_session->rtcp_sock_output, other_rtp_session->rtcp_remote_addr, 0,
															 (const char*)&other_rtp_session->rtcp_send_msg, &rtcp_bytes ) != SWITCH_STATUS_SUCCESS) {
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG,"RTCP packet not written\n");
									}


								}
								switch_core_session_rwunlock(other_session);
							}
						}

					}

					if (rtp_session->flags[SWITCH_RTP_FLAG_RTCP_MUX]) {
						process_rtcp_packet(rtp_session, &rtcp_bytes);
						ret = 1;

						if (!rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] && rtp_session->timer.interval && !rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
							switch_core_timer_sync(&rtp_session->timer);
							reset_jitter_seq(rtp_session);
						}
						goto recvfrom;
					}
				}
			}
		}

		if ((!(io_flags & SWITCH_IO_FLAG_NOBLOCK)) &&
			(rtp_session->dtmf_data.out_digit_dur == 0) && !got_rtp_poll) {
			return_cng_frame();
		}

		if (!bytes && (io_flags & SWITCH_IO_FLAG_NOBLOCK)) {
			rtp_session->missed_count = 0;
			ret = 0;
			goto end;
		}

		check = !bytes;

		if (rtp_session->flags[SWITCH_RTP_FLAG_FLUSH]) {
			bytes = do_flush(rtp_session, SWITCH_FALSE, bytes);
			switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH);
		}

		if ((!bytes && rtp_session->flags[SWITCH_RTP_FLAG_BREAK]) || (bytes && bytes == 4 && *((int *) &rtp_session->recv_msg) == UINT_MAX)) {
			switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_BREAK);

			if (!rtp_session->flags[SWITCH_RTP_FLAG_NOBLOCK] || !rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] ||
				rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] ||
				(bytes && bytes < 5) || (!bytes && poll_loop)) {
				bytes = 0;
				reset_jitter_seq(rtp_session);
				return_cng_frame();
			}
		}

		if (bytes && bytes < 5) {
			continue;
		}

		if (!bytes && poll_loop) {
			goto recvfrom;
		}

		if (bytes && rtp_session->last_rtp_hdr.m && rtp_session->last_rtp_hdr.pt != rtp_session->recv_te &&
			!rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] &&
			!rtp_session->flags[SWITCH_RTP_FLAG_TEXT] &&
			!(rtp_session->rtp_bugs & RTP_BUG_IGNORE_MARK_BIT)) {
			rtp_flush_read_buffer(rtp_session, SWITCH_RTP_FLUSH_ONCE);
		}

		if (rtp_session->last_rtp_hdr.pt == rtp_session->cng_pt || rtp_session->last_rtp_hdr.pt == 13) {
			*flags |= SFF_NOT_AUDIO;
		} else {
			*flags &= ~SFF_NOT_AUDIO; /* If this flag was already set, make sure to remove it when we get real audio */
		}

		/* ignore packets not meant for us unless the auto-adjust window is open (ice mode has its own alternatives to this) */
		if (!using_ice(rtp_session) && bytes) {
			if (rtp_session->flags[SWITCH_RTP_FLAG_AUTOADJ]) {
				if (rtp_session->last_rtp_hdr.pt == rtp_session->cng_pt || rtp_session->last_rtp_hdr.pt == 13) {
					goto recvfrom;

				}
			} else if (!(rtp_session->rtp_bugs & RTP_BUG_ACCEPT_ANY_PACKETS) && !switch_cmp_addr(rtp_session->rtp_from_addr, rtp_session->remote_addr)) {
				goto recvfrom;
			}
		}

		if (bytes && rtp_session->flags[SWITCH_RTP_FLAG_AUTOADJ] && switch_sockaddr_get_port(rtp_session->rtp_from_addr)) {
			if (!switch_cmp_addr(rtp_session->rtp_from_addr, rtp_session->remote_addr)) {
				if (++rtp_session->autoadj_tally >= rtp_session->autoadj_threshold) {
					const char *err;
					uint32_t old = rtp_session->remote_port;
					const char *tx_host;
					const char *old_host;
					char bufa[50], bufb[50];
					char adj_port[6];

					tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtp_from_addr);
					old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO,
									  "Auto Changing %s port from %s:%u to %s:%u\n", rtp_type(rtp_session), old_host, old, tx_host,
									  switch_sockaddr_get_port(rtp_session->rtp_from_addr));

					if (channel) {
						char varname[80] = "";

						switch_snprintf(varname, sizeof(varname), "remote_%s_ip_reported", rtp_type(rtp_session));
						switch_channel_set_variable(channel, varname, switch_channel_get_variable(channel, "remote_media_ip"));

						switch_snprintf(varname, sizeof(varname), "remote_%s_ip", rtp_type(rtp_session));
						switch_channel_set_variable(channel, varname, tx_host);

						switch_snprintf(varname, sizeof(varname), "remote_%s_port_reported", rtp_type(rtp_session));
						switch_snprintf(adj_port, sizeof(adj_port), "%u", switch_sockaddr_get_port(rtp_session->rtp_from_addr));
						switch_channel_set_variable(channel, varname, switch_channel_get_variable(channel, "remote_media_port"));

						switch_snprintf(varname, sizeof(varname), "remote_%s_port", rtp_type(rtp_session));
						switch_channel_set_variable(channel, varname, adj_port);

						switch_snprintf(varname, sizeof(varname), "rtp_auto_adjust_%s", rtp_type(rtp_session));
						switch_channel_set_variable(channel, varname, "true");
					}
					rtp_session->auto_adj_used = 1;
					switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(rtp_session->rtp_from_addr), 0, SWITCH_FALSE, &err);
					if ((rtp_session->rtp_bugs & RTP_BUG_ALWAYS_AUTO_ADJUST)) {
						switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
						switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_AUTOADJ);
					} else {
						switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					}
					if (rtp_session->ice.ice_user) {
						rtp_session->ice.addr = rtp_session->remote_addr;
					}
				}
			} else {
				if ((rtp_session->rtp_bugs & RTP_BUG_ALWAYS_AUTO_ADJUST)) {
					switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
					switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_AUTOADJ);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Correct %s ip/port confirmed.\n", rtp_type(rtp_session));
					switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
				}
				rtp_session->auto_adj_used = 0;
			}
		}

		if (bytes && !(rtp_session->rtp_bugs & RTP_BUG_ALWAYS_AUTO_ADJUST) && rtp_session->autoadj_window) {
			if (--rtp_session->autoadj_window == 0) {
				switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_TEXT]) {
			if (!bytes) {
				if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
					switch_core_timer_next(&rtp_session->timer);
				}
				return_cng_frame();
			} else {
				*payload_type = rtp_session->last_rtp_hdr.pt;
				ret = (int) bytes;
				goto end;
			}
		}

		if (bytes && (rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL])) {
			/* Fast PASS! */
			*flags |= SFF_PROXY_PACKET;

			if (rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
#if 0
				if (rtp_session->has_rtp && check_recv_payload(rtp_session)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING,
									  "Ignoring udptl packet of size of %ld bytes that looks strikingly like a RTP packet.\n", (long)bytes);
					bytes = 0;
					goto do_continue;
				}
#endif
				*flags |= SFF_UDPTL_PACKET;
			}

			ret = (int) bytes;
			goto end;
		}

		if (bytes) {
			rtp_session->missed_count = 0;

			if (bytes < rtp_header_len) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "Ignoring invalid RTP packet size of %ld bytes.\n", (long)bytes);
				bytes = 0;
				goto do_continue;
			}

			if (rtp_session->last_rtp_hdr.pt && (rtp_session->last_rtp_hdr.pt == rtp_session->cng_pt || rtp_session->last_rtp_hdr.pt == 13)) {
				return_cng_frame();
			}
		}

		if (check || bytes) {
			do_2833(rtp_session);
		}

		if (bytes && rtp_session->recv_msg.header.version != 2) {
			uint8_t *data = (uint8_t *) RTP_BODY(rtp_session);

			//if (rtp_session->recv_msg.header.version == 0) {
			//	if (rtp_session->ice.ice_user) {
			//		handle_ice(rtp_session, &rtp_session->ice, (void *) &rtp_session->recv_msg, bytes);
			//		goto recvfrom;
			//	}
			//}

			if (rtp_session->invalid_handler) {
				rtp_session->invalid_handler(rtp_session, rtp_session->sock_input, (void *) &rtp_session->recv_msg, bytes, rtp_session->rtp_from_addr);
			}

			memset(data, 0, 2);
			data[0] = 65;

			rtp_session->last_rtp_hdr.pt = rtp_session->cng_pt != INVALID_PT ? rtp_session->cng_pt : SWITCH_RTP_CNG_PAYLOAD;
			*flags |= SFF_CNG;
			*payload_type = (switch_payload_t) rtp_session->last_rtp_hdr.pt;
			ret = 2 + rtp_header_len;
			goto end;
		} else if (bytes) {
			rtp_session->stats.inbound.period_packet_count++;
		}


		/* Handle incoming RFC2833 packets */
		switch (handle_rfc2833(rtp_session, bytes, &do_cng)) {
		case RESULT_GOTO_END:
			goto end;
		case RESULT_GOTO_RECVFROM:
			goto recvfrom;
		case RESULT_GOTO_TIMERCHECK:
			goto timer_check;
		case RESULT_CONTINUE:
			status = SWITCH_STATUS_SUCCESS;
			goto result_continue;
		}

	result_continue:
	timer_check:

		if (rtp_session->flags[SWITCH_RTP_FLAG_MUTE]) {
			do_cng++;
		}

		if (do_cng) {
			uint8_t *data = (uint8_t *) RTP_BODY(rtp_session);

			do_2833(rtp_session);

			if (rtp_session->last_cng_ts == rtp_session->last_read_ts + rtp_session->samples_per_interval) {
				rtp_session->last_cng_ts = 0;
			} else {
				rtp_session->last_cng_ts = rtp_session->last_read_ts + rtp_session->samples_per_interval;
			}

			memset(data, 0, 2);
			data[0] = 65;
			rtp_session->last_rtp_hdr.pt = rtp_session->cng_pt != INVALID_PT ? rtp_session->cng_pt : SWITCH_RTP_CNG_PAYLOAD;
			*flags |= SFF_CNG;
			*payload_type = (switch_payload_t) rtp_session->last_rtp_hdr.pt;
			ret = 2 + rtp_header_len;
			rtp_session->stats.inbound.skip_packet_count++;
			goto end;
		}


		if (check || (bytes && !rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER])) {
			if (!bytes && rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {	/* We're late! We're Late! */
				if (!rtp_session->flags[SWITCH_RTP_FLAG_NOBLOCK] && status == SWITCH_STATUS_BREAK) {
					switch_cond_next();
					continue;
				}



				if (!rtp_session->flags[SWITCH_RTP_FLAG_PAUSE] && !rtp_session->flags[SWITCH_RTP_FLAG_DTMF_ON] && !rtp_session->dtmf_data.in_digit_ts
					&& rtp_session->cng_count > (rtp_session->one_second * 2) && rtp_session->jitter_lead > JITTER_LEAD_FRAMES) {

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, "%s %s timeout\n",
									  rtp_session_name(rtp_session), rtp_type(rtp_session));

					if (rtp_session->stats.inbound.error_log) {
						rtp_session->stats.inbound.error_log->flaws++;
					}
					rtp_session->stats.inbound.flaws++;
					do_mos(rtp_session, SWITCH_FALSE);
				}

				rtp_session->cng_count++;
				return_cng_frame();
			}
		}

		rtp_session->cng_count = 0;

		if (status == SWITCH_STATUS_BREAK || bytes == 0) {
			if (!(io_flags & SWITCH_IO_FLAG_SINGLE_READ) && rtp_session->flags[SWITCH_RTP_FLAG_DATAWAIT]) {
				goto do_continue;
			}
			return_cng_frame();
		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_GOOGLEHACK] && rtp_session->last_rtp_hdr.pt == 102) {
			rtp_session->last_rtp_hdr.pt = 97;
		}

		break;

	do_continue:

		if (!bytes && !rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] && !switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {

			if (sleep_mss) {
				switch_yield(sleep_mss);
			}
		}

	}

	if (switch_rtp_ready(rtp_session)) {
		*payload_type = (switch_payload_t) rtp_session->last_rtp_hdr.pt;

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


SWITCH_DECLARE(switch_byte_t) switch_rtp_check_auto_adj(switch_rtp_t *rtp_session)
{
	return rtp_session->auto_adj_used;
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
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "RTP RECV DTMF %c:%d\n", dtmf->digit, dtmf->duration);
		bytes++;
		free(pop);
	}
	switch_mutex_unlock(rtp_session->dtmf_data.dtmf_mutex);

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
		if (rdigit->duration < switch_core_min_dtmf_duration(0)) {
			rdigit->duration = switch_core_min_dtmf_duration(0);
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
		if (rdigit->duration < switch_core_min_dtmf_duration(0)) {
			rdigit->duration = switch_core_min_dtmf_duration(0);
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

	bytes = rtp_common_read(rtp_session, payload_type, NULL, flags, io_flags);

	if (bytes < 0) {
		*datalen = 0;
		return bytes == -2 ? SWITCH_STATUS_TIMEOUT : SWITCH_STATUS_GENERR;
	} else if (bytes == 0) {
		*datalen = 0;
		return SWITCH_STATUS_BREAK;
	} else {
		if (bytes > rtp_header_len) {
			bytes -= rtp_header_len;
		}
	}

	*datalen = bytes;

	memcpy(data, RTP_BODY(rtp_session), bytes);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtcp_zerocopy_read_frame(switch_rtp_t *rtp_session, switch_rtcp_frame_t *frame)
{

	if (!rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
		return SWITCH_STATUS_FALSE;
	}

	/* A fresh frame has been found! */
	if (rtp_session->rtcp_fresh_frame) {
		/* turn the flag off! */
		rtp_session->rtcp_fresh_frame = 0;

		*frame = rtp_session->rtcp_frame;

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_TIMEOUT;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_zerocopy_read_frame(switch_rtp_t *rtp_session, switch_frame_t *frame, switch_io_flag_t io_flags)
{
	int bytes = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	bytes = rtp_common_read(rtp_session, &frame->payload, &frame->pmap, &frame->flags, io_flags);

	frame->data = RTP_BODY(rtp_session);

	if (!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] && (bytes < rtp_header_len || switch_test_flag(frame, SFF_CNG))) {
		frame->packet = NULL;
		frame->timestamp = 0;
		frame->seq = 0;
		frame->ssrc = 0;
		frame->m = 0;
	} else {

		frame->packet = &rtp_session->recv_msg;
		frame->packetlen = bytes;
		frame->source = __FILE__;

		switch_set_flag(frame, SFF_RAW_RTP);
		switch_set_flag(frame, SFF_EXTERNAL);
		if (frame->payload == rtp_session->recv_te) {
			switch_set_flag(frame, SFF_RFC2833);
		}
		frame->timestamp = ntohl(rtp_session->last_rtp_hdr.ts);
		frame->seq = (uint16_t) ntohs((uint16_t) rtp_session->last_rtp_hdr.seq);
		frame->ssrc = ntohl(rtp_session->last_rtp_hdr.ssrc);
		frame->m = rtp_session->last_rtp_hdr.m ? SWITCH_TRUE : SWITCH_FALSE;
	}

#ifdef ENABLE_ZRTP
	if (zrtp_on && rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV]) {
		zrtp_session_info_t zrtp_session_info;

		if (rtp_session->zrtp_session && (zrtp_status_ok == zrtp_session_get(rtp_session->zrtp_session, &zrtp_session_info))) {
			if (zrtp_session_info.sas_is_ready) {

				switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);

				const char *uuid = switch_channel_get_partner_uuid(channel);
				if (uuid) {
					switch_core_session_t *other_session;

					if ((other_session = switch_core_session_locate(uuid))) {
						switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
						switch_rtp_t *other_rtp_session = switch_channel_get_private(other_channel, "__zrtp_audio_rtp_session");

						if (other_rtp_session) {
							if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
								switch_mutex_lock(other_rtp_session->read_mutex);
								if (zrtp_status_ok == zrtp_session_get(other_rtp_session->zrtp_session, &zrtp_session_info)) {
									if (rtp_session->zrtp_mitm_tries > ZRTP_MITM_TRIES) {
										switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
										switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
										rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV] = 0;
										rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND] = 0;
									} else if (zrtp_status_ok == zrtp_resolve_mitm_call(other_rtp_session->zrtp_stream, rtp_session->zrtp_stream)) {
										rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV] = 0;
										rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND] = 0;
										switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
										switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
										rtp_session->zrtp_mitm_tries++;
									}
								}
								switch_mutex_unlock(other_rtp_session->read_mutex);
							}
						}

						switch_core_session_rwunlock(other_session);
					}
				}
			}
		} else {
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV] = 0;
			rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND] = 0;
		}
	}
#endif

	if (bytes < 0) {
		frame->datalen = 0;
		return bytes == -2 ? SWITCH_STATUS_TIMEOUT : SWITCH_STATUS_GENERR;
	} else if (!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
		if (bytes < rtp_header_len) {
			frame->datalen = 0;
			return SWITCH_STATUS_BREAK;
		} else {
			bytes -= rtp_header_len;
		}
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

	bytes = rtp_common_read(rtp_session, payload_type, NULL, flags, io_flags);
	*data = RTP_BODY(rtp_session);

	if (bytes < 0) {
		*datalen = 0;
		return SWITCH_STATUS_GENERR;
	} else {
		if (bytes > rtp_header_len) {
			bytes -= rtp_header_len;
		}
	}

	*datalen = bytes;
	return SWITCH_STATUS_SUCCESS;
}

static int rtp_write_ready(switch_rtp_t *rtp_session, uint32_t bytes, int line)
{
	if (!rtp_session) return 0;

	if (rtp_session->ice.ice_user && !(rtp_session->ice.rready || rtp_session->ice.ready)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "Skip sending %s packet %ld bytes (ice not ready @ line %d!)\n",
						  rtp_type(rtp_session), (long)bytes, line);
		return 0;
	}

	if (rtp_session->dtls && rtp_session->dtls->state != DS_READY) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG3, "Skip sending %s packet %ld bytes (dtls not ready @ line %d!)\n",
						  rtp_type(rtp_session), (long)bytes, line);
		return 0;
	}

	return 1;
}


static int rtp_common_write(switch_rtp_t *rtp_session,
							rtp_msg_t *send_msg, void *data, uint32_t datalen, switch_payload_t payload, uint32_t timestamp, switch_frame_flag_t *flags)
{
	switch_size_t bytes;
	uint8_t send = 1;
	uint32_t this_ts = 0;
	int ret;
	switch_time_t now;
	uint8_t m = 0;

	if (!switch_rtp_ready(rtp_session)) {
		return -1;
	}

	if (!rtp_write_ready(rtp_session, datalen, __LINE__)) {
		return 0;
	}

	WRITE_INC(rtp_session);
	
	if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
		//switch_core_timer_sync(&rtp_session->write_timer);
	}

	if (send_msg) {
		bytes = datalen;

		m = (uint8_t) send_msg->header.m;
		rtp_session->ts = ntohl(send_msg->header.ts);

		if (flags && *flags & SFF_RFC2833) {
			if (rtp_session->te == INVALID_PT) {
				ret = 0;
				goto end;
			}
			send_msg->header.pt = rtp_session->te;
		}
		data = send_msg->body;
		if (datalen > rtp_header_len) {
			datalen -= rtp_header_len;
		}
	} else {
		if (*flags & SFF_RFC2833) {
			if (rtp_session->te == INVALID_PT) {
				ret = 0;
				goto end;
			}
			payload = rtp_session->te;
		}

		send_msg = &rtp_session->send_msg;
		send_msg->header.pt = payload;

		m = get_next_write_ts(rtp_session, timestamp);

		rtp_session->send_msg.header.ts = htonl(rtp_session->ts);

		memcpy(send_msg->body, data, datalen);
		bytes = datalen + rtp_header_len;
	}

	if (!switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
		
		if ((rtp_session->rtp_bugs & RTP_BUG_NEVER_SEND_MARKER)) {
			m = 0;
		} else {
			int delta = rtp_session->ts - rtp_session->last_write_ts;

			if (!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] &&
				((!rtp_session->flags[SWITCH_RTP_FLAG_RESET] && (abs(delta) > rtp_session->samples_per_interval * 10))
				|| rtp_session->ts == rtp_session->samples_per_interval)) {
				m++;
			}

			if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
				//switch_core_timer_sync(&rtp_session->write_timer);
			}

			if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] &&
				(rtp_session->write_timer.samplecount - rtp_session->last_write_samplecount) > rtp_session->samples_per_interval * 10) {
				m++;
			}

			if (!rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER] &&
				((unsigned) ((switch_micro_time_now() - rtp_session->last_write_timestamp))) > (rtp_session->ms_per_packet * 10)) {
				m++;
			}

			if (rtp_session->cn && payload != rtp_session->cng_pt) {
				rtp_session->cn = 0;
				m++;
			}

			if (rtp_session->need_mark && !rtp_session->sending_dtmf) {
				m++;
				rtp_session->need_mark = 0;
			}
		}

		if (m) {
			rtp_session->flags[SWITCH_RTP_FLAG_RESET] = 1;
			rtp_session->ts = 0;
		}

		/* If the marker was set, and the timestamp seems to have started over - set a new SSRC, to indicate this is a new stream */
		if (m && !switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND) && (rtp_session->rtp_bugs & RTP_BUG_CHANGE_SSRC_ON_MARKER) &&
			(rtp_session->flags[SWITCH_RTP_FLAG_RESET] || (rtp_session->ts <= rtp_session->last_write_ts && rtp_session->last_write_ts > 0))) {
			switch_rtp_set_ssrc(rtp_session, (uint32_t) ((intptr_t) rtp_session + (uint32_t) switch_epoch_time_now(NULL)));
		}

		if (!switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) && !switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
			send_msg->header.m = (m && !(rtp_session->rtp_bugs & RTP_BUG_NEVER_SEND_MARKER)) ? 1 : 0;
		}
	}

	if (switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
		int external = (*flags & SFF_EXTERNAL);
		/* Normalize the timestamps to our own base by generating a made up starting point then adding the measured deltas to that base
		   so if the timestamps and ssrc of the source change, it will not break the other end's jitter bufffer / decoder etc *cough* CHROME *cough*
		 */

		if (!rtp_session->ts_norm.ts) {
			rtp_session->ts_norm.ts = (uint32_t) rand() % 1000000 + 1;
		}

		if (!rtp_session->ts_norm.last_ssrc || send_msg->header.ssrc != rtp_session->ts_norm.last_ssrc || rtp_session->ts_norm.last_external != external) {
			switch_core_session_t *other_session;
			
			switch_core_session_request_video_refresh(rtp_session->session);
			switch_core_media_gen_key_frame(rtp_session->session);

			if (switch_core_session_get_partner(rtp_session->session, &other_session) == SWITCH_STATUS_SUCCESS) {
				switch_core_session_request_video_refresh(other_session);
				switch_core_media_gen_key_frame(other_session);
				switch_core_session_rwunlock(other_session);
			}

			if (rtp_session->ts_norm.last_ssrc) {
				rtp_session->ts_norm.delta_ttl = 0;
				rtp_session->ts_norm.ts++;
			}

			rtp_session->ts_norm.last_ssrc = send_msg->header.ssrc;
			rtp_session->ts_norm.last_frame = ntohl(send_msg->header.ts);
		}

		rtp_session->ts_norm.last_external = external;

		if (ntohl(send_msg->header.ts) != rtp_session->ts_norm.last_frame) {
			int32_t delta = ntohl(send_msg->header.ts) - rtp_session->ts_norm.last_frame;

			if (delta < 0 || delta > 90000) {
				switch_core_media_gen_key_frame(rtp_session->session);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG1, 
								  "Timestamp shift detected last: %d this: %d delta: %d stick with prev delta: %d\n", 
								  rtp_session->ts_norm.last_frame, ntohl(send_msg->header.ts), delta, rtp_session->ts_norm.delta);
			} else {
				rtp_session->ts_norm.delta = delta;
			}

			rtp_session->ts_norm.ts += rtp_session->ts_norm.delta;

		}
		
		rtp_session->ts_norm.last_frame = ntohl(send_msg->header.ts);
		send_msg->header.ts = htonl(rtp_session->ts_norm.ts);
		this_ts = rtp_session->ts_norm.ts;
	}

	send_msg->header.ssrc = htonl(rtp_session->ssrc);

	if (rtp_session->flags[SWITCH_RTP_FLAG_GOOGLEHACK] && rtp_session->send_msg.header.pt == 97) {
		rtp_session->last_rtp_hdr.pt = 102;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_VAD] &&
		rtp_session->last_rtp_hdr.pt == rtp_session->vad_data.read_codec->implementation->ianacode) {

		int16_t decoded[SWITCH_RECOMMENDED_BUFFER_SIZE / sizeof(int16_t)] = { 0 };
		uint32_t rate = 0;
		uint32_t codec_flags = 0;
		uint32_t len = sizeof(decoded);
		time_t now = switch_epoch_time_now(NULL);
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

								rtp_session->vad_data.start_talking = switch_micro_time_now();

								if (!(rtp_session->rtp_bugs & RTP_BUG_NEVER_SEND_MARKER)) {
									send_msg->header.m = 1;
								}
								rtp_session->vad_data.hangover_hits = rtp_session->vad_data.hangunder_hits = rtp_session->vad_data.cng_count = 0;
								if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_EVENTS_TALK)) {

									if ((rtp_session->vad_data.fire_events & VAD_FIRE_TALK)) {
										switch_event_t *event;
										if (switch_event_create(&event, SWITCH_EVENT_TALK) == SWITCH_STATUS_SUCCESS) {
											switch_channel_event_set_data(switch_core_session_get_channel(rtp_session->vad_data.session), event);
											switch_event_fire(&event);
										}
									}
								}
							}
						} else {
							if (rtp_session->vad_data.hangunder_hits) {
								rtp_session->vad_data.hangunder_hits--;
							}
							if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING)) {
								if (++rtp_session->vad_data.hangover_hits >= rtp_session->vad_data.hangover) {
									rtp_session->vad_data.stop_talking = switch_micro_time_now();
									rtp_session->vad_data.total_talk_time += (rtp_session->vad_data.stop_talking - rtp_session->vad_data.start_talking);

									switch_clear_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING);

									rtp_session->vad_data.hangover_hits = rtp_session->vad_data.hangunder_hits = rtp_session->vad_data.cng_count = 0;
									if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_EVENTS_NOTALK)) {

										if ((rtp_session->vad_data.fire_events & VAD_FIRE_NOT_TALK)) {
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

	if (!switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
		uint32_t ts_delta;

		this_ts = ntohl(send_msg->header.ts);

		ts_delta = abs((int32_t)(this_ts - rtp_session->last_write_ts));

		if (ts_delta > rtp_session->samples_per_second * 2) {
			rtp_session->flags[SWITCH_RTP_FLAG_RESET] = 1;
		}
#ifdef DEBUG_TS_ROLLOVER
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "WRITE TS LAST:%u THIS:%u DELTA:%u\n", rtp_session->last_write_ts, this_ts, ts_delta);
#endif
		if (ts_delta == 0 || !switch_rtp_ready(rtp_session) || rtp_session->sending_dtmf) {
			send = 0;
		}
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_PAUSE]) {
		send = 0;
	}

	if (send) {
		int delta = 1;

		if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && (*flags & SFF_EXTERNAL) && 
			rtp_session->stats.outbound.packet_count && rtp_session->flags[SWITCH_RTP_FLAG_PASSTHRU]) {
			int32_t x = rtp_session->last_write_seq;
			int32_t y = ntohs(send_msg->header.seq);

			if (!rtp_session->video_delta_mode) {
				rtp_session->video_delta_mode = 1;
			} else {
				if (x > UINT16_MAX / 2 && y < UINT16_MAX / 2) {
					x -= (int32_t)UINT16_MAX+1;
				}
			
				delta = y-x;
			}
			
			rtp_session->last_write_seq = y;
		}

		if (!rtp_session->flags[SWITCH_RTP_FLAG_PASSTHRU]) {
			rtp_session->video_delta_mode = 0;
		}

		rtp_session->seq += delta;

		send_msg->header.seq = htons(rtp_session->seq);
		
		if (rtp_session->flags[SWITCH_RTP_FLAG_BYTESWAP] && send_msg->header.pt == rtp_session->payload) {
			switch_swap_linear((int16_t *)send_msg->body, (int) datalen);
		}

#ifdef ENABLE_SRTP
		switch_mutex_lock(rtp_session->ice_mutex);
		if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND]) {
			int sbytes = (int) bytes;
			srtp_err_status_t stat;


			if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_RESET] || !rtp_session->send_ctx[rtp_session->srtp_idx_rtp]) {

				switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET);
				srtp_dealloc(rtp_session->send_ctx[rtp_session->srtp_idx_rtp]);
				rtp_session->send_ctx[rtp_session->srtp_idx_rtp] = NULL;
				if ((stat = srtp_create(&rtp_session->send_ctx[rtp_session->srtp_idx_rtp],
										&rtp_session->send_policy[rtp_session->srtp_idx_rtp])) || !rtp_session->send_ctx[rtp_session->srtp_idx_rtp]) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR,
									  "Error! RE-Activating %s Secure RTP SEND\n", rtp_type(rtp_session));
					rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND] = 0;
					ret = -1;
					switch_mutex_unlock(rtp_session->ice_mutex);
					goto end;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO,
									  "RE-Activating %s Secure RTP SEND\n", rtp_type(rtp_session));
				}
			}

			if (!rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_MKI]) {
				stat = srtp_protect(rtp_session->send_ctx[rtp_session->srtp_idx_rtp], &send_msg->header, &sbytes);
			} else {
				stat = srtp_protect_mki(rtp_session->send_ctx[rtp_session->srtp_idx_rtp], &send_msg->header, &sbytes, 1, SWITCH_CRYPTO_MKI_INDEX);
			}

			if (stat) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR,
								  "Error: %s SRTP protection failed with code %d\n", rtp_type(rtp_session), stat);
			}

			bytes = sbytes;
		}
		switch_mutex_unlock(rtp_session->ice_mutex);
#endif
#ifdef ENABLE_ZRTP
		/* ZRTP Send */
		if (zrtp_on && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA]) {
			unsigned int sbytes = (int) bytes;
			zrtp_status_t stat = zrtp_status_fail;


			stat = zrtp_process_rtp(rtp_session->zrtp_stream, (void *) send_msg, &sbytes);

			switch (stat) {
			case zrtp_status_ok:
				break;
			case zrtp_status_drop:
				/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Error: zRTP protection drop with code %d\n", stat); */
				ret = (int) bytes;
				goto end;
				break;
			case zrtp_status_fail:
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
				break;
			default:
				break;
			}

			bytes = sbytes;
		}
#endif

		now = switch_micro_time_now();
#ifdef RTP_DEBUG_WRITE_DELTA
		{
			int delta = (int) (now - rtp_session->send_time) / 1000;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "WRITE %d delta %d\n", (int) bytes, delta);
		}
#endif
		rtp_session->send_time = now;

		if (rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE]) {
			const char *tx_host;
			const char *old_host;
			const char *my_host;

			char bufa[50], bufb[50], bufc[50];


			tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtp_from_addr);
			old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
			my_host = switch_get_addr(bufc, sizeof(bufc), rtp_session->local_addr);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(rtp_session->session), SWITCH_LOG_CONSOLE,
							  "W %s b=%4ld %s:%u %s:%u %s:%u pt=%d ts=%u seq=%u m=%d\n",
							  rtp_session->session ? switch_channel_get_name(switch_core_session_get_channel(rtp_session->session)) : "NoName",
							  (long) bytes,
							  my_host, switch_sockaddr_get_port(rtp_session->local_addr),
							  old_host, rtp_session->remote_port,
							  tx_host, switch_sockaddr_get_port(rtp_session->rtp_from_addr),
							  send_msg->header.pt, ntohl(send_msg->header.ts), ntohs(send_msg->header.seq), send_msg->header.m);

		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_NACK]) {
			switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);

			if (!rtp_session->vbw) {
				int nack_size = 100;
				const char *var;
				
				if ((var = switch_channel_get_variable(channel, "rtp_nack_buffer_size"))) {
					int tmp = atoi(var);
					
					if (tmp > 0 && tmp < 500) {
						nack_size = tmp;
					}
				}

				switch_jb_create(&rtp_session->vbw, SJB_VIDEO, nack_size, nack_size, rtp_session->pool);

				if (rtp_session->vbw) {
					switch_jb_set_flag(rtp_session->vbw, SJB_QUEUE_ONLY);
					//switch_jb_debug_level(rtp_session->vbw, 10);
				}
			}
			switch_jb_put_packet(rtp_session->vbw, (switch_rtp_packet_t *)send_msg, bytes);
		}

#ifdef RTP_WRITE_PLOSS
		{
			int r = (rand() % 10000) + 1;

			if (r <= 200) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ALERT,
								  "Simulate dropping packet ......... ts: %u seq: %u\n", ntohl(send_msg->header.ts), ntohs(send_msg->header.seq));
			} else {
				if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, (void *) send_msg, &bytes) != SWITCH_STATUS_SUCCESS) {
					rtp_session->seq--;
					ret = -1;
					goto end;
				}
			}
		}
#else
		//if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
		//
		//	rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_READ]++;
		//
		//	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "SEND %u\n", ntohs(send_msg->header.seq));
		//}
		if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, (void *) send_msg, &bytes) != SWITCH_STATUS_SUCCESS) {
			rtp_session->seq -= delta;

			ret = -1;
			goto end;
		}
#endif
		rtp_session->last_write_ts = this_ts;
		rtp_session->flags[SWITCH_RTP_FLAG_RESET] = 0;

		if (rtp_session->queue_delay) {
			rtp_session->delay_samples = rtp_session->queue_delay;
			rtp_session->queue_delay = 0;
		}

		rtp_session->stats.outbound.raw_bytes += bytes;
		rtp_session->stats.outbound.packet_count++;

		if (rtp_session->flags[SWITCH_RTP_FLAG_ENABLE_RTCP]) {
			rtp_session->stats.rtcp.sent_pkt_count++;
		}

		if (send_msg->header.pt == rtp_session->cng_pt) {
			rtp_session->stats.outbound.cng_packet_count++;
		} else {
			rtp_session->stats.outbound.media_packet_count++;
			rtp_session->stats.outbound.media_bytes += bytes;
		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_USE_TIMER]) {
			//switch_core_timer_sync(&rtp_session->write_timer);
			rtp_session->last_write_samplecount = rtp_session->write_timer.samplecount;
		}

		rtp_session->last_write_timestamp = switch_micro_time_now();
	}

	ret = (int) bytes;

 end:

	WRITE_DEC(rtp_session);

	return ret;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_disable_vad(switch_rtp_t *rtp_session)
{

	if (!rtp_session) {
		return SWITCH_STATUS_FALSE;
	}

	if (!rtp_session->flags[SWITCH_RTP_FLAG_VAD]) {
		return SWITCH_STATUS_GENERR;
	}
	switch_core_codec_destroy(&rtp_session->vad_data.vad_codec);
	switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_VAD);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_enable_vad(switch_rtp_t *rtp_session, switch_core_session_t *session, switch_codec_t *codec,
													  switch_vad_flag_t flags)
{
	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (rtp_session->flags[SWITCH_RTP_FLAG_VAD]) {
		return SWITCH_STATUS_GENERR;
	}

	memset(&rtp_session->vad_data, 0, sizeof(rtp_session->vad_data));

	if (switch_true(switch_channel_get_variable(switch_core_session_get_channel(rtp_session->session), "fire_talk_events"))) {
		rtp_session->vad_data.fire_events |= VAD_FIRE_TALK;
	}

	if (switch_true(switch_channel_get_variable(switch_core_session_get_channel(rtp_session->session), "fire_not_talk_events"))) {
		rtp_session->vad_data.fire_events |= VAD_FIRE_NOT_TALK;
	}


	if (switch_core_codec_init(&rtp_session->vad_data.vad_codec,
							   codec->implementation->iananame,
							   codec->implementation->modname,
							   NULL,
							   codec->implementation->samples_per_second,
							   codec->implementation->microseconds_per_packet / 1000,
							   codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_DEBUG, "Activate VAD codec %s %dms\n", codec->implementation->iananame,
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
	rtp_session->vad_data.next_scan = switch_epoch_time_now(NULL);
	rtp_session->vad_data.scan_freq = 0;
	if (switch_test_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_TALKING)) {
		rtp_session->vad_data.start_talking = switch_micro_time_now();
	}
	switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_VAD);
	switch_set_flag(&rtp_session->vad_data, SWITCH_VAD_FLAG_CNG);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_rtp_write_frame(switch_rtp_t *rtp_session, switch_frame_t *frame)
{
	uint8_t fwd = 0;
	void *data = NULL;
	uint32_t len, ts = 0;
	switch_payload_t payload = 0;
	rtp_msg_t *send_msg = NULL;
	srtp_hdr_t local_header;
	int r = 0;

	if (!switch_rtp_ready(rtp_session) || !rtp_session->remote_addr) {
		return -1;
	}

	if (!rtp_write_ready(rtp_session, frame->datalen, __LINE__)) {
		return 0;
	}

	//if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
	//	rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_READ]++;
	//	rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE]++;
	//}


	if (switch_test_flag(frame, SFF_PROXY_PACKET) || switch_test_flag(frame, SFF_UDPTL_PACKET) ||
		rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {

		//if (rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA] || rtp_session->flags[SWITCH_RTP_FLAG_UDPTL]) {
		switch_size_t bytes;
		//char bufa[50];

		/* Fast PASS! */
		if (!switch_test_flag(frame, SFF_PROXY_PACKET) && !switch_test_flag(frame, SFF_UDPTL_PACKET)) {
			return 0;
		}
		bytes = frame->packetlen;
		//tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->remote_addr);

		send_msg = frame->packet;

		if (!rtp_session->flags[SWITCH_RTP_FLAG_UDPTL] && !switch_test_flag(frame, SFF_UDPTL_PACKET)) {

			if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] && rtp_session->payload > 0) {
				send_msg->header.pt = rtp_session->payload;
			}

			send_msg->header.ssrc = htonl(rtp_session->ssrc);
			send_msg->header.seq = htons(++rtp_session->seq);
		}

		if (rtp_session->flags[SWITCH_RTP_FLAG_DEBUG_RTP_WRITE]) {
			const char *tx_host;
			const char *old_host;
			const char *my_host;

			char bufa[50], bufb[50], bufc[50];


			tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtp_from_addr);
			old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
			my_host = switch_get_addr(bufc, sizeof(bufc), rtp_session->local_addr);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(rtp_session->session), SWITCH_LOG_CONSOLE,
							  "W %s b=%4ld %s:%u %s:%u %s:%u pt=%d ts=%u seq=%u m=%d\n",
							  rtp_session->session ? switch_channel_get_name(switch_core_session_get_channel(rtp_session->session)) : "NoName",
							  (long) bytes,
							  my_host, switch_sockaddr_get_port(rtp_session->local_addr),
							  old_host, rtp_session->remote_port,
							  tx_host, switch_sockaddr_get_port(rtp_session->rtp_from_addr),
							  send_msg->header.pt, ntohl(send_msg->header.ts), ntohs(send_msg->header.seq), send_msg->header.m);

		}

		if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, frame->packet, &bytes) != SWITCH_STATUS_SUCCESS) {
			return -1;
		}


		rtp_session->stats.outbound.raw_bytes += bytes;
		rtp_session->stats.outbound.media_bytes += bytes;
		rtp_session->stats.outbound.media_packet_count++;
		rtp_session->stats.outbound.packet_count++;
		return (int) bytes;
	}
#ifdef ENABLE_ZRTP
	if (zrtp_on && rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND]) {
		zrtp_session_info_t zrtp_session_info;

		if (zrtp_status_ok == zrtp_session_get(rtp_session->zrtp_session, &zrtp_session_info)) {
			if (zrtp_session_info.sas_is_ready) {

				switch_channel_t *channel = switch_core_session_get_channel(rtp_session->session);

				const char *uuid = switch_channel_get_partner_uuid(channel);
				if (uuid) {
					switch_core_session_t *other_session;

					if ((other_session = switch_core_session_locate(uuid))) {
						switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
						switch_rtp_t *other_rtp_session = switch_channel_get_private(other_channel, "__zrtp_audio_rtp_session");


						if (other_rtp_session) {
							if (zrtp_status_ok == zrtp_session_get(other_rtp_session->zrtp_session, &zrtp_session_info)) {
								if (rtp_session->zrtp_mitm_tries > ZRTP_MITM_TRIES) {
									rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV] = 0;
									rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND] = 0;
									switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
									switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
								} else if (zrtp_status_ok == zrtp_resolve_mitm_call(other_rtp_session->zrtp_stream, rtp_session->zrtp_stream)) {
									rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_RECV] = 0;
									rtp_session->flags[SWITCH_ZRTP_FLAG_SECURE_MITM_SEND] = 0;
									switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
									switch_rtp_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
									rtp_session->zrtp_mitm_tries++;
								}
								rtp_session->zrtp_mitm_tries++;
							}
						}

						switch_core_session_rwunlock(other_session);
					}
				}
			}
		}
	}
#endif

	fwd = (rtp_session->flags[SWITCH_RTP_FLAG_RAW_WRITE] &&
		   (switch_test_flag(frame, SFF_RAW_RTP) || switch_test_flag(frame, SFF_RAW_RTP_PARSE_FRAME))) ? 1 : 0;

	if (!fwd && !rtp_session->sending_dtmf && !rtp_session->queue_delay && !rtp_session->flags[SWITCH_RTP_FLAG_VIDEO] &&
		rtp_session->flags[SWITCH_RTP_FLAG_RAW_WRITE] && (rtp_session->rtp_bugs & RTP_BUG_GEN_ONE_GEN_ALL)) {

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_WARNING, "Generating RTP locally but timestamp passthru is configured, disabling....\n");
		rtp_session->flags[SWITCH_RTP_FLAG_RAW_WRITE] = 0;
		rtp_session->flags[SWITCH_RTP_FLAG_RESET] = 1;
	}

	switch_assert(frame != NULL);

	if (switch_test_flag(frame, SFF_CNG)) {
		if (rtp_session->cng_pt != INVALID_PT) {
			payload = rtp_session->cng_pt;
		} else {
			return (int) frame->packetlen;
		}
	} else {
		payload = rtp_session->payload;
#if 0
		if (rtp_session->pmaps && *rtp_session->pmaps) {
			payload_map_t *pmap;
			for (pmap = *rtp_session->pmaps; pmap; pmap = pmap->next) {
				if (pmap->current) {
					payload = pmap->pt;
				}
			}
		}
#endif
	}


	if (switch_test_flag(frame, SFF_RTP_HEADER) || rtp_session->flags[SWITCH_RTP_FLAG_TEXT]) {
		switch_size_t wrote;

		wrote = switch_rtp_write_manual(rtp_session, frame->data, frame->datalen,
										frame->m, frame->payload, (uint32_t) (frame->timestamp), &frame->flags);

		rtp_session->stats.outbound.raw_bytes += wrote;
		rtp_session->stats.outbound.media_bytes += wrote;
		rtp_session->stats.outbound.media_packet_count++;
		rtp_session->stats.outbound.packet_count++;

		return wrote;
	}

	if (frame->pmap && rtp_session->pmaps && *rtp_session->pmaps) {
		payload_map_t *pmap;

		switch_mutex_lock(rtp_session->flag_mutex);
		for (pmap = *rtp_session->pmaps; pmap; pmap = pmap->next) {
			if (pmap->negotiated && pmap->hash == frame->pmap->hash) {
				payload = pmap->recv_pt;
				break;
			}
		}
		switch_mutex_unlock(rtp_session->flag_mutex);
	}

	if (fwd) {
		send_msg = frame->packet;
		local_header = send_msg->header;
		len = frame->packetlen;
		ts = 0;

		send_msg->header.pt = payload;

		if (switch_test_flag(frame, SFF_RAW_RTP_PARSE_FRAME)) {
			send_msg->header.version = 2;
			send_msg->header.m = frame->m;

			send_msg->header.ts = htonl(frame->timestamp);
			if (frame->ssrc) {
				send_msg->header.ssrc = htonl(frame->ssrc);
			} else {
				send_msg->header.ssrc = htonl(rtp_session->ssrc);
			}
		}

	} else {
		data = frame->data;
		len = frame->datalen;
		ts = rtp_session->flags[SWITCH_RTP_FLAG_RAW_WRITE] ? (uint32_t) frame->timestamp : 0;
		if (!ts) ts = rtp_session->last_write_ts + rtp_session->samples_per_interval;
	}

	/*
	  if (rtp_session->flags[SWITCH_RTP_FLAG_VIDEO]) {
	  send_msg->header.pt = rtp_session->payload;
	  }
	*/

	r = rtp_common_write(rtp_session, send_msg, data, len, payload, ts, &frame->flags);

	if (send_msg) {
		send_msg->header = local_header;
	}

	return r;

}

SWITCH_DECLARE(switch_rtp_stats_t *) switch_rtp_get_stats(switch_rtp_t *rtp_session, switch_memory_pool_t *pool)
{
	switch_rtp_stats_t *s;

	if (!rtp_session) {
		return NULL;
	}

	switch_mutex_lock(rtp_session->flag_mutex);
	if (pool) {
		s = switch_core_alloc(pool, sizeof(*s));
		*s = rtp_session->stats;
	} else {
		s = &rtp_session->stats;
	}

	if (rtp_session->jb) {
		switch_jb_get_frames(rtp_session->jb, NULL, NULL, NULL, (uint32_t *)&s->inbound.largest_jb_size);
	}

	do_mos(rtp_session, SWITCH_FALSE);

	switch_mutex_unlock(rtp_session->flag_mutex);

	return s;
}

SWITCH_DECLARE(int) switch_rtp_write_manual(switch_rtp_t *rtp_session,
											void *data, uint32_t datalen, uint8_t m, switch_payload_t payload, uint32_t ts, switch_frame_flag_t *flags)
{
	switch_size_t bytes;
	int ret = -1;

	if (!switch_rtp_ready(rtp_session) || !rtp_session->remote_addr || datalen > SWITCH_RTP_MAX_BUF_LEN) {
		return -1;
	}

	if (!rtp_write_ready(rtp_session, datalen, __LINE__)) {
		return 0;
	}

	if (payload == INVALID_PT) {
		return 0;
	}

	WRITE_INC(rtp_session);

	rtp_session->write_msg = rtp_session->send_msg;
	rtp_session->write_msg.header.seq = htons(++rtp_session->seq);
	rtp_session->write_msg.header.ts = htonl(ts);
	rtp_session->write_msg.header.pt = payload;
	rtp_session->write_msg.header.m = m;
	memcpy(rtp_session->write_msg.body, data, datalen);

	bytes = rtp_header_len + datalen;

	if (switch_rtp_write_raw(rtp_session, (void *) &rtp_session->write_msg, &bytes, SWITCH_TRUE) != SWITCH_STATUS_SUCCESS) {
		rtp_session->seq--;
		ret = -1;
		goto end;
	}

	if (((*flags) & SFF_RTP_HEADER)) {
		rtp_session->last_write_ts = ts;
		rtp_session->flags[SWITCH_RTP_FLAG_RESET] = 0;
	}

	ret = (int) bytes;

 end:

	WRITE_DEC(rtp_session);

	return ret;
}



SWITCH_DECLARE(switch_status_t) switch_rtp_write_raw(switch_rtp_t *rtp_session, void *data, switch_size_t *bytes, switch_bool_t process_encryption)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(bytes);

	if (!switch_rtp_ready(rtp_session) || !rtp_session->remote_addr || *bytes > SWITCH_RTP_MAX_BUF_LEN) {
		return status;
	}

	if (!rtp_write_ready(rtp_session, *bytes, __LINE__)) {
		return SWITCH_STATUS_NOT_INITALIZED;
	}

	WRITE_INC(rtp_session);

	if (process_encryption) {
		process_encryption = SWITCH_FALSE;
#ifdef ENABLE_SRTP
		if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND]) {

			int sbytes = (int) *bytes;
			srtp_err_status_t stat;

			if (rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_RESET]) {
				switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET);
				srtp_dealloc(rtp_session->send_ctx[rtp_session->srtp_idx_rtp]);
				rtp_session->send_ctx[rtp_session->srtp_idx_rtp] = NULL;
				if ((stat = srtp_create(&rtp_session->send_ctx[rtp_session->srtp_idx_rtp],
										&rtp_session->send_policy[rtp_session->srtp_idx_rtp])) || !rtp_session->send_ctx[rtp_session->srtp_idx_rtp]) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP SEND\n");
					rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND] = 0;
					status = SWITCH_STATUS_FALSE;
					goto end;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_INFO, "RE-Activating Secure RTP SEND\n");
				}
			}

			if (!rtp_session->flags[SWITCH_RTP_FLAG_SECURE_SEND_MKI]) {
				stat = srtp_protect(rtp_session->send_ctx[rtp_session->srtp_idx_rtp], &rtp_session->write_msg.header, &sbytes);
			} else {
				stat = srtp_protect_mki(rtp_session->send_ctx[rtp_session->srtp_idx_rtp], &rtp_session->write_msg.header, &sbytes, 1, SWITCH_CRYPTO_MKI_INDEX);
			}

			if (stat) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(rtp_session->session), SWITCH_LOG_ERROR, "Error: SRTP protection failed with code %d\n", stat);
			}
			*bytes = sbytes;
		}
#endif
#ifdef ENABLE_ZRTP
		/* ZRTP Send */
		if (zrtp_on && !rtp_session->flags[SWITCH_RTP_FLAG_PROXY_MEDIA]) {
			unsigned int sbytes = (int) *bytes;
			zrtp_status_t stat = zrtp_status_fail;

			stat = zrtp_process_rtp(rtp_session->zrtp_stream, (void *) &rtp_session->write_msg, &sbytes);

			switch (stat) {
			case zrtp_status_ok:
				break;
			case zrtp_status_drop:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection drop with code %d\n", stat);
				status = SWITCH_STATUS_SUCCESS;
				goto end;
				break;
			case zrtp_status_fail:
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
				break;
			default:
				break;
			}

			*bytes = sbytes;
		}
#endif
	}

	status = switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, data, bytes);
#if defined(ENABLE_SRTP) || defined(ENABLE_ZRTP)
 end:
#endif

	WRITE_DEC(rtp_session);

	return status;
}

SWITCH_DECLARE(uint32_t) switch_rtp_get_ssrc(switch_rtp_t *rtp_session)
{
	return rtp_session->ssrc;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

