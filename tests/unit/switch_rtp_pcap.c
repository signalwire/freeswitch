/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2021, Anthony Minessale II <anthm@freeswitch.org>
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
* Dragos Oancea <dragos@signalwire.com>
*
* switch_rtp_pcap.c -- tests RTP stack using PCAP.
*/


#include <switch.h>
#include <test/switch_test.h>

/* before adding a pcap file: tcprewrite --dstipmap=X.X.X.X/32:192.168.0.1/32 --srcipmap=X.X.X.X/32:192.168.0.2/32 -i in.pcap -o out.pcap */

#include <pcap.h>

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

static const char *rx_host = "127.0.0.1";
static const char *tx_host = "127.0.0.1";
static switch_rtp_t *rtp_session = NULL;
const char *err = NULL;
switch_rtp_packet_t rtp_packet;
switch_frame_flag_t *frame_flags;
switch_io_flag_t io_flags;
switch_payload_t read_pt;
static switch_port_t audio_rx_port = 1234;

/* media_timeout configured on the test channel (ms); see rtp_test_start_call(). */
#define TEST_MEDIA_TIMEOUT_MS 1000

/* https://www.tcpdump.org/pcap.html */
/* IP header */
struct sniff_ip {
	u_char ip_vhl;		/* version << 4 | header length >> 2 */
	u_char ip_tos;		/* type of service */
	u_short ip_len;		/* total length */
	u_short ip_id;		/* identification */
	u_short ip_off;		/* fragment offset field */
#define IP_RF 0x8000		/* reserved fragment flag */
#define IP_DF 0x4000		/* dont fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff	/* mask for fragmenting bits */
	u_char ip_ttl;		/* time to live */
	u_char ip_p;		/* protocol */
	u_short ip_sum;		/* checksum */
	struct in_addr ip_src,ip_dst; /* source and dest address */
};

#define IP_HL(ip)		(((ip)->ip_vhl) & 0x0f)


static switch_status_t rtp_test_start_call(switch_core_session_t **psession)
{
	char *r_sdp;
	uint8_t match = 0, p = 0;
	switch_core_session_t *session; 
	switch_channel_t *channel = NULL;
	switch_status_t status;
	switch_media_handle_t *media_handle;
	switch_core_media_params_t *mparams;
	switch_stream_handle_t stream = { 0 };
	switch_call_cause_t cause;

	/*tone stream extension*/
	status = switch_ivr_originate(NULL, psession, &cause, "null/+1234", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
	session = *psession;

	if (!(session)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "no session\n");
		return SWITCH_STATUS_FALSE;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "switch_ivr_originate() failed\n");
		return SWITCH_STATUS_FALSE;
	}

	channel = switch_core_session_get_channel(session);
	if (!channel) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "switch_core_session_get_channel() failed\n");
		return SWITCH_STATUS_FALSE;
	}
	mparams = switch_core_session_alloc(session, sizeof(switch_core_media_params_t));
	mparams->inbound_codec_string = switch_core_session_strdup(session, "PCMU");
	mparams->outbound_codec_string = switch_core_session_strdup(session, "PCMU");
	mparams->rtpip = switch_core_session_strdup(session, (char *)rx_host);

	status = switch_media_handle_create(&media_handle, session, mparams);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "switch_media_handle_create() failed\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_channel_set_variable(channel, "absolute_codec_string", "PCMU");
	switch_channel_set_variable(channel, "send_silence_when_idle", "-1");
	switch_channel_set_variable(channel, "rtp_timer_name", "soft");
	switch_channel_set_variable(channel, "media_timeout", "1000");

	switch_channel_set_variable(channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, rx_host);
	switch_channel_set_variable_printf(channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, "%d", audio_rx_port);

	r_sdp = switch_core_session_sprintf(session,
	"v=0\n"
	"o=FreeSWITCH 1632033305 1632033306 IN IP4 %s\n"
	"s=-\n"
	"c=IN IP4 %s\n"
	"t=0 0\n"
	"m=audio 11114 RTP/AVP 0 101\n"
	"a=rtpmap:0 PCMU/8000\n"
	"a=rtpmap:101 telephone-event/8000\n"
	"a=rtcp-mux\n",
	tx_host, tx_host);
	 
	status = switch_core_media_prepare_codecs(session, SWITCH_FALSE);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "switch_core_media_prepare_codecs() failed\n");
		return SWITCH_STATUS_FALSE;
	}
	   
	match = switch_core_media_negotiate_sdp(session, r_sdp, &p, SDP_OFFER);
	if (match != 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "switch_core_media_negotiate_sdp() failed\n");
		return SWITCH_STATUS_FALSE;
	}

	status = switch_core_media_choose_ports(session, SWITCH_TRUE, SWITCH_FALSE);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "switch_core_media_choose_ports() failed\n");
		return SWITCH_STATUS_FALSE;
	}

	status = switch_core_media_activate_rtp(session);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "switch_core_media_activate_rtp() failed\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_DEBUG_RTP_READ);
	switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_DEBUG_RTP_WRITE);

	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute("fsctl", "debug_level 10", session, &stream);
	switch_safe_free(stream.data);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t rtp_test_end_call(switch_core_session_t **psession) 
{
	switch_channel_t *channel = NULL;
	switch_core_session_t *session = *psession; 

	channel = switch_core_session_get_channel(session);
	if (!channel) {
		return SWITCH_STATUS_FALSE;
	}
	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	switch_media_handle_destroy(session);
	switch_core_session_rwunlock(session);

	/* switch_media_handle_destroy() already destroyed the session's RTP session; clear both
	   pointers so callers and later tests never act on the freed objects. */
	*psession = NULL;
	rtp_session = NULL;

	return SWITCH_STATUS_SUCCESS;
}

