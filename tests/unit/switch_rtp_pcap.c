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

static int got_media_timeout = 0;

//#define USE_RTCP_PCAP 

#define NTP_TIME_OFFSET 2208988800UL

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

/* switch_rtp.c - calc_local_lsr_now()  */
#if 0
static inline uint32_t test_calc_local_lsr_now(switch_time_t now, uint32_t past /*milliseconds*/) 
{
//	switch_time_t now;
	uint32_t ntp_sec, ntp_usec, lsr_now, sec;
//	now = switch_micro_time_now() - (past * 1000);
	now = now - (past * 1000);
	sec = (uint32_t)(now/1000000);        /* convert to seconds     */
	ntp_sec = sec+NTP_TIME_OFFSET;  /* convert to NTP seconds */
	ntp_usec = (uint32_t)(now - ((switch_time_t) sec*1000000)); /* remove seconds to keep only the microseconds */
	 
	lsr_now = (uint32_t)(ntp_usec*0.065536) | (ntp_sec&0x0000ffff)<<16; /* 0.065536 is used for convertion from useconds to fraction of     65536 (x65536/1000000) */
	return lsr_now;
}

static void test_prepare_rtcp(void *rtcp_packet, float est_last, uint32_t rtt, uint8_t loss) 
{
	/* taken from switch_rtp.c, rtcp_generate_sender_info() */
	/* === */
	char *rtcp_sr_trigger = rtcp_packet;
	switch_time_t now;
	uint32_t sec, ntp_sec, ntp_usec;
	uint32_t ntp_msw;
	uint32_t ntp_lsw;
	uint32_t *ptr_msw;
	uint32_t *ptr_lsw;
	uint32_t lsr;
	uint32_t *ptr_lsr;
	uint32_t dlsr = 0;
	uint32_t *ptr_dlsr;
	uint8_t *ptr_loss;

	now = switch_micro_time_now();
	sec = (uint32_t)(now/1000000);        /* convert to seconds     */
	ntp_sec = sec+NTP_TIME_OFFSET;  /* convert to NTP seconds */
	ntp_msw = htonl(ntp_sec);   /* store result in "most significant word" */
	ntp_usec = (uint32_t)(now - (sec*1000000)); /* remove seconds to keep only the microseconds */
	ntp_lsw = htonl((u_long)(ntp_usec*(double)(((uint64_t)1)<<32)*1.0e-6)); 

	/* === */

	/*patch the RTCP payload to set the RTT we want */

	ptr_msw = (uint32_t *)rtcp_sr_trigger + 2;
	*ptr_msw = ntp_msw;
	
	ptr_lsw = (uint32_t *)rtcp_sr_trigger + 3;
	*ptr_lsw = ntp_lsw;

	lsr = test_calc_local_lsr_now(now, est_last * 1000 + rtt /*ms*/);

	ptr_lsr = (uint32_t *)rtcp_sr_trigger + 11;
	*ptr_lsr  = htonl(lsr);

	ptr_dlsr = (uint32_t *)rtcp_sr_trigger + 12;
	*ptr_dlsr  = htonl(dlsr);

	ptr_loss = (uint8_t *)rtcp_sr_trigger + 32;
	*ptr_loss  = loss;
}
#endif

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
	   
	match = switch_core_media_negotiate_sdp(session, r_sdp, &p, SDP_TYPE_REQUEST);
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
	
	return SWITCH_STATUS_SUCCESS;
}

static void rtp_test_init_frame(switch_frame_t **pwrite_frame, switch_core_session_t **psession)
{
	const unsigned char hdr_packet[]="\x80\x00\xcd\x15\xfd\x86\x00\x00\x61\x5a\xe1\x37";

	switch_frame_alloc(pwrite_frame, SWITCH_RECOMMENDED_BUFFER_SIZE);
	(*pwrite_frame)->codec = switch_core_session_get_write_codec(*psession);

	(*pwrite_frame)->datalen = SWITCH_RTP_HEADER_LEN; /*init with dummy RTP header*/
	memcpy((*pwrite_frame)->data, &hdr_packet, SWITCH_RTP_HEADER_LEN);
}

