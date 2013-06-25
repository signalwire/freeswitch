/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * switch_rtp.c -- RTP
 *
 */
//#define DEBUG_2833
//#define RTP_DEBUG_WRITE_DELTA
//#define DEBUG_MISSED_SEQ

#include <switch.h>
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
#include <datatypes.h>
#include <srtp.h>

#define READ_INC(rtp_session) switch_mutex_lock(rtp_session->read_mutex); rtp_session->reading++
#define READ_DEC(rtp_session)  switch_mutex_unlock(rtp_session->read_mutex); rtp_session->reading--
#define WRITE_INC(rtp_session)  switch_mutex_lock(rtp_session->write_mutex); rtp_session->writing++
#define WRITE_DEC(rtp_session) switch_mutex_unlock(rtp_session->write_mutex); rtp_session->writing--



#define rtp_header_len 12
#define RTP_START_PORT 16384
#define RTP_END_PORT 32768
#define MASTER_KEY_LEN   30
#define RTP_MAGIC_NUMBER 42
#define MAX_SRTP_ERRS 10
#define RTP_TS_RESET 1

static switch_port_t START_PORT = RTP_START_PORT;
static switch_port_t END_PORT = RTP_END_PORT;
static switch_port_t NEXT_PORT = RTP_START_PORT;
static switch_mutex_t *port_lock = NULL;
static void do_flush(switch_rtp_t *rtp_session);

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
	char body[SWITCH_RTP_MAX_BUF_LEN];
	switch_rtp_hdr_ext_t *ext;
	char *ebody;
} rtp_msg_t;

#define RTP_BODY(_s) (char *) (_s->recv_msg.ebody ? _s->recv_msg.ebody : _s->recv_msg.body)

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
	char *pass;
	uint32_t stuncount;
	uint32_t funny_stun;
	uint32_t default_stuncount;
} switch_rtp_ice_t;

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

	switch_sockaddr_t *remote_addr, *rtcp_remote_addr;
	rtp_msg_t recv_msg;
	rtcp_msg_t rtcp_recv_msg;

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
	int8_t sending_dtmf;
	uint8_t need_mark;
	switch_payload_t payload;
	switch_payload_t rpayload;
	switch_rtp_invalid_handler_t invalid_handler;
	void *private_data;
	uint32_t ts;
	uint32_t last_write_ts;
	uint32_t last_read_ts;
	uint32_t last_cng_ts;
	uint32_t last_write_samplecount;
	uint32_t delay_samples;
	uint32_t next_write_samplecount;
	uint32_t max_next_write_samplecount;
	uint32_t queue_delay;
	switch_time_t last_write_timestamp;
	uint32_t flags;
	switch_memory_pool_t *pool;
	switch_sockaddr_t *from_addr, *rtcp_from_addr;
	char *rx_host;
	switch_port_t rx_port;
	switch_rtp_ice_t ice;
	switch_rtp_ice_t rtcp_ice;
	char *timer_name;
	char *local_host_str;
	char *remote_host_str;
	char *eff_remote_host_str;
	switch_time_t last_stun;
	uint32_t samples_per_interval;
	uint32_t samples_per_second;
	uint32_t conf_samples_per_interval;
	uint32_t rsamples_per_interval;
	uint32_t ms_per_packet;
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
	switch_timer_t timer;
	uint8_t ready;
	uint8_t cn;
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
	switch_rtp_bug_flag_t rtp_bugs;
	switch_rtp_stats_t stats;
	uint32_t hot_hits;
	uint32_t sync_packets;
	int rtcp_interval;
	switch_bool_t rtcp_fresh_frame;

	switch_time_t send_time;
	switch_byte_t auto_adj_used;
	uint8_t pause_jb;
	uint16_t last_seq;
	switch_time_t last_read_time;
	switch_size_t last_flush_packet_count;
	uint32_t interdigit_delay;

#ifdef ENABLE_ZRTP
	zrtp_session_t *zrtp_session;
	zrtp_profile_t *zrtp_profile;
	zrtp_stream_t *zrtp_stream;
	int zrtp_mitm_tries;
	int zinit;
#endif


};

struct switch_rtcp_source {
       unsigned ssrc1:32;
       unsigned fraction_lost:8;
       unsigned cumulative_lost:24;
       unsigned hi_seq_recieved:32;
       unsigned interarrival_jitter:32;
       unsigned lsr:32;
       unsigned lsr_delay:32;
};

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN
struct switch_rtcp_s_desc_head {
       unsigned v:2;
       unsigned padding:1;
       unsigned sc:5;
       unsigned pt:8;
       unsigned length:16;
};

#else /*  BIG_ENDIAN */
struct switch_rtcp_s_desc_head {
       unsigned sc:5;
       unsigned padding:1;
       unsigned v:2;
       unsigned pt:8;
       unsigned length:16;
};
#endif

struct switch_rtcp_s_desc_trunk {
       unsigned ssrc:32;
       unsigned cname:8;
       unsigned length:8;
       char text[1]; 
};


struct switch_rtcp_senderinfo {
	unsigned ssrc:32;
	unsigned ntp_msw:32;
	unsigned ntp_lsw:32;
	unsigned ts:32;
	unsigned pc:32;
	unsigned oc:32;
       struct switch_rtcp_source sr_source;
       struct switch_rtcp_s_desc_head sr_desc_head;
       struct switch_rtcp_s_desc_trunk sr_desc_ssrc;

};

typedef enum {
	RESULT_CONTINUE,
	RESULT_GOTO_END,
	RESULT_GOTO_RECVFROM,
	RESULT_GOTO_TIMERCHECK
} handle_rfc2833_result_t;

static void do_2833(switch_rtp_t *rtp_session, switch_core_session_t *session);

static handle_rfc2833_result_t handle_rfc2833(switch_rtp_t *rtp_session, switch_size_t bytes, int *do_cng)
{

#ifdef DEBUG_2833
	if (rtp_session->dtmf_data.in_digit_sanity && !(rtp_session->dtmf_data.in_digit_sanity % 100)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sanity %d %ld\n", rtp_session->dtmf_data.in_digit_sanity, bytes);
	}
#endif

	if (rtp_session->dtmf_data.in_digit_sanity && !--rtp_session->dtmf_data.in_digit_sanity) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		rtp_session->dtmf_data.last_digit = 0;
		rtp_session->dtmf_data.in_digit_ts = 0;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed DTMF sanity check.\n");
	}

	/* RFC2833 ... like all RFC RE: VoIP, guaranteed to drive you to insanity! 
	   We know the real rules here, but if we enforce them, it's an interop nightmare so,
	   we put up with as much as we can so we don't have to deal with being punished for
	   doing it right. Nice guys finish last!
	*/
	if (bytes > rtp_header_len && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) &&
		!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PASS_RFC2833) && rtp_session->recv_te && rtp_session->recv_msg.header.pt == rtp_session->recv_te) {
		switch_size_t len = bytes - rtp_header_len;
		unsigned char *packet = (unsigned char *) RTP_BODY(rtp_session);
		int end;
		uint16_t duration;
		char key;
		uint16_t in_digit_seq;
		uint32_t ts;

		if (!(packet[0] || packet[1] || packet[2] || packet[3]) && len >= 8) {

			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			packet += 4;
			len -= 4;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "DTMF payload offset by 4 bytes.\n");
		}

		if (!(packet[0] || packet[1] || packet[2] || packet[3]) && rtp_session->dtmf_data.in_digit_ts) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed DTMF payload check.\n");
			rtp_session->dtmf_data.last_digit = 0;
			rtp_session->dtmf_data.in_digit_ts = 0;
			rtp_session->dtmf_data.in_digit_sanity = 0;
		}

		end = packet[1] & 0x80 ? 1 : 0;
		duration = (packet[2] << 8) + packet[3];
		key = switch_rfc2833_to_char(packet[0]);
		in_digit_seq = ntohs((uint16_t) rtp_session->recv_msg.header.seq);
		ts = htonl(rtp_session->recv_msg.header.ts);

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
				   ts, duration, rtp_session->recv_msg.header.m, end, end && !rtp_session->dtmf_data.in_digit_ts ? "ignored" : "");
#endif

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
					stfu_n_reset(rtp_session->jb);
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
				   key, in_digit_seq, rtp_session->dtmf_data.in_digit_seq, ts, duration, rtp_session->recv_msg.header.m, end);
#endif
			switch_cond_next();
			return RESULT_GOTO_RECVFROM;
		}
	}

	if (bytes && rtp_session->dtmf_data.in_digit_ts) {
		if (!switch_rtp_ready(rtp_session)) {
			return RESULT_GOTO_END;
		}

		if (!rtp_session->dtmf_data.in_interleaved && rtp_session->recv_msg.header.pt != rtp_session->recv_te) {
			/* Drat, they are sending audio still as well as DTMF ok fine..... *sigh* */
			rtp_session->dtmf_data.in_interleaved = 1;
		}
			
		if (rtp_session->dtmf_data.in_interleaved || (rtp_session->rtp_bugs & RTP_BUG_IGNORE_DTMF_DURATION)) {
			if (rtp_session->recv_msg.header.pt == rtp_session->recv_te) {
				return RESULT_GOTO_RECVFROM;
			}
		} else {
			*do_cng = 1;
			return RESULT_GOTO_TIMERCHECK;
		}
	}

	return RESULT_CONTINUE;
}

struct switch_rtcp_report_block {
	uint32_t ssrc; /* The SSRC identifier of the source to which the information in this reception report block pertains. */
	unsigned int fraction :8; /* The fraction of RTP data packets from source SSRC_n lost since the previous SR or RR packet was sent */
	int lost :24; /* The total number of RTP data packets from source SSRC_n that have been lost since the beginning of reception */
	uint32_t highest_sequence_number_received;
	uint32_t jitter; /* An estimate of the statistical variance of the RTP data packet interarrival time, measured in timestamp units and expressed as an unsigned integer. */
	uint32_t lsr; /* The middle 32 bits out of 64 in the NTP timestamp */
	uint32_t dlsr; /* The delay, expressed in units of 1/65536 seconds, between receiving the last SR packet from source SSRC_n and sending this reception report block */
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

	if (rtp_session->ice.stuncount != 0) {
		rtp_session->ice.stuncount--;
		goto end;
	}
#if 0
	if (rtp_session->last_stun) {
		elapsed = (unsigned int) ((switch_micro_time_now() - rtp_session->last_stun) / 1000);

		if (elapsed > 30000) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No stun for a long time (PUNT!)\n");
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	}
#endif

	if (rtp_session->ice.funny_stun) {
		*start++ = 0;
		*start++ = 0;
		*start++ = 0x22;
		*start++ = 0x22;
	}

	packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, start);
	bytes = switch_stun_packet_length(packet);

	if (rtp_session->ice.funny_stun) {
		packet = (switch_stun_packet_t *) buf;
		bytes += 4;
	}


	switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_stun_addr, 0, (void *) packet, &bytes);
	rtp_session->ice.stuncount = rtp_session->ice.default_stuncount;

 end:
	WRITE_DEC(rtp_session);

	return status;
}

static switch_status_t ice_out(switch_rtp_t *rtp_session, switch_rtp_ice_t *ice)
{
	uint8_t buf[256] = { 0 };
	switch_stun_packet_t *packet;
	unsigned int elapsed;
	switch_size_t bytes;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_sockaddr_t *remote_addr = rtp_session->remote_addr;
	switch_socket_t *sock_output = rtp_session->sock_output;
	
	if (ice == &rtp_session->rtcp_ice) {
		sock_output = rtp_session->rtcp_sock_output;		
		remote_addr = rtp_session->rtcp_remote_addr;
	}

	switch_assert(rtp_session != NULL);
	switch_assert(ice->ice_user != NULL);

	READ_INC(rtp_session);

	if (ice->stuncount != 0) {
		ice->stuncount--;
		goto end;
	}

	if (rtp_session->last_stun) {
		elapsed = (unsigned int) ((switch_micro_time_now() - rtp_session->last_stun) / 1000);

		if (elapsed > 30000) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No stun for a long time (PUNT!)\n");
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
	}

	packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
	switch_stun_packet_attribute_add_username(packet, ice->ice_user, 32);
	if (ice->pass) {
		switch_stun_packet_attribute_add_password(packet, ice->pass, (uint16_t)strlen(ice->pass));
	}
	bytes = switch_stun_packet_length(packet);

	switch_socket_sendto(sock_output, remote_addr, 0, (void *) packet, &bytes);
						 
	ice->stuncount = ice->default_stuncount;


 end:
	READ_DEC(rtp_session);

	return status;
}