/* Pull the next PCMU RTP packet out of a capture. Returns a pointer to the UDP payload
   (RTP header + body) and its length via rtp_len, or NULL at end of capture. The inter-packet
   gap recorded in the capture is honored so packets are replayed at their original pace.
   Non-RTP frames (too short, or not PCMU) are skipped. */
static const unsigned char *pcap_next_rtp(pcap_t *pcap, struct timeval *prev_ts, switch_size_t *rtp_len)
{
	const unsigned char *packet;
	struct pcap_pkthdr pcap_header;

	while ((packet = pcap_next(pcap, &pcap_header))) {
		const struct sniff_ip *ip;
		int jump_over;
		long diff_us = 0;

		if (prev_ts->tv_sec || prev_ts->tv_usec) {
			diff_us = (pcap_header.ts.tv_sec - prev_ts->tv_sec) * 1000000L + (pcap_header.ts.tv_usec - prev_ts->tv_usec);
		}
		*prev_ts = pcap_header.ts;

		/* need at least ethernet + minimal IP + UDP + RTP header */
		if (pcap_header.caplen <= 14 + 20 + 8 + SWITCH_RTP_HEADER_LEN) {
			continue;
		}

		ip = (const struct sniff_ip *)(packet + 14);
		jump_over = 14 /*ethernet*/ + IP_HL(ip) * 4 /*IP header*/ + 8 /*UDP header*/;

		if ((bpf_u_int32)(jump_over + SWITCH_RTP_HEADER_LEN) > pcap_header.caplen) {
			continue;
		}

		packet += jump_over;

		/* RTP version 2, PCMU (PT 0) */
		if (packet[0] != 0x80 || (packet[1] & 0x7f) != 0) {
			continue;
		}

		if (diff_us > 0) {
			switch_yield(diff_us);
		}

		*rtp_len = pcap_header.caplen - jump_over;
		return packet;
	}

	return NULL;
}

/* Feed the session's read path a few times so a datagram just sent to it gets parsed. */
static void rtp_test_pump_reads(switch_rtp_t *rtp, int rounds)
{
	char rpacket[SWITCH_RECOMMENDED_BUFFER_SIZE];
	switch_payload_t pt = 0;
	switch_frame_flag_t frameflags = 0;
	uint32_t datalen;
	int i;

	for (i = 0; i < rounds; i++) {
		datalen = sizeof(rpacket);
		switch_rtp_read(rtp, (void *)rpacket, &datalen, &pt, &frameflags, SWITCH_IO_FLAG_NONE);
	}
}

/* The RTCP reader logs "Got NACK count <n>" with the number of FCI entries it is about to process,
   i.e. the output of the bounds clamp in switch_rtp.c:rtcp_nack_fci_count(). The NACK test binds this
   logger to read that number back and assert the clamp behaved. -1 means "not seen yet". */