static void show_event(switch_event_t *event) {
	char *str;
	/*print the event*/
	switch_event_serialize_json(event, &str);
	if (str) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s\n", str);
		switch_safe_free(str);
	}
}

static void event_handler(switch_event_t *event) 
{
	const char *new_ev = switch_event_get_header(event, "Event-Name");

	if (new_ev && !strcmp(new_ev, "CHANNEL_HANGUP")) {
		if (!strcmp(switch_event_get_header(event, "Hangup-Cause"), "MEDIA_TIMEOUT")) {
			got_media_timeout = 1;
		}
	}

	show_event(event);
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
#if 0
	FST_TEST_BEGIN(test_rtp_stall_with_rtcp_muxed_with_timer)
	{
		switch_core_session_t *session = NULL;
		switch_status_t status;
		uint32_t plen = SWITCH_RTP_HEADER_LEN;
		char rpacket[SWITCH_RECOMMENDED_BUFFER_SIZE];
		switch_payload_t pt = { 0 };
		switch_frame_flag_t frameflags = { 0 };
		int x = 0;
		switch_frame_t *write_frame;
		pcap_t *pcap;
		const unsigned char *packet;
		char errbuf[PCAP_ERRBUF_SIZE];
		struct pcap_pkthdr pcap_header;
		char rtcp_sr_trigger[] = "\x81\xc8\x00\x0c\x78\x9d\xac\x45\xe2\x67\xa5\x74\x30\x60\x56\x81\x00\x19"
			"\xaa\x00\x00\x00\x06\xd7\x00\x01\x2c\x03\x5e\xbd\x2f\x0b\x00"
			"\x00\x00\x00\x00\x00\x57\xc4\x00\x00\x00\x39\xa5\x73\xfe\x90\x00\x00\x2c\x87"
			"\x81\xca\x00\x0c\x78\x9d\xac\x45\x01\x18\x73\x69\x70\x3a\x64\x72\x40\x31\x39\x32\x2e"
			"\x31\x36\x38\x2e\x30\x2e\x31\x33\x3a\x37\x30\x36\x30\x06\x0e\x4c\x69\x6e\x70\x68\x6f"
			"\x6e\x65\x2d\x33\x2e\x36\x2e\x31\x00\x00";
		const struct sniff_ip *ip; /* The IP header */
		int size_ip, jump_over;
		struct timeval prev_ts = { 0 };
		switch_time_t time_nowpacket = 0, time_prevpacket = 0;
		switch_socket_t *sock_rtp = NULL;
		switch_sockaddr_t *sock_addr = NULL;
		const char *str_err;
		switch_size_t rough_add = 0;

		status = rtp_test_start_call(&session);
		fst_requires(status == SWITCH_STATUS_SUCCESS);
		fst_requires(session);

		pcap = pcap_open_offline_with_tstamp_precision("pcap/milliwatt.long.pcmu.rtp.pcap", PCAP_TSTAMP_PRECISION_MICRO, errbuf);
		fst_requires(pcap);

		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_ENABLE_RTCP);

		rtp_session = switch_core_media_get_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO);

		rtp_test_init_frame(&write_frame, &session);

		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_PAUSE);

		if (switch_socket_create(&sock_rtp, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			fst_requires(0); /*exit*/ 
		}

		switch_sockaddr_new(&sock_addr, rx_host, audio_rx_port, switch_core_session_get_pool(session));
		fst_requires(sock_addr);

		switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(sock_addr), 0, SWITCH_FALSE, &str_err);
		switch_rtp_reset(rtp_session);

		while ((packet = pcap_next(pcap, &pcap_header))) {
			/*assume only UDP/RTP packets in the pcap*/
			uint32_t rcvd_datalen = pcap_header.caplen;
			size_t len;
			switch_size_t tmp_len;

			int diff_us = (pcap_header.ts.tv_sec-prev_ts.tv_sec)*1000000+(pcap_header.ts.tv_usec-prev_ts.tv_usec);

			if (diff_us > 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SENT pkt diff: %d us\n", diff_us);
				usleep(diff_us);
			}

			prev_ts = pcap_header.ts;

			len = pcap_header.caplen;

			if (len <= 42) {
				continue;
			} 

			ip = (struct sniff_ip*)(packet + 14);
			size_ip = IP_HL(ip) * 4;

			jump_over = 14 /*SIZE_ETHERNET*/ + size_ip /*IP HDR size*/ + 8 /* UDP HDR SIZE */; /* jump 42 bytes over network layers/headers */
			packet += jump_over;
			x++;
			
			if (!(x%10)) { /* send a RTCP SR packet every 10th RTP packet */
				int add_rtt = 200;
				test_prepare_rtcp(&rtcp_sr_trigger, 2, add_rtt, 0xa0);
				tmp_len = sizeof(rtcp_sr_trigger);
				/*RTCP muxed*/
				if (switch_socket_sendto(sock_rtp, sock_addr, MSG_CONFIRM, (const char*)rtcp_sr_trigger, &tmp_len) != SWITCH_STATUS_SUCCESS) {
					fst_requires(0);
				}

				plen = sizeof(rtcp_sr_trigger);
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Sent RTCP. Packet size = [%u]\n", plen);
				status = switch_rtp_read(rtp_session, (void *)rpacket, &rcvd_datalen, &pt, &frameflags, io_flags);
				if (pt == SWITCH_RTP_CNG_PAYLOAD /*timeout*/) { 
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "read CNG/RTCP, skip\n");
					while (1) {
						status = switch_rtp_read(rtp_session, (void *)&rpacket, &rcvd_datalen, &pt, &frameflags, io_flags);
						if (frameflags || SFF_RTCP) break;
					}
				}
				fst_requires(status == SWITCH_STATUS_SUCCESS);
			}

			if (packet[0] == 0x80 && packet[1] == 0 /*PCMU*/) {
				int16_t *seq = (int16_t *)packet + 1;
				plen = len - jump_over;
				tmp_len = plen;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sent RTP. Packet size = [%u] seq = [%d]\n", plen, htons(*seq));
				if (switch_socket_sendto(sock_rtp, sock_addr, MSG_CONFIRM, (const char*)packet, &tmp_len) != SWITCH_STATUS_SUCCESS) {
					fst_requires(0);
				}
			}

			status = switch_rtp_read(rtp_session, (void *)&rpacket, &rcvd_datalen, &pt, &frameflags, io_flags);
			if (pt == SWITCH_RTP_CNG_PAYLOAD /*timeout*/) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "read CNG, skip\n");
				continue;
			}
			time_prevpacket = time_nowpacket;
			time_nowpacket = switch_time_now();
			if (time_prevpacket) { // skip init.
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RECV pkt diff: %ld us\n", time_nowpacket - time_prevpacket);

				fst_requires((time_nowpacket - time_prevpacket) < 80000);
				rough_add += time_nowpacket - time_prevpacket; /* just add to var for visual comparison */
			}
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			if (pt == SWITCH_RTP_CNG_PAYLOAD /*timeout*/) continue;
			fst_requires(rcvd_datalen == plen - SWITCH_RTP_HEADER_LEN);
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RECV total delay: %lu\n", rough_add); /*around 17092408 us*/
		switch_yield(1000 * 1000);

		if (write_frame) switch_frame_free(&write_frame);

		switch_rtp_destroy(&rtp_session);

		rtp_test_end_call(&session);

		switch_socket_close(sock_rtp);

		pcap_close(pcap);

		switch_yield(1000 * 1000);
	}
	FST_TEST_END()