static void handle_stun_ping_reply(switch_rtp_t *rtp_session, void *data, switch_size_t len)
{
	if (!switch_rtp_ready(rtp_session)) {
		return;
	}

	rtp_session->last_stun = switch_micro_time_now();
}

static void handle_ice(switch_rtp_t *rtp_session, switch_rtp_ice_t *ice, void *data, switch_size_t len)
{
	switch_stun_packet_t *packet;
	switch_stun_packet_attribute_t *attr;
	void *end_buf;
	char username[33] = { 0 };
	unsigned char buf[512] = { 0 };
	switch_size_t cpylen = len;

	if (!switch_rtp_ready(rtp_session) || zstr(ice->user_ice) || zstr(ice->ice_user)) {
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
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		//int sbytes = (int) cpylen;
		//int stat;

		//if ((stat = srtp_unprotect(rtp_session->recv_ctx, buf, &sbytes)) || !(packet = switch_stun_packet_parse(buf, sizeof(buf)))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid STUN/ICE packet received\n");
			goto end;
			//}
	}
	end_buf = buf + ((sizeof(buf) > packet->header.length) ? packet->header.length : sizeof(buf));

	rtp_session->last_stun = switch_micro_time_now();

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

	if ((packet->header.type == SWITCH_STUN_BINDING_REQUEST) && !strcmp(ice->user_ice, username)) {
		uint8_t stunbuf[512];
		switch_stun_packet_t *rpacket;
		const char *remote_ip;
		switch_size_t bytes;
		char ipbuf[25];
		//int rtcp = 0;
		switch_sockaddr_t *from_addr = rtp_session->from_addr;
		switch_socket_t *sock_output = rtp_session->sock_output;
		
		if (ice == &rtp_session->rtcp_ice) {
			//rtcp = 1;
			from_addr = rtp_session->rtcp_from_addr;
			sock_output = rtp_session->rtcp_sock_output;

		}


		memset(stunbuf, 0, sizeof(stunbuf));
		rpacket = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, packet->header.id, stunbuf);
		switch_stun_packet_attribute_add_username(rpacket, username, 32);
		remote_ip = switch_get_addr(ipbuf, sizeof(ipbuf), from_addr);
		switch_stun_packet_attribute_add_binded_address(rpacket, (char *) remote_ip, switch_sockaddr_get_port(from_addr));
		bytes = switch_stun_packet_length(rpacket);
		switch_socket_sendto(sock_output, from_addr, 0, (void *) rpacket, &bytes);

	} else if (packet->header.type == SWITCH_STUN_BINDING_ERROR_RESPONSE) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		
		ice_out(rtp_session, ice);

		if (session) {
			switch_core_session_message_t msg = { 0 };
			msg.from = __FILE__;
			msg.numeric_arg = packet->header.type;
			msg.pointer_arg = packet;
			msg.message_id = SWITCH_MESSAGE_INDICATE_STUN_ERROR;
			switch_core_session_receive_message(session, &msg);			
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
							  "STUN/ICE binding error received on %s channel\n", switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) ? "video" : "audio");
		}

	}

 end:

	READ_DEC(rtp_session);
	WRITE_DEC(rtp_session);
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

	switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, rtp_packet, &len);
	return status;
}

static void zrtp_event_callback(zrtp_stream_t *stream, unsigned event)
{
	switch_rtp_t *rtp_session = zrtp_stream_get_userdata(stream);
	zrtp_session_info_t zrtp_session_info;
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *fsevent = NULL;
	const char *type;

	type = switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) ? "video" : "audio";

	switch (event) {
	case ZRTP_EVENT_IS_SECURE:
		{
			switch_set_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_SEND);
			switch_set_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_RECV);
			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
				switch_set_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
				switch_set_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
			}
			if (zrtp_status_ok == zrtp_session_get(stream->session, &zrtp_session_info)) {
				if (zrtp_session_info.sas_is_ready) {

					switch_channel_set_variable_name_printf(channel, "true", "zrtp_secure_media_confirmed_%s", type);
					switch_channel_set_variable_name_printf(channel, stream->session->sas1.buffer, "zrtp_sas1_string_%s", type);
					switch_channel_set_variable_name_printf(channel, stream->session->sas2.buffer, "zrtp_sas2_string", type);
					zrtp_verified_set(zrtp_global, &stream->session->zid, &stream->session->peer_zid, (uint8_t)1);
				}
			}

			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
				switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");

				if (session) {
					switch_channel_t *channel = switch_core_session_get_channel(session);
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
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Enrolled complete!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_enroll_complete_%s", type);
		}
		break;

	case ZRTP_EVENT_USER_ALREADY_ENROLLED:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "User already enrolled!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_already_enrolled_%s", type);
		}
		break;

	case ZRTP_EVENT_NEW_USER_ENROLLED:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "New user enrolled!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_new_user_enrolled_%s", type);
		}
		break;

	case ZRTP_EVENT_USER_UNENROLLED:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "User unenrolled!\n");
			switch_channel_set_variable_name_printf(channel, "true", "zrtp_user_unenrolled_%s", type);
		}
		break;

	case ZRTP_EVENT_IS_PENDINGCLEAR:
		{
			switch_channel_set_variable_name_printf(channel, "false", "zrtp_secure_media_confirmed_%s", type);
			switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_SEND);
			switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_RECV);
			switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
			switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
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
	switch_core_hash_init(&alloc_hash, pool);
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
	uint8_t m = 0;

	if (rtp_session->rtp_bugs & RTP_BUG_SEND_LINEAR_TIMESTAMPS) {
		rtp_session->ts += rtp_session->samples_per_interval;
		if (rtp_session->ts <= rtp_session->last_write_ts && rtp_session->ts > 0) {
			rtp_session->ts = rtp_session->last_write_ts + rtp_session->samples_per_interval;
		}
	} else if (timestamp) {
		rtp_session->ts = (uint32_t) timestamp;
		/* Send marker bit if timestamp is lower/same as before (resetted/new timer) */
		if (rtp_session->ts <= rtp_session->last_write_ts && !(rtp_session->rtp_bugs & RTP_BUG_NEVER_SEND_MARKER)) {
			m++;
		}
	} else if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
		rtp_session->ts = rtp_session->timer.samplecount;

		if (rtp_session->ts <= rtp_session->last_write_ts && rtp_session->ts > 0) {
			rtp_session->ts = rtp_session->last_write_ts + rtp_session->samples_per_interval;
		}
	} else {
		rtp_session->ts += rtp_session->samples_per_interval;
		if (rtp_session->ts <= rtp_session->last_write_ts && rtp_session->ts > 0) {
			rtp_session->ts = rtp_session->last_write_ts + rtp_session->samples_per_interval;
		}		
	}

	return m;
}



static int check_srtp_and_ice(switch_rtp_t *rtp_session)
{
	int ret = 0;


	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTO_CNG) && rtp_session->send_msg.header.ts &&
		rtp_session->timer.samplecount >= (rtp_session->last_write_samplecount + (rtp_session->samples_per_interval * 60))) {
		uint8_t data[10] = { 0 };
		switch_frame_flag_t frame_flags = SFF_NONE;
		data[0] = 65;
		rtp_session->cn++;

		get_next_write_ts(rtp_session, 0);
		rtp_session->send_msg.header.ts = htonl(rtp_session->ts);
		
		switch_rtp_write_manual(rtp_session, (void *) data, 2, 0, rtp_session->cng_pt, ntohl(rtp_session->send_msg.header.ts), &frame_flags);
		
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
			rtp_session->last_write_samplecount = rtp_session->timer.samplecount;
		}
	}

	if (rtp_session->rtcp_sock_output &&
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP) && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_PASSTHRU) &&
		rtp_session->rtcp_interval && (rtp_session->stats.read_count % rtp_session->rtcp_interval) == 0) {
		struct switch_rtcp_senderinfo *sr = (struct switch_rtcp_senderinfo*) rtp_session->rtcp_send_msg.body;
		const char* str_cname=NULL;
		
		//rtp_msg_t *send_msg = &rtp_session->send_msg;
		switch_size_t rtcp_bytes;
		switch_byte_t *ptr = (switch_byte_t *)rtp_session->rtcp_send_msg.body;
		switch_time_t when = 0;

		rtp_session->rtcp_send_msg.header.version = 2;
		rtp_session->rtcp_send_msg.header.p = 0;
		rtp_session->rtcp_send_msg.header.count = 1;

		sr->ssrc = htonl(rtp_session->ssrc);

		if (!rtp_session->stats.inbound.period_packet_count) {
			rtp_session->rtcp_send_msg.header.type = 201;
			rtcp_bytes = sizeof(switch_rtcp_hdr_t) + 4;
			ptr += 4;
		} else {
			switch_time_t when;
			rtp_session->rtcp_send_msg.header.type = 200;			
			
			if (rtp_session->send_time) {
				when = rtp_session->send_time;
			} else {
				when = switch_micro_time_now();
			}

			sr->ntp_msw = htonl((u_long)(when / 1000000 + 2208988800UL));
			/*
			sr->ntp_lsw = htonl((u_long)(when % 1000000 * ((UINT_MAX * 1.0)/ 1000000.0)));
			*/
			sr->ntp_lsw = htonl((u_long)(rtp_session->send_time % 1000000 * 4294.967296));
			sr->ts = htonl(rtp_session->last_write_ts);
			sr->pc = htonl(rtp_session->stats.outbound.packet_count);
			sr->oc = htonl((rtp_session->stats.outbound.raw_bytes - rtp_session->stats.outbound.packet_count * sizeof(srtp_hdr_t)));

		}

		/* TBD need to put more accurate stats here. */

		sr->sr_source.ssrc1 = htonl(rtp_session->stats.rtcp.peer_ssrc);
		sr->sr_source.fraction_lost = 0;
		sr->sr_source.cumulative_lost = htonl(rtp_session->stats.inbound.skip_packet_count);
		sr->sr_source.hi_seq_recieved = htonl(rtp_session->recv_msg.header.seq);
		sr->sr_source.interarrival_jitter = htonl(0);
		sr->sr_source.lsr = htonl(0);
		sr->sr_source.lsr_delay = htonl(0);

		sr->sr_desc_head.v = 0x02;
		sr->sr_desc_head.padding = 0;
		sr->sr_desc_head.sc = 1;
		sr->sr_desc_head.pt = 202;
		sr->sr_desc_head.length = htons(5);

		sr->sr_desc_ssrc.ssrc = htonl(rtp_session->ssrc);
		sr->sr_desc_ssrc.cname = 0x1; 
		{
			char bufa[30];
			str_cname = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtcp_local_addr);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Setting RTCP src-1 to %s\n", str_cname);
			sr->sr_desc_ssrc.length = strlen(str_cname);
			memcpy ((char*)sr->sr_desc_ssrc.text, str_cname, strlen(str_cname));
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Setting RTCP src-1 LENGTH  to %d (%d, %s)\n", sr->sr_desc_ssrc.length, sr->sr_desc_head.length, str_cname);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Setting msw = %d, lsw = %d \n", sr->ntp_msw, sr->ntp_lsw);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "now = %"SWITCH_TIME_T_FMT", now lo = %d, now hi = %d\n", when, (int32_t)(when&0xFFFFFFFF), (int32_t)((when>>32&0xFFFFFFFF)));

		rtcp_bytes = sizeof(switch_rtcp_hdr_t) + sizeof(struct switch_rtcp_senderinfo) + sr->sr_desc_ssrc.length -1 ;
		rtp_session->rtcp_send_msg.header.length = htons((u_short)(rtcp_bytes / 4) - 1); 
		

#ifdef ENABLE_SRTP
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
			int sbytes = (int) rtcp_bytes;
			int stat = srtp_protect_rtcp(rtp_session->send_ctx, &rtp_session->rtcp_send_msg.header, &sbytes);
			if (stat) {
				switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error: SRTP RTCP protection failed with code %d\n", stat);
			}
			rtcp_bytes = sbytes;
		}
#endif