static volatile int captured_nack_count = -1;

static switch_status_t nack_count_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	const char *marker = "Got NACK count ";
	const char *p;

	(void) level;

	if (node && node->content && (p = strstr(node->content, marker))) {
		captured_nack_count = atoi(p + strlen(marker));
	}

	return SWITCH_STATUS_SUCCESS;
}

FST_CORE_DB_BEGIN("./conf_rtp")
{
FST_SUITE_BEGIN(switch_rtp_pcap)
{

FST_SETUP_BEGIN()
{
	fst_requires_module("mod_loopback");
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

	/*
	 * Regression test for the bounds clamp in switch_rtp.c:rtcp_nack_fci_count().
	 *
	 * An RTCP RTPFB/NACK packet carries a count of FCI feedback words implied by its 16-bit
	 * length field (words - 2, after the 3-word RTPFB header). A malicious or corrupt sender can
	 * set length far larger than the datagram actually holds. The reader must iterate only over
	 * the FCI words that are really present, not the count the header claims -- otherwise the
	 * handle_nack() loop walks off the end of the receive buffer (out-of-bounds read).
	 *
	 * The test feeds two feedback packets through the real rtcp-mux read path and, via a bound
	 * logger, reads back the FCI count the parser decided to process:
	 *   1. A well-formed NACK with an honest length: the count must equal the entries present.
	 *   2. A NACK whose length claims 0xFFFF words but that carries only two FCI words in a
	 *      20-byte datagram: the count must be clamped to the two words that fit, NOT the 65533
	 *      the length field implies.
	 * Under ASan/Valgrind the unclamped walk is additionally caught as an out-of-bounds read on
	 * the switch_rtp_read() below.
	 */
	FST_TEST_BEGIN(test_rtcp_nack_oversized_length)
	{
		switch_core_session_t *session = NULL;
		switch_status_t status;
		switch_socket_t *sock_rtp = NULL;
		switch_sockaddr_t *sock_addr = NULL;
		const char *str_err = NULL;
		switch_size_t send_len;
		int i;
		switch_log_level_t verbose = SWITCH_LOG_DEBUG10;
		int logger_bound = 0;

		/* A well-formed NACK: one FCI word, so length = 4 words - 1 = 3. 16-byte datagram.
		   fits = (16 - 12)/4 = 1, so the parser must report a count of 1. */
		unsigned char nack_ok[16] = {
			0x81, 0xcd, 0x00, 0x03,             /* V=2/P=0/FMT=1, PT=205, length=3 (honest) */
			0xde, 0xad, 0xbe, 0xef,             /* sender SSRC */
			0xca, 0xfe, 0xba, 0xbe,             /* media SSRC */
			0x00, 0x2a, 0x00, 0x00              /* FCI 0: PID=42, BLP=0 */
		};

		/* The attack packet: length claims 0xFFFF words, but only two FCI words are present in a
		   20-byte datagram. fits = (20 - 12)/4 = 2, so the parser must report a count of 2, not
		   65533 (= 0xFFFF - 2). */
		unsigned char nack_oversized[20] = {
			0x81, 0xcd, 0xff, 0xff,             /* V=2/P=0/FMT=1, PT=205, length=0xFFFF (lie) */
			0xde, 0xad, 0xbe, 0xef,             /* sender SSRC */
			0xca, 0xfe, 0xba, 0xbe,             /* media SSRC */
			0x00, 0x01, 0x00, 0x00,             /* FCI 0: PID=1,  BLP=0 */
			0x00, 0x02, 0x00, 0x00              /* FCI 1: PID=2,  BLP=0 */
		};

		status = rtp_test_start_call(&session);
		fst_requires(status == SWITCH_STATUS_SUCCESS);
		fst_requires(session);

		/* The "Got NACK count N" line is logged at DEBUG2; raise the core log level so the bound
		   logger actually receives it, and capture it. */
		switch_core_session_ctl(SCSC_LOGLEVEL, &verbose);
		captured_nack_count = -1;
		fst_requires(switch_log_bind_logger(nack_count_logger, SWITCH_LOG_DEBUG10, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS);
		logger_bound = 1;

		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_ENABLE_RTCP);

		rtp_session = switch_core_media_get_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO);
		fst_requires(rtp_session);

		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_PAUSE);
		/* This test feeds only RTCP; drop the media timeout so the silent reads don't hang up. */
		switch_rtp_set_media_timeout(rtp_session, 0);
		/* Accept the feedback from the test socket and route it through the RTPFB/NACK branch of
		   the RTCP reader (only reached when the VIDEO flag is set). */
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_AUTOADJ);
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_VIDEO);
		switch_rtp_set_flag(rtp_session, SWITCH_RTP_FLAG_NACK);

		fst_requires(switch_socket_create(&sock_rtp, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS);

		switch_sockaddr_new(&sock_addr, rx_host, audio_rx_port, switch_core_session_get_pool(session));
		fst_requires(sock_addr);

		switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(sock_addr), 0, SWITCH_FALSE, &str_err);
		switch_rtp_reset(rtp_session);

		/* 1) Honest length: expect the parser to process exactly the one FCI word present.
		   rtcp-mux means the feedback arrives on the RTP socket, so a few send/read rounds let
		   the source address be adopted and the packet reach the parser. We resend until the
		   expected count is observed (a datagram may be dropped before autoadj locks on); an honest
		   packet only ever yields a count of 1, so waiting for exactly that is race-free even if a
		   stray packet is still queued. */
		captured_nack_count = -1;
		for (i = 0; i < 40 && captured_nack_count != 1; i++) {
			send_len = sizeof(nack_ok);
			fst_requires(switch_socket_sendto(sock_rtp, sock_addr, MSG_CONFIRM, (const char *)nack_ok, &send_len) == SWITCH_STATUS_SUCCESS);
			rtp_test_pump_reads(rtp_session, 2);
		}
		fst_xcheck(captured_nack_count == 1,
				   "RTCP NACK with an honest length is read as exactly the FCI entries present");

		/* 2) Oversized length with only two FCI words present: the parser must clamp to the two
		   that fit in the datagram, never the 65533 the length field claims. A regression here is
		   also an out-of-bounds read, caught by ASan/Valgrind on the reads below. The oversized
		   packet only ever yields a count of 2, so resend until exactly 2 is seen: this ignores any
		   honest packet from step 1 still sitting in the socket buffer (which would read back as 1). */
		captured_nack_count = -1;
		for (i = 0; i < 40 && captured_nack_count != 2; i++) {
			send_len = sizeof(nack_oversized);
			fst_requires(switch_socket_sendto(sock_rtp, sock_addr, MSG_CONFIRM, (const char *)nack_oversized, &send_len) == SWITCH_STATUS_SUCCESS);
			rtp_test_pump_reads(rtp_session, 2);
		}
		fst_xcheck(captured_nack_count == 2,
				   "oversized RTCP NACK length is clamped to the FCI entries that fit the datagram");

		/* The session survived parsing the malformed feedback and is still usable. */
		fst_xcheck(switch_rtp_ready(rtp_session) == SWITCH_TRUE,
				   "RTP session stays ready after an oversized RTCP NACK");

		if (logger_bound) switch_log_unbind_logger(nack_count_logger);
		if (sock_rtp) switch_socket_close(sock_rtp);
		/* rtp_test_end_call() -> switch_media_handle_destroy() destroys the RTP session. */
		if (session) rtp_test_end_call(&session);
	}
	FST_TEST_END()

	/*
	 * Media-timeout test. With media_timeout configured (TEST_MEDIA_TIMEOUT_MS), the RTP stack
	 * must hang up the channel with SWITCH_CAUSE_MEDIA_TIMEOUT once inbound media stops -- but not
	 * while media is still arriving. The test asserts both directions and the timing:
	 *   - while packets from the capture are replayed at their original pace for well over the
	 *     timeout, the channel stays up (no false timeout);
	 *   - once the packets stop, the channel is torn down with MEDIA_TIMEOUT;
	 *   - the teardown does not happen before roughly the configured timeout has elapsed.
	 */
	FST_TEST_BEGIN(test_rtp_media_timeout)
	{
		switch_core_session_t *session = NULL;
		switch_channel_t *channel = NULL;
		switch_status_t status;
		char rpacket[SWITCH_RECOMMENDED_BUFFER_SIZE];
		switch_payload_t pt = { 0 };
		switch_frame_flag_t frameflags = { 0 };
		pcap_t *pcap;
		const unsigned char *packet;
		char errbuf[PCAP_ERRBUF_SIZE];
		struct timeval prev_ts = { 0 };
		switch_socket_t *sock_rtp = NULL;
		switch_sockaddr_t *sock_addr = NULL;
		const char *str_err;
		switch_size_t rtp_len;
		switch_time_t feed_until, last_media_at, detected_at = 0, deadline;
		int fed = 0;

		status = rtp_test_start_call(&session);
		fst_requires(status == SWITCH_STATUS_SUCCESS);
		fst_requires(session);

		channel = switch_core_session_get_channel(session);
		fst_requires(channel);

		pcap = pcap_open_offline_with_tstamp_precision("pcap/milliwatt.pcmu.rtp.pcap", PCAP_TSTAMP_PRECISION_MICRO, errbuf);
		fst_requires(pcap);

		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_ENABLE_RTCP);

		rtp_session = switch_core_media_get_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO);
		fst_requires(rtp_session);

		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_PAUSE);

		fst_requires(switch_socket_create(&sock_rtp, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS);

		switch_sockaddr_new(&sock_addr, rx_host, audio_rx_port, switch_core_session_get_pool(session));
		fst_requires(sock_addr);

		switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(sock_addr), 0, SWITCH_FALSE, &str_err);
		switch_rtp_reset(rtp_session);

		/* Phase 1: replay media for well over the timeout. The channel must stay up throughout --
		   continuously arriving media must not be mistaken for a timeout. */
		feed_until = switch_micro_time_now() + (TEST_MEDIA_TIMEOUT_MS + 700) * 1000;
		last_media_at = switch_micro_time_now();

		while (switch_micro_time_now() < feed_until && (packet = pcap_next_rtp(pcap, &prev_ts, &rtp_len))) {
			uint32_t rcvd_datalen = sizeof(rpacket);
			switch_size_t tmp_len = rtp_len;

			fst_requires(switch_socket_sendto(sock_rtp, sock_addr, MSG_CONFIRM, (const char *)packet, &tmp_len) == SWITCH_STATUS_SUCCESS);

			status = switch_rtp_read(rtp_session, (void *)rpacket, &rcvd_datalen, &pt, &frameflags, io_flags);
			if (status == SWITCH_STATUS_SUCCESS && pt != SWITCH_RTP_CNG_PAYLOAD) {
				last_media_at = switch_micro_time_now();
			}
			fed++;
		}

		/* We replayed media for longer than the timeout; the channel must not have been torn down. */
		fst_requires(fed > 0);
		fst_xcheck(switch_channel_get_cause(channel) != SWITCH_CAUSE_MEDIA_TIMEOUT,
				   "channel is not hung up for media timeout while media is still flowing");

		/* Phase 2: stop sending. Within a bounded window the stack must raise MEDIA_TIMEOUT. */
		deadline = switch_micro_time_now() + 5 * 1000 * 1000;
		while (switch_micro_time_now() < deadline) {
			uint32_t rcvd_datalen = sizeof(rpacket);

			switch_rtp_read(rtp_session, (void *)rpacket, &rcvd_datalen, &pt, &frameflags, io_flags);

			if (switch_channel_get_cause(channel) == SWITCH_CAUSE_MEDIA_TIMEOUT) {
				detected_at = switch_micro_time_now();
				break;
			}
			switch_yield(20 * 1000);
		}

		fst_xcheck(detected_at != 0, "channel is hung up with MEDIA_TIMEOUT after media stops");

		/* The timeout must honor the configured duration rather than firing immediately. Allow a
		   300ms slack below the configured value for scheduling jitter. */
		if (detected_at != 0) {
			switch_time_t silent_us = detected_at - last_media_at;
			fst_xcheck(silent_us >= (switch_time_t)(TEST_MEDIA_TIMEOUT_MS - 300) * 1000,
					   "MEDIA_TIMEOUT does not fire before roughly the configured timeout elapses");
		}

		switch_rtp_destroy(&rtp_session);

		rtp_test_end_call(&session);

		switch_socket_close(sock_rtp);

		pcap_close(pcap);
	}
	FST_TEST_END()
}
FST_SUITE_END()
}
FST_CORE_END()