#endif

	FST_TEST_BEGIN(test_rtp_media_timeout)
	{
		switch_core_session_t *session = NULL;
		switch_status_t status;
		uint32_t plen = SWITCH_RTP_HEADER_LEN;
		char rpacket[SWITCH_RECOMMENDED_BUFFER_SIZE];
		switch_payload_t pt = { 0 };
		switch_frame_flag_t frameflags = { 0 };
		int x = 0;
		switch_frame_t *write_frame;
		pcap_t *pcap;
		const unsigned char *packet;
		char errbuf[PCAP_ERRBUF_SIZE];
		struct pcap_pkthdr pcap_header;
		const struct sniff_ip *ip; /* The IP header */
		int size_ip, jump_over;
		struct timeval prev_ts = { 0 };
		switch_socket_t *sock_rtp = NULL;
		switch_sockaddr_t *sock_addr = NULL;
		const char *str_err;

		status = rtp_test_start_call(&session);
		fst_requires(status == SWITCH_STATUS_SUCCESS);
		fst_requires(session);

		switch_event_bind("", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL);

		pcap = pcap_open_offline_with_tstamp_precision("pcap/milliwatt.pcmu.rtp.pcap", PCAP_TSTAMP_PRECISION_MICRO, errbuf);
		fst_requires(pcap);

		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_ENABLE_RTCP);

		rtp_session = switch_core_media_get_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO);
		fst_requires(rtp_session);

		rtp_test_init_frame(&write_frame, &session);

		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_PAUSE);

		if (switch_socket_create(&sock_rtp, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			fst_requires(0); /*exit*/ 
		}

		switch_sockaddr_new(&sock_addr, rx_host, audio_rx_port, switch_core_session_get_pool(session));
		fst_requires(sock_addr);

		switch_rtp_set_remote_address(rtp_session, tx_host, switch_sockaddr_get_port(sock_addr), 0, SWITCH_FALSE, &str_err);
		switch_rtp_reset(rtp_session);

		/* send 3 packets then wait and expect RTP timeout */
		while ((packet = pcap_next(pcap, &pcap_header)) && x < 3) {
			/*assume only UDP/RTP packets in the pcap*/
			uint32_t rcvd_datalen = pcap_header.caplen;
			size_t len;
			switch_size_t tmp_len;

			int diff_us = (pcap_header.ts.tv_sec-prev_ts.tv_sec)*1000000+(pcap_header.ts.tv_usec-prev_ts.tv_usec);
			if (diff_us > 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "SENT pkt diff: %d us\n", diff_us);
				usleep(diff_us);
			}

			x++;

			prev_ts = pcap_header.ts;

			len = pcap_header.caplen;

			if (len <= 42) {
				continue;
			} 

			ip = (struct sniff_ip*)(packet + 14);
			size_ip = IP_HL(ip) * 4;

			jump_over = 14 /*SIZE_ETHERNET*/ + size_ip /*IP HDR size*/ + 8 /* UDP HDR SIZE */; /* jump 42 bytes over network layers/headers */
			packet += jump_over;
			
			if (packet[0] == 0x80 && packet[1] == 0 /*PCMU*/) {
				int16_t *seq = (int16_t *)packet + 1;
				plen = len - jump_over;
				tmp_len = plen;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sent RTP. Packet size = [%u] seq = [%d]\n", plen, htons(*seq));
				if (switch_socket_sendto(sock_rtp, sock_addr, MSG_CONFIRM, (const char*)packet, &tmp_len) != SWITCH_STATUS_SUCCESS) {
					fst_requires(0);
				}
			}

			status = switch_rtp_read(rtp_session, (void *)&rpacket, &rcvd_datalen, &pt, &frameflags, io_flags);
			if (pt == SWITCH_RTP_CNG_PAYLOAD /*timeout*/) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "read CNG, skip\n");
				continue;
			}

			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(rcvd_datalen == plen - SWITCH_RTP_HEADER_LEN);
		}

		x = 150; /* 3 seconds max */
		while (x || !got_media_timeout) {
			uint32_t rcvd_datalen; 
			status = switch_rtp_read(rtp_session, (void *)&rpacket, &rcvd_datalen, &pt, &frameflags, io_flags);
			if (pt == SWITCH_RTP_CNG_PAYLOAD /*timeout*/) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "read CNG, skip\n");
			}
			switch_yield(20 * 1000);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			x--;
		}

		if (write_frame) switch_frame_free(&write_frame);

		switch_rtp_destroy(&rtp_session);

		rtp_test_end_call(&session);

		switch_socket_close(sock_rtp);

		pcap_close(pcap);

		fst_check(got_media_timeout);
	}
	FST_TEST_END()
}
FST_SUITE_END()
}
FST_CORE_END()