#ifdef ENABLE_ZRTP
		/* ZRTP Send */
		if (zrtp_on && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
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
		if (switch_socket_sendto(rtp_session->rtcp_sock_output, rtp_session->rtcp_remote_addr, 0, 
								 (void *)&rtp_session->rtcp_send_msg, &rtcp_bytes ) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,"RTCP packet not written\n");
		} else {
			rtp_session->stats.inbound.period_packet_count = 0;
		}

		if (rtp_session->rtcp_ice.ice_user) {
			ice_out(rtp_session, &rtp_session->rtcp_ice);
		}
	}
	

	if (rtp_session->remote_stun_addr) {
		do_stun_ping(rtp_session);
	}

	if (rtp_session->ice.ice_user) {
		if (ice_out(rtp_session, &rtp_session->ice) != SWITCH_STATUS_SUCCESS) {
			ret = -1;
			goto end;
		}
	}

	if (rtp_session->rtcp_ice.ice_user) {
		if (ice_out(rtp_session, &rtp_session->rtcp_ice) != SWITCH_STATUS_SUCCESS) {
			ret = -1;
			goto end;
		}
	}

 end:

	return ret;
}

SWITCH_DECLARE(void) switch_rtp_ping(switch_rtp_t *rtp_session)
{
	check_srtp_and_ice(rtp_session);
}

SWITCH_DECLARE(void) switch_rtp_get_random(void *buf, uint32_t len)
{
#ifdef ENABLE_SRTP
	crypto_get_random(buf, len);
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

	for (hi = switch_hash_first(NULL, alloc_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
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
	crypto_kernel_shutdown();
#endif

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

SWITCH_DECLARE(void) switch_rtp_intentional_bugs(switch_rtp_t *rtp_session, switch_rtp_bug_flag_t bugs)
{
	rtp_session->rtp_bugs = bugs;

	if ((rtp_session->rtp_bugs & RTP_BUG_START_SEQ_AT_ZERO)) {
		rtp_session->seq = 0;
	}

}


static switch_status_t enable_remote_rtcp_socket(switch_rtp_t *rtp_session, const char **err) {
	
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {

		if (switch_sockaddr_info_get(&rtp_session->rtcp_remote_addr, rtp_session->eff_remote_host_str, SWITCH_UNSPEC, 
									 rtp_session->remote_rtcp_port, 0, rtp_session->pool) != SWITCH_STATUS_SUCCESS || !rtp_session->rtcp_remote_addr) {
			*err = "RTCP Remote Address Error!";
			return SWITCH_STATUS_FALSE;
		} else {
			const char *host;
			char bufa[30];
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			host = switch_get_addr(bufa, sizeof(bufa), rtp_session->rtcp_remote_addr);

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setting RTCP remote addr to %s:%d\n", host, rtp_session->remote_rtcp_port);
		}

		if (!(rtp_session->rtcp_sock_input && rtp_session->rtcp_sock_output)) {
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
	char bufa[30];

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
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
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error allocating rtcp [%s]\n", *err);
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
	
	if (switch_socket_bind(new_sock, rtp_session->local_addr) != SWITCH_STATUS_SUCCESS) {
		char *em = switch_core_sprintf(rtp_session->pool, "Bind Error! %s:%d", host, port);
		*err = em;
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

	switch_socket_create_pollset(&rtp_session->read_pollfd, rtp_session->sock_input, SWITCH_POLLIN | SWITCH_POLLERR, rtp_session->pool);

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
		if ((status = enable_local_rtcp_socket(rtp_session, err)) == SWITCH_STATUS_SUCCESS) {
			*err = "Success";
		}
	} else {
		status = SWITCH_STATUS_SUCCESS;
		*err = "Success";
	}
	
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
	if (rtp_session->missed_count >= max) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
						  "new max missed packets(%d->%d) greater than current missed packets(%d). RTP will timeout.\n",
						  rtp_session->missed_count, max, rtp_session->missed_count);
	}

	rtp_session->max_missed_packets = max;
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

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP) && rtp_session->rtcp_sock_input) {
		switch_socket_sendto(rtp_session->rtcp_sock_input, rtp_session->rtcp_local_addr, 0, (void *) &o, &len);
	}
}

SWITCH_DECLARE(switch_status_t) switch_rtp_udptl_mode(switch_rtp_t *rtp_session) 
{
	switch_socket_t *sock;

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		ping_socket(rtp_session);
	}

	READ_INC(rtp_session);
	WRITE_INC(rtp_session);

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) || rtp_session->timer.timer_interface) {
		switch_core_timer_destroy(&rtp_session->timer);
		memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
		switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
	}

	switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP);

	if (rtp_session->rtcp_sock_input) {
		ping_socket(rtp_session);
		switch_socket_shutdown(rtp_session->rtcp_sock_input, SWITCH_SHUTDOWN_READWRITE);
	}

	if (rtp_session->rtcp_sock_output && rtp_session->rtcp_sock_output != rtp_session->rtcp_sock_input) {
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

	switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_UDPTL);
	switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA);
	switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, FALSE);
	switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
		
	WRITE_DEC(rtp_session);
	READ_DEC(rtp_session);

	switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_STICKY_FLUSH);
	switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_FLUSH);

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

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {	
		if (remote_rtcp_port) {
			rtp_session->remote_rtcp_port = remote_rtcp_port;
		} else {
			rtp_session->remote_rtcp_port = rtp_session->eff_remote_port + 1;
		}
		status = enable_remote_rtcp_socket(rtp_session, err);
	}

	switch_mutex_unlock(rtp_session->write_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_add_crypto_key(switch_rtp_t *rtp_session,
														  switch_rtp_crypto_direction_t direction,
														  uint32_t index, switch_rtp_crypto_key_type_t type, unsigned char *key, switch_size_t keylen)
{
#ifndef ENABLE_SRTP
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "SRTP NOT SUPPORTED IN THIS BUILD!\n");
	return SWITCH_STATUS_FALSE;
#else
	switch_rtp_crypto_key_t *crypto_key;
	srtp_policy_t *policy;
	err_status_t stat;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_event_t *fsevent = NULL;

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

	switch_channel_set_variable(channel, "send_silence_when_idle", "400");

	switch (crypto_key->type) {
	case AES_CM_128_HMAC_SHA1_80:
		crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy->rtp);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "sip_has_crypto", "AES_CM_128_HMAC_SHA1_80");
		}
		break;
	case AES_CM_128_HMAC_SHA1_32:
		crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy->rtp);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "sip_has_crypto", "AES_CM_128_HMAC_SHA1_32");
		}
		break;
	case AES_CM_128_NULL_AUTH:
		crypto_policy_set_aes_cm_128_null_auth(&policy->rtp);
		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
			switch_channel_set_variable(channel, "sip_has_crypto", "AES_CM_128_NULL_AUTH");
		}
		break;
	default:
		break;
	}

	policy->next = NULL;
	policy->key = (uint8_t *) crypto_key->key;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
		crypto_policy_set_rtcp_default(&policy->rtcp);
		policy->rtcp.sec_serv = sec_serv_none;
	}

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
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating Secure RTP RECV\n");
				switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error allocating srtp [%d]\n", stat);
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
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Activating Secure RTP SEND\n");
				switch_set_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error allocating SRTP [%d]\n", stat);
				return status;
			}
		}

		break;
	default:
		abort();
		break;
	}

	if (switch_event_create(&fsevent, SWITCH_EVENT_CALL_SECURE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(fsevent, SWITCH_STACK_BOTTOM, "secure_type", "srtp:%s", switch_channel_get_variable(channel, "sip_has_crypto"));
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

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_change_interval(switch_rtp_t *rtp_session, uint32_t ms_per_packet, uint32_t samples_per_interval)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int change_timer = 0;

	if (rtp_session->ms_per_packet && rtp_session->ms_per_packet != ms_per_packet) {
		change_timer = 1;
	}

	switch_rtp_set_interval(rtp_session, ms_per_packet, samples_per_interval);

	if (change_timer && rtp_session->timer_name) {
		READ_INC(rtp_session);
		WRITE_INC(rtp_session);

		if (rtp_session->timer.timer_interface) {
			switch_core_timer_destroy(&rtp_session->timer);
		}
		if ((status = switch_core_timer_init(&rtp_session->timer,
											 rtp_session->timer_name, ms_per_packet / 1000,
											 samples_per_interval, rtp_session->pool)) == SWITCH_STATUS_SUCCESS) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
							  "RE-Starting timer [%s] %d bytes per %dms\n", rtp_session->timer_name, samples_per_interval, ms_per_packet / 1000);
		} else {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
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

SWITCH_DECLARE(switch_status_t) switch_rtp_create(switch_rtp_t **new_rtp_session,
												  switch_payload_t payload,
												  uint32_t samples_per_interval,
												  uint32_t ms_per_packet,
												  switch_rtp_flag_t flags, char *timer_name, const char **err, switch_memory_pool_t *pool)
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
	rtp_session->te = 101;
	rtp_session->recv_te = 101;

	switch_mutex_init(&rtp_session->flag_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->read_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->write_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&rtp_session->dtmf_data.dtmf_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_queue_create(&rtp_session->dtmf_data.dtmf_queue, 100, rtp_session->pool);
	switch_queue_create(&rtp_session->dtmf_data.dtmf_inqueue, 100, rtp_session->pool);

	switch_rtp_set_flag(rtp_session, flags);

	/* for from address on recvfrom calls */
	switch_sockaddr_create(&rtp_session->from_addr, pool);

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
		switch_sockaddr_create(&rtp_session->rtcp_from_addr, pool);
	}
	rtp_session->seq = (uint16_t) rand();
	rtp_session->ssrc = (uint32_t) ((intptr_t) rtp_session + (uint32_t) switch_epoch_time_now(NULL));

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
	rtp_session->rpayload = payload;




	switch_rtp_set_interval(rtp_session, ms_per_packet, samples_per_interval);
	rtp_session->conf_samples_per_interval = samples_per_interval;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) && zstr(timer_name)) {
		timer_name = "soft";
	}

	if (!zstr(timer_name) && !strcasecmp(timer_name, "none")) {
		timer_name = NULL;
	}

	if (!zstr(timer_name)) {
		rtp_session->timer_name = switch_core_strdup(pool, timer_name);
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);

		if (switch_core_timer_init(&rtp_session->timer, timer_name, ms_per_packet / 1000, samples_per_interval, pool) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
							  "Starting timer [%s] %d bytes per %dms\n", timer_name, samples_per_interval, ms_per_packet / 1000);
		} else {
			memset(&rtp_session->timer, 0, sizeof(rtp_session->timer));
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error starting timer [%s], async RTP disabled\n", timer_name);
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Not using a timer\n");
		switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_USE_TIMER);
		switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
	}

	if (channel) {
		switch_channel_set_private(channel, "__rtcp_audio_rtp_session", rtp_session);
	}

#ifdef ENABLE_ZRTP
	if (zrtp_on && session && channel && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		switch_rtp_t *master_rtp_session = NULL;

		int initiator = 0;
		const char *zrtp_enabled = switch_channel_get_variable(channel, "zrtp_secure_media");
		const char *srtp_enabled = switch_channel_get_variable(channel, "sip_secure_media");

		if (switch_true(srtp_enabled) && switch_true(zrtp_enabled)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
							  "You can not have ZRTP and SRTP enabled simultaneously, ZRTP will be disabled for this call!\n");
			switch_channel_set_variable(channel, "zrtp_secure_media", NULL);
			zrtp_enabled = NULL;
		}


		if (switch_true(zrtp_enabled)) {
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
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
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! zRTP INIT Failed\n");
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
		switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_FLUSH);
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

SWITCH_DECLARE(void) switch_rtp_set_recv_pt(switch_rtp_t *rtp_session, switch_payload_t pt)
{
	rtp_session->rpayload = pt;
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

		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error resolving stun ping addr\n");
		return SWITCH_STATUS_FALSE;
	}

	if (funny) {
		rtp_session->ice.funny_stun++;
	}

	rtp_session->stun_port = stun_port;

	rtp_session->ice.default_stuncount = packet_count;

	rtp_session->stun_ip = switch_core_strdup(rtp_session->pool, stun_ip);
	return SWITCH_STATUS_SUCCESS;
}

static void jb_callback(stfu_instance_t *i, void *udata)
{
	switch_core_session_t *session = (switch_core_session_t *) udata;
	stfu_report_t r = { 0 };

	stfu_n_report(i, &r);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG8, 
					  "%s JB REPORT:\nlen: %u\nin: %u\nclean: %u\ngood: %u\nbad: %u\n",
					  switch_core_session_get_name(session),
					  r.qlen,
					  r.packet_in_count,
					  r.clean_count,
					  r.consecutive_good_count,
					  r.consecutive_bad_count
					  );

}

SWITCH_DECLARE(stfu_instance_t *) switch_rtp_get_jitter_buffer(switch_rtp_t *rtp_session)
{
	if (!switch_rtp_ready(rtp_session) || !rtp_session->jb) {
		return NULL;
	}

	return rtp_session->jb;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_pause_jitter_buffer(switch_rtp_t *rtp_session, switch_bool_t pause)
{
	
	if (!switch_rtp_ready(rtp_session) || !rtp_session->jb) {
		return SWITCH_STATUS_FALSE;
	}

	if (!!pause == !!rtp_session->pause_jb) {
		return SWITCH_STATUS_FALSE;
	}

	if (rtp_session->pause_jb && !pause) {
		stfu_n_reset(rtp_session->jb);
	}

	rtp_session->pause_jb = pause ? 1 : 0;
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_deactivate_jitter_buffer(switch_rtp_t *rtp_session)
{
	
	if (!switch_rtp_ready(rtp_session) || !rtp_session->jb) {
		return SWITCH_STATUS_FALSE;
	}

	READ_INC(rtp_session);
	stfu_n_destroy(&rtp_session->jb);
	READ_DEC(rtp_session);
	
	return SWITCH_STATUS_SUCCESS;
}

static void jb_logger(const char *file, const char *func, int line, int level, const char *fmt, ...)
{
	int ret;
	char *data;
	va_list ap;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	if (ret != -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "%s", data);
		free(data);
	}

	//switch_log_printf(SWITCH_CHANNEL_ID_LOG_CLEAN, file, func, line, NULL, level, fmt, ap);
	va_end(ap);
}

SWITCH_DECLARE(switch_status_t) switch_rtp_debug_jitter_buffer(switch_rtp_t *rtp_session, const char *name)
{

	if (!switch_rtp_ready(rtp_session) || !rtp_session->jb) {
		return SWITCH_STATUS_FALSE;
	}
	
	stfu_n_debug(rtp_session->jb, name);
	stfu_global_set_logger(jb_logger);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_jitter_buffer(switch_rtp_t *rtp_session, 
																  uint32_t queue_frames, 
																  uint32_t max_queue_frames, 
																  uint32_t samples_per_packet, 
																  uint32_t samples_per_second,
																  uint32_t max_drift)
{

	if (!switch_rtp_ready(rtp_session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (queue_frames < 1) {
		queue_frames = 3;
	}

	if (max_queue_frames < queue_frames) {
		max_queue_frames = queue_frames * 3;
	}

	READ_INC(rtp_session);
	if (rtp_session->jb) {
		stfu_n_resize(rtp_session->jb, queue_frames);
	} else {
		rtp_session->jb = stfu_n_init(queue_frames, max_queue_frames ? max_queue_frames : 50, samples_per_packet, samples_per_second, max_drift);
	}
	READ_DEC(rtp_session);
	
	if (rtp_session->jb) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		stfu_n_call_me(rtp_session->jb, jb_callback, session);

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_rtcp(switch_rtp_t *rtp_session, int send_rate, switch_port_t remote_port)
{
	const char *err = NULL;

	if (!rtp_session->ms_per_packet) {
		return SWITCH_STATUS_FALSE;
	}
	
	switch_set_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP);

	if (!(rtp_session->remote_rtcp_port = remote_port)) {
		rtp_session->remote_rtcp_port = rtp_session->remote_port + 1;
	}
	
	if (send_rate == -1) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		switch_set_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_PASSTHRU);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RTCP passthru enabled. Remote Port: %d\n", rtp_session->remote_rtcp_port);
	} else {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RTCP send rate is: %d and packet rate is: %d Remote Port: %d\n", 
						  send_rate, rtp_session->ms_per_packet, rtp_session->remote_rtcp_port);
		rtp_session->rtcp_interval = send_rate/(rtp_session->ms_per_packet/1000);
	}

	return enable_local_rtcp_socket(rtp_session, &err) || enable_remote_rtcp_socket(rtp_session, &err);

}

SWITCH_DECLARE(switch_status_t) switch_rtp_activate_ice(switch_rtp_t *rtp_session, char *login, char *rlogin, const char *password)
{
	char ice_user[80];
	char user_ice[80];

	switch_snprintf(ice_user, sizeof(ice_user), "%s%s", login, rlogin);
	switch_snprintf(user_ice, sizeof(user_ice), "%s%s", rlogin, login);

	rtp_session->ice.ice_user = switch_core_strdup(rtp_session->pool, ice_user);
	rtp_session->ice.user_ice = switch_core_strdup(rtp_session->pool, user_ice);
	if (password) {
		rtp_session->ice.pass = switch_core_strdup(rtp_session->pool, password);
	}
	
	rtp_session->ice.default_stuncount = 25;
	rtp_session->ice.stuncount = 0;

	if (rtp_session->ice.ice_user) {
		if (ice_out(rtp_session, &rtp_session->ice) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_rtp_activate_rtcp_ice(switch_rtp_t *rtp_session, char *login, char *rlogin, const char *password)
{
	char ice_user[80];
	char user_ice[80];

	switch_snprintf(ice_user, sizeof(ice_user), "%s%s", login, rlogin);
	switch_snprintf(user_ice, sizeof(user_ice), "%s%s", rlogin, login);
	rtp_session->rtcp_ice.ice_user = switch_core_strdup(rtp_session->pool, ice_user);
	rtp_session->rtcp_ice.user_ice = switch_core_strdup(rtp_session->pool, user_ice);
	if (password) {
		rtp_session->rtcp_ice.pass = switch_core_strdup(rtp_session->pool, password);
	}
	rtp_session->rtcp_ice.default_stuncount = 25;
	rtp_session->rtcp_ice.stuncount = 0;
	
	if (rtp_session->rtcp_ice.ice_user) {
		if (ice_out(rtp_session, &rtp_session->rtcp_ice) != SWITCH_STATUS_SUCCESS) {
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(void) switch_rtp_flush(switch_rtp_t *rtp_session)
{
	if (!switch_rtp_ready(rtp_session)) {
		return;
	}

	switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_FLUSH);
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
		ping_socket(rtp_session);
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
			ping_socket(rtp_session);
			switch_socket_shutdown(rtp_session->sock_input, SWITCH_SHUTDOWN_READWRITE);
		}
		if (rtp_session->sock_output && rtp_session->sock_output != rtp_session->sock_input) {
			switch_socket_shutdown(rtp_session->sock_output, SWITCH_SHUTDOWN_READWRITE);
		}
		
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
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

	if (!rtp_session || !rtp_session->flag_mutex || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SHUTDOWN)) {
		return 0;
	}

	switch_mutex_lock(rtp_session->flag_mutex);
	ret = (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_IO) && rtp_session->sock_input && rtp_session->sock_output && rtp_session->remote_addr
		   && rtp_session->ready == 2) ? 1 : 0;
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

	if (switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_VAD)) {
		switch_rtp_disable_vad(*rtp_session);
	}

#ifdef ENABLE_SRTP
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
#endif

#ifdef ENABLE_ZRTP
	/* ZRTP */
	if (zrtp_on && !switch_test_flag((*rtp_session), SWITCH_RTP_FLAG_PROXY_MEDIA)) {

		if ((*rtp_session)->zrtp_stream != NULL) {
			zrtp_stream_stop((*rtp_session)->zrtp_stream);
		}

		if (switch_test_flag((*rtp_session), SWITCH_ZRTP_FLAG_SECURE_SEND)) {
			switch_clear_flag((*rtp_session), SWITCH_ZRTP_FLAG_SECURE_SEND);
		}

		if (switch_test_flag((*rtp_session), SWITCH_ZRTP_FLAG_SECURE_RECV)) {
			switch_clear_flag((*rtp_session), SWITCH_ZRTP_FLAG_SECURE_RECV);
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
		rtp_flush_read_buffer(rtp_session, SWITCH_RTP_FLUSH_ONCE);
	} else if (flags & SWITCH_RTP_FLAG_NOBLOCK) {
		switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, TRUE);
	}

}

SWITCH_DECLARE(uint32_t) switch_rtp_test_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags)
{
	return (uint32_t) switch_test_flag(rtp_session, flags);
}

SWITCH_DECLARE(void) switch_rtp_clear_flag(switch_rtp_t *rtp_session, switch_rtp_flag_t flags)
{
	switch_clear_flag_locked(rtp_session, flags);

	if (flags & SWITCH_RTP_FLAG_NOBLOCK) {
		switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, FALSE);
	}
}

static void set_dtmf_delay(switch_rtp_t *rtp_session, uint32_t ms, uint32_t max_ms)
{
	int upsamp, max_upsamp;
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");	

	if (!max_ms) max_ms = ms;

	upsamp = ms * (rtp_session->samples_per_second / 1000);
	max_upsamp = max_ms * (rtp_session->samples_per_second / 1000);

	rtp_session->queue_delay = upsamp;

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
		rtp_session->max_next_write_samplecount = rtp_session->timer.samplecount + max_upsamp;
		rtp_session->next_write_samplecount = rtp_session->timer.samplecount + upsamp;
		rtp_session->last_write_ts += upsamp;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Queue digit delay of %dms\n", ms);	
}

static void do_2833(switch_rtp_t *rtp_session, switch_core_session_t *session)
{
	switch_frame_flag_t flags = 0;
	uint32_t samples = rtp_session->samples_per_interval;

	if (!rtp_session->last_write_ts) {
		return;
	}
	
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

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Send %s packet for [%c] ts=%u dur=%d/%d/%d seq=%d lw=%u\n",
							  loops == 1 ? "middle" : "end", rtp_session->dtmf_data.out_digit,
							  rtp_session->dtmf_data.timestamp_dtmf,
							  rtp_session->dtmf_data.out_digit_sofar,
							  rtp_session->dtmf_data.out_digit_sub_sofar, rtp_session->dtmf_data.out_digit_dur, rtp_session->seq, rtp_session->last_write_ts);
		}

		if (loops != 1) {
			rtp_session->sending_dtmf = 0;
			rtp_session->need_mark = 1;
			
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
				rtp_session->last_write_samplecount = rtp_session->timer.samplecount;
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

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
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
			rtp_session->dtmf_data.out_digit_packet[1] = 7;
			rtp_session->dtmf_data.out_digit_packet[2] = (unsigned char) (rtp_session->dtmf_data.out_digit_sub_sofar >> 8);
			rtp_session->dtmf_data.out_digit_packet[3] = (unsigned char) rtp_session->dtmf_data.out_digit_sub_sofar;


			rtp_session->dtmf_data.timestamp_dtmf = rtp_session->last_write_ts + samples;
			rtp_session->last_write_ts = rtp_session->dtmf_data.timestamp_dtmf;
			
			wrote = switch_rtp_write_manual(rtp_session,
											rtp_session->dtmf_data.out_digit_packet,
											4,
											rtp_session->rtp_bugs & RTP_BUG_CISCO_SKIP_MARK_BIT_2833 ? 0 : 1,
											rtp_session->te, rtp_session->dtmf_data.timestamp_dtmf, &flags);

			
			rtp_session->stats.outbound.raw_bytes += wrote;
			rtp_session->stats.outbound.dtmf_packet_count++;
			
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Send start packet for [%c] ts=%u dur=%d/%d/%d seq=%d lw=%d\n",
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

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) ||
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) ||
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
		return;
	}


	if (switch_rtp_ready(rtp_session)) {
		rtp_session->last_write_ts = RTP_TS_RESET;
		switch_set_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH);

		switch (flush) {
		case SWITCH_RTP_FLUSH_STICK:
			switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_STICKY_FLUSH);
			break;
		case SWITCH_RTP_FLUSH_UNSTICK:
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_STICKY_FLUSH);
			break;
		default:
				break;
		}
	}
}

static void do_flush(switch_rtp_t *rtp_session)
{
	int was_blocking = 0;
	switch_size_t bytes;
	uint32_t flushed = 0;

	if (!switch_rtp_ready(rtp_session) || 
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) || 
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) 
		) {
		return;
	}
	

	READ_INC(rtp_session);

	if (switch_rtp_ready(rtp_session)) {
		
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_DEBUG_RTP_READ)) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
			if (!session) {
				switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_DEBUG_RTP_READ);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "RTP HAS NO SESSION!\n");
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
								  SWITCH_LOG_CONSOLE, "%s FLUSH\n", switch_channel_get_name(switch_core_session_get_channel(session))
								  );
			}
		}

		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK)) {
			was_blocking = 1;
			switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
			switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, TRUE);
		}

		do {
			if (switch_rtp_ready(rtp_session)) {
				bytes = sizeof(rtp_msg_t);
				switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock_input, 0, (void *) &rtp_session->recv_msg, &bytes);
				
				if (bytes) {
					int do_cng = 0;

					/* Make sure to handle RFC2833 packets, even if we're flushing the packets */
					if (bytes > rtp_header_len && rtp_session->recv_te && rtp_session->recv_msg.header.pt == rtp_session->recv_te) {
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

		if (rtp_session->jb && flushed) {
			stfu_n_sync(rtp_session->jb, flushed);
		}

		if (was_blocking && switch_rtp_ready(rtp_session)) {
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_NOBLOCK);
			switch_socket_opt_set(rtp_session->sock_input, SWITCH_SO_NONBLOCK, FALSE);
		}
	}

	READ_DEC(rtp_session);
}

#define return_cng_frame() do_cng = 1; goto timer_check

static switch_status_t read_rtp_packet(switch_rtp_t *rtp_session, switch_size_t *bytes, switch_frame_flag_t *flags, switch_bool_t return_jb_packet)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	stfu_frame_t *jb_frame;
	uint32_t ts;

	switch_assert(bytes);
 more:
	*bytes = sizeof(rtp_msg_t);

	status = switch_socket_recvfrom(rtp_session->from_addr, rtp_session->sock_input, 0, (void *) &rtp_session->recv_msg, bytes);
	ts = ntohl(rtp_session->recv_msg.header.ts);
	rtp_session->recv_msg.ebody = NULL;

	if (*bytes) {
		uint16_t seq = ntohs((uint16_t) rtp_session->recv_msg.header.seq);

		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL) &&
			rtp_session->recv_msg.header.version == 2 && rtp_session->recv_msg.header.x) { /* header extensions */

			rtp_session->recv_msg.ext = (switch_rtp_hdr_ext_t *) rtp_session->recv_msg.body;

			rtp_session->recv_msg.ext->length = ntohs((uint16_t)rtp_session->recv_msg.ext->length);
			rtp_session->recv_msg.ext->profile = ntohs((uint16_t)rtp_session->recv_msg.ext->profile);

			if (rtp_session->recv_msg.ext->length < SWITCH_RTP_MAX_BUF_LEN_WORDS) {
				rtp_session->recv_msg.ebody = rtp_session->recv_msg.body + (rtp_session->recv_msg.ext->length * 4) + 4;
			}
		}
		

#ifdef DEBUG_MISSED_SEQ		
		if (rtp_session->last_seq && rtp_session->last_seq+1 != seq) {
			//2012-11-28 18:33:11.799070 [ERR] switch_rtp.c:2883 Missed -65536 RTP frames from sequence [65536] to [-1] (missed). Time since last read [20021]
			switch_size_t flushed_packets_diff = rtp_session->stats.inbound.flush_packet_count - rtp_session->last_flush_packet_count;
			switch_size_t num_missed = (switch_size_t)seq - (rtp_session->last_seq+1);

			if (num_missed == 1) { /* We missed one packet */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missed one RTP frame with sequence [%d]%s. Time since last read [%ld]\n",
								  rtp_session->last_seq+1, (flushed_packets_diff == 1) ? " (flushed by FS)" : " (missed)",
								  rtp_session->last_read_time ? switch_micro_time_now()-rtp_session->last_read_time : 0);
			} else { /* We missed multiple packets */
				if (flushed_packets_diff == 0) { 
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (missed). Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  rtp_session->last_read_time ? switch_micro_time_now()-rtp_session->last_read_time : 0);
				} else if (flushed_packets_diff == num_missed) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (flushed by FS). Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  rtp_session->last_read_time ? switch_micro_time_now()-rtp_session->last_read_time : 0);
				} else if (num_missed > flushed_packets_diff) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (%ld packets flushed by FS, %ld packets missed)."
									  " Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  flushed_packets_diff, num_missed-flushed_packets_diff,
									  rtp_session->last_read_time ? switch_micro_time_now()-rtp_session->last_read_time : 0);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
									  "Missed %ld RTP frames from sequence [%d] to [%d] (%ld packets flushed by FS). Time since last read [%ld]\n",
									  num_missed, rtp_session->last_seq+1, seq-1,
									  flushed_packets_diff, rtp_session->last_read_time ? switch_micro_time_now()-rtp_session->last_read_time : 0);
				}
			}

		}
#endif
		rtp_session->last_seq = seq;
	}

	rtp_session->last_flush_packet_count = rtp_session->stats.inbound.flush_packet_count;
	rtp_session->last_read_time = switch_micro_time_now();

	if (*bytes && (!rtp_session->recv_te || rtp_session->recv_msg.header.pt != rtp_session->recv_te) && 
		ts && !rtp_session->jb && !rtp_session->pause_jb && ts == rtp_session->last_cng_ts) {
		/* we already sent this frame..... */
		*bytes = 0;
		return SWITCH_STATUS_SUCCESS;
	}

	if (*bytes) {
		rtp_session->stats.inbound.raw_bytes += *bytes;
		if (rtp_session->recv_te && rtp_session->recv_msg.header.pt == rtp_session->recv_te) {
			rtp_session->stats.inbound.dtmf_packet_count++;
		} else if (rtp_session->cng_pt && (rtp_session->recv_msg.header.pt == rtp_session->cng_pt || rtp_session->recv_msg.header.pt == 13)) {
			rtp_session->stats.inbound.cng_packet_count++;
		} else {
			rtp_session->stats.inbound.media_packet_count++;
			rtp_session->stats.inbound.media_bytes += *bytes;
		}

		rtp_session->stats.inbound.packet_count++;


		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
#ifdef ENABLE_ZRTP
			/* ZRTP Recv */
			if(zrtp_on) {
				
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
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV) && (!rtp_session->ice.ice_user || rtp_session->recv_msg.header.version == 2)) {
				int sbytes = (int) *bytes;
				err_status_t stat = 0;

				if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV_RESET)) {
					switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV_RESET);
					srtp_dealloc(rtp_session->recv_ctx);
					rtp_session->recv_ctx = NULL;
					if ((stat = srtp_create(&rtp_session->recv_ctx, &rtp_session->recv_policy))) {
						switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP RECV\n");
						return SWITCH_STATUS_FALSE;
					} else {
						switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "RE-Activating Secure RTP RECV\n");
						rtp_session->srtp_errs = 0;
					}
				}

				if (!(*flags & SFF_PLC)) {
					stat = srtp_unprotect(rtp_session->recv_ctx, &rtp_session->recv_msg.header, &sbytes);
				}

				if (stat && rtp_session->recv_msg.header.pt != rtp_session->recv_te && rtp_session->recv_msg.header.pt != rtp_session->cng_pt) {
					if (++rtp_session->srtp_errs >= MAX_SRTP_ERRS) {
						switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
										  "Error: SRTP unprotect failed with code %d%s\n", stat,
										  stat == err_status_replay_fail ? " (replay check failed)" : stat ==
										  err_status_auth_fail ? " (auth check failed)" : "");
						return SWITCH_STATUS_FALSE;
					} else {
						sbytes = 0;
					}
				} else {
					rtp_session->srtp_errs = 0;
				}

				*bytes = sbytes;
			}
#endif
		}
	}


	if ((rtp_session->recv_te && rtp_session->recv_msg.header.pt == rtp_session->recv_te) || 
		(*bytes < rtp_header_len && *bytes > 0) ||
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
		return SWITCH_STATUS_SUCCESS;
	}


	rtp_session->last_read_ts = ts;
	
	
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_BYTESWAP) && rtp_session->recv_msg.header.pt == rtp_session->rpayload) {
		switch_swap_linear((int16_t *)RTP_BODY(rtp_session), (int) *bytes - rtp_header_len);
	}

	if (rtp_session->jb && !rtp_session->pause_jb && rtp_session->recv_msg.header.version == 2 && *bytes) {
		if (rtp_session->recv_msg.header.m && rtp_session->recv_msg.header.pt != rtp_session->recv_te && 
			!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) && !(rtp_session->rtp_bugs & RTP_BUG_IGNORE_MARK_BIT)) {
			stfu_n_reset(rtp_session->jb);
		}

		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) && rtp_session->timer.interval) {
			switch_core_timer_sync(&rtp_session->timer);
		}

		if (stfu_n_eat(rtp_session->jb, rtp_session->last_read_ts, 
					   ntohs((uint16_t) rtp_session->recv_msg.header.seq),
					   rtp_session->recv_msg.header.pt,
					   RTP_BODY(rtp_session), *bytes - rtp_header_len, rtp_session->timer.samplecount) == STFU_ITS_TOO_LATE) {
			
			goto more;
		}

		status = SWITCH_STATUS_FALSE;
		if (!return_jb_packet) {
			return status;
		}
		*bytes = 0;
	}

	if (rtp_session->jb && !rtp_session->pause_jb) {
		if ((jb_frame = stfu_n_read_a_frame(rtp_session->jb))) {
			memcpy(RTP_BODY(rtp_session), jb_frame->data, jb_frame->dlen);

			if (jb_frame->plc) {
				(*flags) |= SFF_PLC;
			} else {
				rtp_session->stats.inbound.jb_packet_count++;
			}
			*bytes = jb_frame->dlen + rtp_header_len;
			rtp_session->recv_msg.header.ts = htonl(jb_frame->ts);
			rtp_session->recv_msg.header.pt = jb_frame->pt;
			rtp_session->recv_msg.header.seq = htons(jb_frame->seq);
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

static switch_status_t read_rtcp_packet(switch_rtp_t *rtp_session, switch_size_t *bytes, switch_frame_flag_t *flags)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_assert(bytes);

	*bytes = sizeof(rtcp_msg_t);
	if ((status = switch_socket_recvfrom(rtp_session->rtcp_from_addr, rtp_session->rtcp_sock_input, 0, (void *) &rtp_session->rtcp_recv_msg, bytes)) 
		!= SWITCH_STATUS_SUCCESS) {
		*bytes = 0;
	}

#ifdef ENABLE_SRTP
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_RECV) && (!rtp_session->ice.ice_user || rtp_session->rtcp_recv_msg.header.version == 2)) {
		int sbytes = (int) *bytes;
		err_status_t stat = 0;

		stat = srtp_unprotect_rtcp(rtp_session->recv_ctx, &rtp_session->rtcp_recv_msg.header, &sbytes);
		
		if (stat) {
			if (++rtp_session->srtp_errs >= MAX_SRTP_ERRS) {
				switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR,
								  "Error: SRTP RTCP unprotect failed with code %d%s\n", stat,
								  stat == err_status_replay_fail ? " (replay check failed)" : stat ==
								  err_status_auth_fail ? " (auth check failed)" : "");
				return SWITCH_STATUS_FALSE;
			} else {
				sbytes = 0;
			}
		} else {
			rtp_session->srtp_errs = 0;
		}
		
		*bytes = sbytes;
		
	}
#endif


#ifdef ENABLE_ZRTP
	if (zrtp_on && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		/* ZRTP Recv */
		if (bytes) {
			unsigned int sbytes = (int) *bytes;
			zrtp_status_t stat = 0;
			
			stat = zrtp_process_srtcp(rtp_session->zrtp_stream, (void *) &rtp_session->rtcp_recv_msg, &sbytes);
			
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


	if (*bytes) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10,"Received an RTCP packet of length %" SWITCH_SIZE_T_FMT " bytes\n", *bytes);
		if (rtp_session->rtcp_recv_msg.header.version == 2) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10,"RTCP packet type is %d\n", rtp_session->rtcp_recv_msg.header.type);
			if (rtp_session->rtcp_recv_msg.header.type == 200) {
				struct switch_rtcp_senderinfo* sr = (struct switch_rtcp_senderinfo*)rtp_session->rtcp_recv_msg.body;

				rtp_session->rtcp_fresh_frame = 1;

				rtp_session->stats.rtcp.packet_count += ntohl(sr->pc);
				rtp_session->stats.rtcp.octet_count += ntohl(sr->oc);
				rtp_session->stats.rtcp.peer_ssrc = ntohl(sr->ssrc);

				/* sender report */
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10,"Received a SR with %d report blocks, " \
								  "length in words = %d, " \
								  "SSRC = 0x%X, " \
								  "NTP MSW = %u, " \
								  "NTP LSW = %u, " \
								  "RTP timestamp = %u, " \
								  "Sender Packet Count = %u, " \
								  "Sender Octet Count = %u\n",
								  rtp_session->rtcp_recv_msg.header.count,
								  ntohs((uint16_t)rtp_session->rtcp_recv_msg.header.length),
								  ntohl(sr->ssrc),
								  ntohl(sr->ntp_msw),
								  ntohl(sr->ntp_lsw),
								  ntohl(sr->ts),
								  ntohl(sr->pc),
								  ntohl(sr->oc));
			}
		} else {
			if (rtp_session->rtcp_recv_msg.header.version != 2) {
				if (rtp_session->rtcp_recv_msg.header.version == 0) {
					if (rtp_session->ice.ice_user) {
						handle_ice(rtp_session, &rtp_session->rtcp_ice, (void *) &rtp_session->rtcp_recv_msg, *bytes);
					}
				} else {

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), 
									  SWITCH_LOG_DEBUG, "Received an unsupported RTCP packet version %d\nn", rtp_session->rtcp_recv_msg.header.version);
				}
			}
		
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}



static int rtp_common_read(switch_rtp_t *rtp_session, switch_payload_t *payload_type, switch_frame_flag_t *flags, switch_io_flag_t io_flags)
{
	switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
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

	if (session) {
		channel = switch_core_session_get_channel(session);
	}

	if (!switch_rtp_ready(rtp_session)) {
		return -1;
	}

	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
		sleep_mss = rtp_session->timer.interval * 1000;
	}

	READ_INC(rtp_session);



	while (switch_rtp_ready(rtp_session)) {
		int do_cng = 0;
		int read_pretriggered = 0;
		bytes = 0;

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
			if ((switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTOFLUSH) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_STICKY_FLUSH)) &&
				!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) && 
				!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) && 
				!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL) &&
				rtp_session->read_pollfd) {
				if (switch_poll(rtp_session->read_pollfd, 1, &fdr, 0) == SWITCH_STATUS_SUCCESS) {
					status = read_rtp_packet(rtp_session, &bytes, flags, SWITCH_FALSE);
					/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Initial (%i) %d\n", status, bytes); */
					if (status != SWITCH_STATUS_FALSE) {
						read_pretriggered = 1;
					}

					if (bytes) {
						if (switch_poll(rtp_session->read_pollfd, 1, &fdr, 0) == SWITCH_STATUS_SUCCESS) {
							rtp_session->hot_hits++;//+= rtp_session->samples_per_interval;
							
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "%s Hot Hit %d\n", 
											  switch_core_session_get_name(session),
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

			if (hot_socket && (rtp_session->hot_hits % 10) != 0) { 
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "%s timer while HOT\n", switch_core_session_get_name(session));
				switch_core_timer_next(&rtp_session->timer);
			} else if (hot_socket) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "%s skip timer once\n", switch_core_session_get_name(session));
				rtp_session->sync_packets++;
				switch_core_timer_sync(&rtp_session->timer);
			} else {
				
				if (rtp_session->sync_packets) {

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10,
									  "%s Auto-Flush catching up %d packets (%d)ms.\n",
									  switch_core_session_get_name(session),
									  rtp_session->sync_packets, (rtp_session->ms_per_packet * rtp_session->sync_packets) / 1000);
					switch_core_timer_sync(&rtp_session->timer);
					rtp_session->hot_hits = 0;
				} else {

					switch_core_timer_next(&rtp_session->timer);
				}

				rtp_session->sync_packets = 0;
			}
		}

	recvfrom:

		rtp_session->stats.read_count++;

		if (!read_pretriggered) {
			bytes = 0;
		}
		read_loops++;
		//poll_loop = 0;

		if (!switch_rtp_ready(rtp_session)) {
			break;
		}
		
		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) && rtp_session->read_pollfd) {
			int pt = poll_sec * 1000000;

			do_2833(rtp_session, session);

			if ((rtp_session->ice.ice_user && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) || rtp_session->dtmf_data.out_digit_dur > 0 || rtp_session->dtmf_data.in_digit_sanity || rtp_session->sending_dtmf || 
				switch_queue_size(rtp_session->dtmf_data.dtmf_queue) || switch_queue_size(rtp_session->dtmf_data.dtmf_inqueue)) {
				pt = 20000;
			}
			
			if ((io_flags & SWITCH_IO_FLAG_NOBLOCK)) {
				pt = 0;
			}

			poll_status = switch_poll(rtp_session->read_pollfd, 1, &fdr, pt);

			
			if (rtp_session->dtmf_data.out_digit_dur > 0) {
				return_cng_frame();
			}
		}

		if (poll_status == SWITCH_STATUS_SUCCESS) {
			if (read_pretriggered) {
				read_pretriggered = 0;
			} else {
				status = read_rtp_packet(rtp_session, &bytes, flags, SWITCH_TRUE);
				//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Read bytes (%i) %ld\n", status, bytes); 
			}
			poll_loop = 0;
		} else {
			int vid_cng = 1, ice = 0;

			if (!SWITCH_STATUS_IS_BREAK(poll_status) && poll_status != SWITCH_STATUS_TIMEOUT) {
				char tmp[128] = "";
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Poll failed with error: %d [%s]\n",
					poll_status, switch_strerror_r(poll_status, tmp, sizeof(tmp)));
				ret = -1;
				goto end;
			}

			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
				if (rtp_session->rtcp_ice.ice_user) {
					if (ice_out(rtp_session, &rtp_session->rtcp_ice) != SWITCH_STATUS_SUCCESS) {
						ret = -1;
						goto end;
					}
					vid_cng = 0;
					ice = 1;
				}

				if (rtp_session->ice.ice_user) {
					if (ice_out(rtp_session, &rtp_session->ice) != SWITCH_STATUS_SUCCESS) {
						ret = -1;
						goto end;
					}
					vid_cng = 0;
					ice = 1;
				}
				

				if (ice) {

					if (check_srtp_and_ice(rtp_session)) {
						ret = -1;
						goto end;
					}

					if (poll_loop < 50) {
						poll_loop++;
						goto recvfrom;
					}
				}

			}
			
			poll_loop++;

			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
				rtp_session->missed_count += (poll_sec * 1000) / (rtp_session->ms_per_packet ? rtp_session->ms_per_packet / 1000 : 20);
				bytes = 0;

				if (rtp_session->max_missed_packets) {
					if (rtp_session->missed_count >= rtp_session->max_missed_packets) {
						ret = -2;
						goto end;
					}
				}
			}

			if ((!(io_flags & SWITCH_IO_FLAG_NOBLOCK)) && 
				(rtp_session->dtmf_data.out_digit_dur == 0 || (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) && vid_cng))) {
				return_cng_frame();
			}
		}


		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP) && rtp_session->rtcp_read_pollfd) {
			rtcp_poll_status = switch_poll(rtp_session->rtcp_read_pollfd, 1, &rtcp_fdr, 0);
						
			if (rtcp_poll_status == SWITCH_STATUS_SUCCESS) {
				rtcp_status = read_rtcp_packet(rtp_session, &rtcp_bytes, flags);
				
				if (rtcp_status == SWITCH_STATUS_SUCCESS) {
					switch_rtp_reset_media_timer(rtp_session);

					if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RTCP_PASSTHRU)) {
						switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
						switch_channel_t *channel = switch_core_session_get_channel(session);

						const char *uuid = switch_channel_get_partner_uuid(channel);
						if (uuid) {
							switch_core_session_t *other_session;
							switch_rtp_t *other_rtp_session = NULL;

							if ((other_session = switch_core_session_locate(uuid))) {
								switch_channel_t *other_channel = switch_core_session_get_channel(other_session);					
								if ((other_rtp_session = switch_channel_get_private(other_channel, "__rtcp_audio_rtp_session")) && 
									other_rtp_session->rtcp_sock_output &&
									switch_test_flag(other_rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
									*other_rtp_session->rtcp_send_msg.body = *rtp_session->rtcp_recv_msg.body;

#ifdef ENABLE_SRTP
									if (switch_test_flag(other_rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
										int sbytes = (int) rtcp_bytes;
										int stat = srtp_protect_rtcp(other_rtp_session->send_ctx, &other_rtp_session->rtcp_send_msg.header, &sbytes);
										if (stat) {
											switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error: SRTP RTCP protection failed with code %d\n", stat);
										}
										rtcp_bytes = sbytes;
									}
#endif

#ifdef ENABLE_ZRTP
									/* ZRTP Send */
									if (zrtp_on && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
										unsigned int sbytes = (unsigned int) bytes;
										zrtp_status_t stat = zrtp_status_fail;
									
										stat = zrtp_process_rtcp(other_rtp_session->zrtp_stream, (void *) &other_rtp_session->rtcp_send_msg, &sbytes);
									
										switch (stat) {
										case zrtp_status_ok:
											break;
										case zrtp_status_drop:
											switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error: zRTP protection drop with code %d\n", stat);
											ret = (int) bytes;
											goto end;
											break;
										case zrtp_status_fail:
											switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
											break;
										default:
											break;
										}
									
										bytes = sbytes;
									}
#endif
									if (switch_socket_sendto(other_rtp_session->rtcp_sock_output, other_rtp_session->rtcp_remote_addr, 0, 
															 (const char*)&other_rtp_session->rtcp_send_msg, &rtcp_bytes ) != SWITCH_STATUS_SUCCESS) {
										switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,"RTCP packet not written\n");
									}
								
								
								}
								switch_core_session_rwunlock(other_session);
							}
						}
					
					}
				}
			}
		}


		if (bytes && rtp_session->recv_msg.header.version == 2 && 
			!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL) &&
			rtp_session->recv_msg.header.pt != 13 && 
			rtp_session->recv_msg.header.pt != rtp_session->recv_te && 
			(!rtp_session->cng_pt || rtp_session->recv_msg.header.pt != rtp_session->cng_pt) && 
			rtp_session->recv_msg.header.pt != rtp_session->rpayload && !(rtp_session->rtp_bugs & RTP_BUG_ACCEPT_ANY_PACKETS)) {
			/* drop frames of incorrect payload number and return CNG frame instead */
			return_cng_frame();			
		}

		if (!bytes && (io_flags & SWITCH_IO_FLAG_NOBLOCK)) {
			rtp_session->missed_count = 0;
			ret = 0;
			goto end;
		}

		if (rtp_session->max_missed_packets && read_loops == 1) {
			if (bytes) {
				rtp_session->missed_count = 0;
			} else if (++rtp_session->missed_count >= rtp_session->max_missed_packets) {
				ret = -2;
				goto end;
			}
		}

		check = !bytes;

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_FLUSH)) {
			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
				do_flush(rtp_session);
				bytes = 0;
			}
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_FLUSH);
		}
		
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_BREAK) || (bytes && bytes == 4 && *((int *) &rtp_session->recv_msg) == UINT_MAX)) {
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_BREAK);

			if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK) || !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) || 
				switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL) || 
				(bytes && bytes < 5) || (!bytes && poll_loop)) {
				bytes = 0;
				return_cng_frame();
			}
		}


		if (bytes && bytes < 5) {
			continue;
		}

		if (!bytes && poll_loop) {
			goto recvfrom;
		}

		if (bytes && rtp_session->recv_msg.header.m && rtp_session->recv_msg.header.pt != rtp_session->recv_te && 
			!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) && !(rtp_session->rtp_bugs & RTP_BUG_IGNORE_MARK_BIT)) {
			rtp_flush_read_buffer(rtp_session, SWITCH_RTP_FLUSH_ONCE);
		}


		if (bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_DEBUG_RTP_READ)) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");

			if (!session) {
				switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_DEBUG_RTP_READ);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "RTP HAS NO SESSION!\n");
			} else {
				const char *tx_host;
				const char *old_host;
				const char *my_host;

				char bufa[30], bufb[30], bufc[30];


				tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->from_addr);
				old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
				my_host = switch_get_addr(bufc, sizeof(bufc), rtp_session->local_addr);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_CONSOLE,
								  "R %s b=%4ld %s:%u %s:%u %s:%u pt=%d ts=%u m=%d\n",
								  switch_channel_get_name(switch_core_session_get_channel(session)),
								  (long) bytes,
								  my_host, switch_sockaddr_get_port(rtp_session->local_addr),
								  old_host, rtp_session->remote_port,
								  tx_host, switch_sockaddr_get_port(rtp_session->from_addr),
								  rtp_session->recv_msg.header.pt, ntohl(rtp_session->recv_msg.header.ts), rtp_session->recv_msg.header.m);

			}
		}

		if (((rtp_session->cng_pt && rtp_session->recv_msg.header.pt == rtp_session->cng_pt) || rtp_session->recv_msg.header.pt == 13)) {
			*flags |= SFF_NOT_AUDIO;
		} else {
			*flags &= ~SFF_NOT_AUDIO; /* If this flag was already set, make sure to remove it when we get real audio */
		}


		/* ignore packets not meant for us unless the auto-adjust window is open */
		if (bytes) {
			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ)) {
				if (((rtp_session->cng_pt && rtp_session->recv_msg.header.pt == rtp_session->cng_pt) || rtp_session->recv_msg.header.pt == 13)) {
					goto recvfrom;

				}
			} else if (!(rtp_session->rtp_bugs & RTP_BUG_ACCEPT_ANY_PACKETS) && !switch_cmp_addr(rtp_session->from_addr, rtp_session->remote_addr)) {
				goto recvfrom;

			}
		}

		if (bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ) && switch_sockaddr_get_port(rtp_session->from_addr)) {
			if (!switch_cmp_addr(rtp_session->from_addr, rtp_session->remote_addr)) {
				if (++rtp_session->autoadj_tally >= 10) {
					const char *err;
					uint32_t old = rtp_session->remote_port;
					const char *tx_host;
					const char *old_host;
					char bufa[30], bufb[30];
					char adj_port[6];

					tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->from_addr);
					old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
									  "Auto Changing port from %s:%u to %s:%u\n", old_host, old, tx_host,
									  switch_sockaddr_get_port(rtp_session->from_addr));

					if (channel) {
						switch_channel_set_variable(channel, "remote_media_ip_reported", switch_channel_get_variable(channel, "remote_media_ip"));
						switch_channel_set_variable(channel, "rtp_auto_adjust_ip", tx_host);
						switch_channel_set_variable(channel, "remote_media_ip", tx_host);
						switch_snprintf(adj_port, sizeof(adj_port), "%u", switch_sockaddr_get_port(rtp_session->from_addr));
						switch_channel_set_variable(channel, "remote_media_port_reported", switch_channel_get_variable(channel, "remote_media_port"));
						switch_channel_set_variable(channel, "remote_media_port", adj_port);
						switch_channel_set_variable(channel, "rtp_auto_adjust_port", adj_port);
						switch_channel_set_variable(channel, "rtp_auto_adjust", "true");
					}
					rtp_session->auto_adj_used = 1;
					switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(rtp_session->from_addr), 0, SWITCH_FALSE, &err);
					switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Correct ip/port confirmed.\n");
				switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
				rtp_session->auto_adj_used = 0;
			}
		}

		if (bytes && rtp_session->autoadj_window) {
			if (--rtp_session->autoadj_window == 0) {
				switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
			}
		}

		if (bytes && (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL))) {
			/* Fast PASS! */
			*flags |= SFF_PROXY_PACKET;

			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
#if 0
				if (rtp_session->recv_msg.header.version == 2 && rtp_session->recv_msg.header.pt == rtp_session->rpayload) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, 
									  "Ignoring udptl packet of size of %ld bytes that looks strikingly like a RTP packet.\n", (long)bytes);
					bytes = 0;
					goto do_continue;					
				}
#endif
				*flags |= SFF_UDPTL_PACKET;
			} else {
				check_srtp_and_ice(rtp_session);
			}

			ret = (int) bytes;
			goto end;
		}

		if (bytes) {
			rtp_session->missed_count = 0;

			if (bytes < rtp_header_len) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Ignoring invalid RTP packet size of %ld bytes.\n", (long)bytes);
				bytes = 0;
				goto do_continue;
			}
			
			if (rtp_session->recv_msg.header.pt && (rtp_session->recv_msg.header.pt == rtp_session->cng_pt || rtp_session->recv_msg.header.pt == 13)) {
				return_cng_frame();
			}
		}


		if (check_srtp_and_ice(rtp_session)) {
			ret = -1;
			goto end;
		}
		

		if (check || bytes) {
			do_2833(rtp_session, session);
		}

		if (bytes && rtp_session->recv_msg.header.version != 2) {
			uint8_t *data = (uint8_t *) RTP_BODY(rtp_session);

			if (rtp_session->recv_msg.header.version == 0) {
				if (rtp_session->ice.ice_user) {
					handle_ice(rtp_session, &rtp_session->ice, (void *) &rtp_session->recv_msg, bytes);
					goto recvfrom;
				} else if (rtp_session->remote_stun_addr) {
					handle_stun_ping_reply(rtp_session, (void *) &rtp_session->recv_msg, bytes);
					goto recvfrom;
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
			goto result_continue;
		}

	result_continue:
	timer_check:

		if (do_cng) {
			uint8_t *data = (uint8_t *) RTP_BODY(rtp_session);

			do_2833(rtp_session, session);

			if (rtp_session->last_cng_ts == rtp_session->last_read_ts + rtp_session->samples_per_interval) {
				rtp_session->last_cng_ts = 0;
			} else {
				rtp_session->last_cng_ts = rtp_session->last_read_ts + rtp_session->samples_per_interval;
			}

			memset(data, 0, 2);
			data[0] = 65;
			rtp_session->recv_msg.header.pt = (uint32_t) rtp_session->cng_pt ? rtp_session->cng_pt : SWITCH_RTP_CNG_PAYLOAD;
			*flags |= SFF_CNG;
			*payload_type = (switch_payload_t) rtp_session->recv_msg.header.pt;
			ret = 2 + rtp_header_len;
			rtp_session->stats.inbound.skip_packet_count++;
			goto end;
		}


		if (check || (bytes && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER))) {
			if (!bytes && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {	/* We're late! We're Late! */
				if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_NOBLOCK) && status == SWITCH_STATUS_BREAK) {
					switch_cond_next();
					continue;
				}
				
				return_cng_frame();
			}
		}
		
		if (status == SWITCH_STATUS_BREAK || bytes == 0) {
			if (!(io_flags & SWITCH_IO_FLAG_SINGLE_READ) && switch_test_flag(rtp_session, SWITCH_RTP_FLAG_DATAWAIT)) {
				goto do_continue;
			}
			return_cng_frame();
		}

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_GOOGLEHACK) && rtp_session->recv_msg.header.pt == 102) {
			rtp_session->recv_msg.header.pt = 97;
		}

		break;

	do_continue:

		if (!bytes && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
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
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		_dtmf = (switch_dtmf_t *) pop;
		*dtmf = *_dtmf;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RTP RECV DTMF %c:%d\n", dtmf->digit, dtmf->duration);
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

	bytes = rtp_common_read(rtp_session, payload_type, flags, io_flags);

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

	if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_ENABLE_RTCP)) {
		return SWITCH_STATUS_FALSE;
	}

	/* A fresh frame has been found! */
	if (rtp_session->rtcp_fresh_frame) {
		struct switch_rtcp_senderinfo* sr = (struct switch_rtcp_senderinfo*)rtp_session->rtcp_recv_msg.body;
		/* we remove the header lenght because with directly have a pointer on the body */
		unsigned packet_length = (ntohs((uint16_t) rtp_session->rtcp_recv_msg.header.length) + 1) * 4 - sizeof(switch_rtcp_hdr_t);
		unsigned int reportsOffset = sizeof(struct switch_rtcp_senderinfo);
		int i = 0;
		unsigned int offset;

		/* turn the flag off! */
		rtp_session->rtcp_fresh_frame = 0;

		frame->ssrc = ntohl(sr->ssrc);
		frame->packet_type = (uint16_t)rtp_session->rtcp_recv_msg.header.type;
		frame->ntp_msw = ntohl(sr->ntp_msw);
		frame->ntp_lsw = ntohl(sr->ntp_lsw);
		frame->timestamp = ntohl(sr->ts);
		frame->packet_count =  ntohl(sr->pc);
		frame->octect_count = ntohl(sr->oc);

		for (offset = reportsOffset; offset < packet_length; offset += sizeof(struct switch_rtcp_report_block)) {
			struct switch_rtcp_report_block* report = (struct switch_rtcp_report_block*) (rtp_session->rtcp_recv_msg.body + offset);
			frame->reports[i].ssrc = ntohl(report->ssrc);
			frame->reports[i].fraction = (uint8_t)ntohl(report->fraction);
			frame->reports[i].lost = ntohl(report->lost);
			frame->reports[i].highest_sequence_number_received = ntohl(report->highest_sequence_number_received);
			frame->reports[i].jitter = ntohl(report->jitter);
			frame->reports[i].lsr = ntohl(report->lsr);
			frame->reports[i].dlsr = ntohl(report->dlsr);
			i++;
			if (i >= MAX_REPORT_BLOCKS) {
				break;
			}
		}
		frame->report_count = (uint16_t)i;

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

	bytes = rtp_common_read(rtp_session, &frame->payload, &frame->flags, io_flags);

	frame->data = RTP_BODY(rtp_session);
	frame->packet = &rtp_session->recv_msg;
	frame->packetlen = bytes;
	frame->source = __FILE__;

	switch_set_flag(frame, SFF_RAW_RTP);
	if (frame->payload == rtp_session->recv_te) {
		switch_set_flag(frame, SFF_RFC2833);
	}
	frame->timestamp = ntohl(rtp_session->recv_msg.header.ts);
	frame->seq = (uint16_t) ntohs((uint16_t) rtp_session->recv_msg.header.seq);
	frame->ssrc = ntohl(rtp_session->recv_msg.header.ssrc);
	frame->m = rtp_session->recv_msg.header.m ? SWITCH_TRUE : SWITCH_FALSE;

#ifdef ENABLE_ZRTP
	if (zrtp_on && switch_test_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV)) {
		zrtp_session_info_t zrtp_session_info;

		if (rtp_session->zrtp_session && (zrtp_status_ok == zrtp_session_get(rtp_session->zrtp_session, &zrtp_session_info))) {
			if (zrtp_session_info.sas_is_ready) {
				switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
				switch_channel_t *channel = switch_core_session_get_channel(session);

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
										switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
										switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
										switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
										switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
									} else if (zrtp_status_ok == zrtp_resolve_mitm_call(other_rtp_session->zrtp_stream, rtp_session->zrtp_stream)) {
										switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
										switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
										switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
										switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
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
			switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
			switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
		}
	}
#endif

	if (bytes < 0) {
		frame->datalen = 0;
		return bytes == -2 ? SWITCH_STATUS_TIMEOUT : SWITCH_STATUS_GENERR;
	} else if (bytes < rtp_header_len) {
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
		return SWITCH_STATUS_FALSE;
	}

	WRITE_INC(rtp_session);

	if (send_msg) {
		bytes = datalen;
		if (flags && *flags & SFF_RFC2833) {
			send_msg->header.pt = rtp_session->te;
		}
		data = send_msg->body;
		if (datalen > rtp_header_len) {
			datalen -= rtp_header_len;
		}
	} else {
		if (*flags & SFF_RFC2833) {
			payload = rtp_session->te;
		}

		send_msg = &rtp_session->send_msg;
		send_msg->header.pt = payload;

		m = get_next_write_ts(rtp_session, timestamp);

		rtp_session->send_msg.header.ts = htonl(rtp_session->ts);

		memcpy(send_msg->body, data, datalen);
		bytes = datalen + rtp_header_len;
	}


	if ((rtp_session->rtp_bugs & RTP_BUG_NEVER_SEND_MARKER)) {
		m = 0;
	} else {
		if ((rtp_session->last_write_ts != RTP_TS_RESET && rtp_session->ts > (rtp_session->last_write_ts + (rtp_session->samples_per_interval * 10)))
			|| rtp_session->ts == rtp_session->samples_per_interval) {
			m++;
		}

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) && 
			(rtp_session->timer.samplecount - rtp_session->last_write_samplecount) > rtp_session->samples_per_interval * 10) {
			m++;
		}
			
		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER) &&
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
		rtp_session->last_write_ts = RTP_TS_RESET;
		rtp_session->ts = 0;
	}

	/* If the marker was set, and the timestamp seems to have started over - set a new SSRC, to indicate this is a new stream */
	if (m && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND) && (rtp_session->rtp_bugs & RTP_BUG_CHANGE_SSRC_ON_MARKER) && 
		(rtp_session->last_write_ts == RTP_TS_RESET || (rtp_session->ts <= rtp_session->last_write_ts && rtp_session->last_write_ts > 0))) {
		switch_rtp_set_ssrc(rtp_session, (uint32_t) ((intptr_t) rtp_session + (uint32_t) switch_epoch_time_now(NULL)));
	}

	if (!switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) && !switch_rtp_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
		send_msg->header.m = (m && !(rtp_session->rtp_bugs & RTP_BUG_NEVER_SEND_MARKER)) ? 1 : 0;
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

	this_ts = ntohl(send_msg->header.ts);

	if (abs(rtp_session->last_write_ts - this_ts) > 16000) {
		rtp_session->last_write_ts = RTP_TS_RESET;
	}

	if (!switch_rtp_ready(rtp_session) || rtp_session->sending_dtmf || !this_ts || 
		(rtp_session->last_write_ts > RTP_TS_RESET && this_ts < rtp_session->last_write_ts)) {
		send = 0;
	}


	if (send) {
		send_msg->header.seq = htons(++rtp_session->seq);

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_BYTESWAP) && send_msg->header.pt == rtp_session->payload) {
			switch_swap_linear((int16_t *)send_msg->body, (int) datalen);
		}

#ifdef ENABLE_SRTP
		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
			int sbytes = (int) bytes;
			err_status_t stat;


			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET)) {
				switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
				switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET);
				srtp_dealloc(rtp_session->send_ctx);
				rtp_session->send_ctx = NULL;
				if ((stat = srtp_create(&rtp_session->send_ctx, &rtp_session->send_policy))) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP SEND\n");
					ret = -1;
					goto end;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "RE-Activating Secure RTP SEND\n");
				}
			}


			stat = srtp_protect(rtp_session->send_ctx, &send_msg->header, &sbytes);
			if (stat) {
				switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error: SRTP protection failed with code %d\n", stat);
			}

			bytes = sbytes;
		}
#endif
#ifdef ENABLE_ZRTP
		/* ZRTP Send */
		if (zrtp_on && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
			unsigned int sbytes = (int) bytes;
			zrtp_status_t stat = zrtp_status_fail;
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");

			stat = zrtp_process_rtp(rtp_session->zrtp_stream, (void *) send_msg, &sbytes);

			switch (stat) {
			case zrtp_status_ok:
				break;
			case zrtp_status_drop:
				/* switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Error: zRTP protection drop with code %d\n", stat); */
				ret = (int) bytes;
				goto end;
				break;
			case zrtp_status_fail:
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
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

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_DEBUG_RTP_WRITE)) {
			switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");

			if (!session) {
				switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_DEBUG_RTP_WRITE);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "RTP HAS NO SESSION!\n");
			} else {
				const char *tx_host;
				const char *old_host;
				const char *my_host;

				char bufa[30], bufb[30], bufc[30];


				tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->from_addr);
				old_host = switch_get_addr(bufb, sizeof(bufb), rtp_session->remote_addr);
				my_host = switch_get_addr(bufc, sizeof(bufc), rtp_session->local_addr);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_CONSOLE,
								  "W %s b=%4ld %s:%u %s:%u %s:%u pt=%d ts=%u m=%d\n",
								  switch_channel_get_name(switch_core_session_get_channel(session)),
								  (long) bytes,
								  my_host, switch_sockaddr_get_port(rtp_session->local_addr),
								  old_host, rtp_session->remote_port,
								  tx_host, switch_sockaddr_get_port(rtp_session->from_addr),
								  send_msg->header.pt, ntohl(send_msg->header.ts), send_msg->header.m);

			}
		}


		if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, (void *) send_msg, &bytes) != SWITCH_STATUS_SUCCESS) {
			rtp_session->seq--;
			ret = -1;
			goto end;
		}
		rtp_session->last_write_ts = this_ts;

		if (rtp_session->queue_delay) {
			rtp_session->delay_samples = rtp_session->queue_delay;
			rtp_session->queue_delay = 0;
		}



		rtp_session->stats.outbound.raw_bytes += bytes;
		rtp_session->stats.outbound.packet_count++;

		if (rtp_session->cng_pt && send_msg->header.pt == rtp_session->cng_pt) {
			rtp_session->stats.outbound.cng_packet_count++;
		} else {
			rtp_session->stats.outbound.media_packet_count++;
			rtp_session->stats.outbound.media_bytes += bytes;
		}

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_USE_TIMER)) {
			rtp_session->last_write_samplecount = rtp_session->timer.samplecount;
		} else {
			rtp_session->last_write_timestamp = switch_micro_time_now();
		}
		
	}

#if 0
	if (rtp_session->ice.ice_user) {
		if (ice_out(rtp_session, &rtp_session->ice) != SWITCH_STATUS_SUCCESS) {
			ret = -1;
			goto end;
		}
	}
#endif

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

	if (switch_true(switch_channel_get_variable(switch_core_session_get_channel(session), "fire_talk_events"))) {
		rtp_session->vad_data.fire_events |= VAD_FIRE_TALK;
	}

	if (switch_true(switch_channel_get_variable(switch_core_session_get_channel(session), "fire_not_talk_events"))) {
		rtp_session->vad_data.fire_events |= VAD_FIRE_NOT_TALK;
	}
	

	if (switch_core_codec_init(&rtp_session->vad_data.vad_codec,
							   codec->implementation->iananame,
							   NULL,
							   codec->implementation->samples_per_second,
							   codec->implementation->microseconds_per_packet / 1000,
							   codec->implementation->number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, rtp_session->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't load codec?\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Activate VAD codec %s %dms\n", codec->implementation->iananame,
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
	switch_set_flag_locked(rtp_session, SWITCH_RTP_FLAG_VAD);
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

	if (!switch_rtp_ready(rtp_session) || !rtp_session->remote_addr) {
		return -1;
	}
	
	if (switch_test_flag(frame, SFF_PROXY_PACKET) || switch_test_flag(frame, SFF_UDPTL_PACKET) ||
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
		
	//if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA) || switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL)) {
		switch_size_t bytes;
		//char bufa[30];

		/* Fast PASS! */
		if (!switch_test_flag(frame, SFF_PROXY_PACKET) && !switch_test_flag(frame, SFF_UDPTL_PACKET)) {
			return 0;
		}
		bytes = frame->packetlen;
		//tx_host = switch_get_addr(bufa, sizeof(bufa), rtp_session->remote_addr);

		send_msg = frame->packet;

		if (!switch_test_flag(rtp_session, SWITCH_RTP_FLAG_UDPTL) && !switch_test_flag(frame, SFF_UDPTL_PACKET)) {

			if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO) && rtp_session->payload > 0) {
				send_msg->header.pt = rtp_session->payload;
			}
		
			send_msg->header.ssrc = htonl(rtp_session->ssrc);
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
	if (zrtp_on && switch_test_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND)) {
		zrtp_session_info_t zrtp_session_info;

		if (zrtp_status_ok == zrtp_session_get(rtp_session->zrtp_session, &zrtp_session_info)) {
			if (zrtp_session_info.sas_is_ready) {
				switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
				switch_channel_t *channel = switch_core_session_get_channel(session);

				const char *uuid = switch_channel_get_partner_uuid(channel);
				if (uuid) {
					switch_core_session_t *other_session;

					if ((other_session = switch_core_session_locate(uuid))) {
						switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
						switch_rtp_t *other_rtp_session = switch_channel_get_private(other_channel, "__zrtp_audio_rtp_session");


						if (other_rtp_session) {
							if (zrtp_status_ok == zrtp_session_get(other_rtp_session->zrtp_session, &zrtp_session_info)) {
								if (rtp_session->zrtp_mitm_tries > ZRTP_MITM_TRIES) {
									switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
									switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
									switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
									switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
								} else if (zrtp_status_ok == zrtp_resolve_mitm_call(other_rtp_session->zrtp_stream, rtp_session->zrtp_stream)) {
									switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
									switch_clear_flag(rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
									switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_RECV);
									switch_clear_flag(other_rtp_session, SWITCH_ZRTP_FLAG_SECURE_MITM_SEND);
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

	fwd = (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) && switch_test_flag(frame, SFF_RAW_RTP)) ? 1 : 0;

	if (!fwd && !rtp_session->sending_dtmf && !rtp_session->queue_delay && 
		switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) && (rtp_session->rtp_bugs & RTP_BUG_GEN_ONE_GEN_ALL)) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Generating RTP locally but timestamp passthru is configured, disabling....\n");
		switch_clear_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE);
		rtp_session->last_write_ts = RTP_TS_RESET;
	}

	switch_assert(frame != NULL);

	if (switch_test_flag(frame, SFF_CNG)) {
		if (rtp_session->cng_pt) {
			payload = rtp_session->cng_pt;
		} else {
			return (int) frame->packetlen;
		}
	} else {
		payload = rtp_session->payload;
	}

	if (switch_test_flag(frame, SFF_RTP_HEADER)) {
		switch_size_t wrote = switch_rtp_write_manual(rtp_session, frame->data, frame->datalen,
													  frame->m, frame->payload, (uint32_t) (frame->timestamp), &frame->flags);

		rtp_session->stats.outbound.raw_bytes += wrote;
		rtp_session->stats.outbound.media_bytes += wrote;
		rtp_session->stats.outbound.media_packet_count++;
		rtp_session->stats.outbound.packet_count++;
	}

	if (fwd) {
		send_msg = frame->packet;
		len = frame->packetlen;
		ts = 0;
		// Trying this based on http://jira.freeswitch.org/browse/MODSOFIA-90
		//if (frame->codec && frame->codec->agreed_pt == frame->payload) {
		send_msg->header.pt = payload;
		//}
	} else {
		data = frame->data;
		len = frame->datalen;
		ts = switch_test_flag(rtp_session, SWITCH_RTP_FLAG_RAW_WRITE) ? (uint32_t) frame->timestamp : 0;
	}

	/*
	  if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO)) {
	  send_msg->header.pt = rtp_session->payload;
	  }
	*/

	return rtp_common_write(rtp_session, send_msg, data, len, payload, ts, &frame->flags);
}

SWITCH_DECLARE(switch_rtp_stats_t *) switch_rtp_get_stats(switch_rtp_t *rtp_session, switch_memory_pool_t *pool)
{
	switch_rtp_stats_t *s;

	if (pool) {
		s = switch_core_alloc(pool, sizeof(*s));
		*s = rtp_session->stats;
	} else {
		s = &rtp_session->stats;
	}

	if (rtp_session->jb) {
		s->inbound.largest_jb_size = stfu_n_get_most_qlen(rtp_session->jb);
	}

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

	WRITE_INC(rtp_session);

	rtp_session->write_msg = rtp_session->send_msg;
	rtp_session->write_msg.header.seq = htons(++rtp_session->seq);
	rtp_session->write_msg.header.ts = htonl(ts);
	rtp_session->write_msg.header.pt = payload;
	rtp_session->write_msg.header.m = m;
	memcpy(rtp_session->write_msg.body, data, datalen);

	bytes = rtp_header_len + datalen;

#ifdef ENABLE_SRTP
	if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND)) {
		switch_core_session_t *session = switch_core_memory_pool_get_data(rtp_session->pool, "__session");
		int sbytes = (int) bytes;
		err_status_t stat;

		if (switch_test_flag(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET)) {
			switch_clear_flag_locked(rtp_session, SWITCH_RTP_FLAG_SECURE_SEND_RESET);
			srtp_dealloc(rtp_session->send_ctx);
			rtp_session->send_ctx = NULL;
			if ((stat = srtp_create(&rtp_session->send_ctx, &rtp_session->send_policy))) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error! RE-Activating Secure RTP SEND\n");
				ret = -1;
				goto end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "RE-Activating Secure RTP SEND\n");
			}
		}

		stat = srtp_protect(rtp_session->send_ctx, &rtp_session->write_msg.header, &sbytes);
		if (stat) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error: SRTP protection failed with code %d\n", stat);
		}
		bytes = sbytes;
	}
#endif
#ifdef ENABLE_ZRTP
	/* ZRTP Send */
	if (zrtp_on && !switch_test_flag(rtp_session, SWITCH_RTP_FLAG_PROXY_MEDIA)) {
		unsigned int sbytes = (int) bytes;
		zrtp_status_t stat = zrtp_status_fail;

		stat = zrtp_process_rtp(rtp_session->zrtp_stream, (void *) &rtp_session->write_msg, &sbytes);

		switch (stat) {
		case zrtp_status_ok:
			break;
		case zrtp_status_drop:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection drop with code %d\n", stat);
			ret = (int) bytes;
			goto end;
			break;
		case zrtp_status_fail:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: zRTP protection fail with code %d\n", stat);
			break;
		default:
			break;
		}

		bytes = sbytes;
	}
#endif

	if (switch_socket_sendto(rtp_session->sock_output, rtp_session->remote_addr, 0, (void *) &rtp_session->write_msg, &bytes) != SWITCH_STATUS_SUCCESS) {
		rtp_session->seq--;
		ret = -1;
		goto end;
	}

	if (((*flags) & SFF_RTP_HEADER)) {
		rtp_session->last_write_ts = ts;
	}

	ret = (int) bytes;

 end:

	WRITE_DEC(rtp_session);

	return ret;
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

