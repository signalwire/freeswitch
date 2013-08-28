/*
 * Copyright (c) 2010, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "switch.h"

#include <sng_tc/sngtc_node.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_sangoma_codec_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sangoma_codec_shutdown);
SWITCH_MODULE_DEFINITION(mod_sangoma_codec, mod_sangoma_codec_load, mod_sangoma_codec_shutdown, NULL);

#define SANGOMA_SESS_HASH_KEY_FORMAT "sngtc%lu"

/* it seemed we need higher PTIME than the calling parties, so we assume nobody will use higher ptime than 40 */
#define SANGOMA_DEFAULT_SAMPLING_RATE 80
#define SANGOMA_TRANSCODE_CONFIG "sangoma_codec.conf" 

/* \brief vocallos configuration */
static sngtc_init_cfg_t g_init_cfg;

/* configured RTP IP */
static int g_rtpip = 0;

static char g_soap_url[255] = "";

/* \brief protect vocallo session creation and destroy */
static switch_mutex_t *g_sessions_lock = NULL;

/* \brief next unique session id (protected by g_sessions_lock) */
unsigned long long g_next_session_id = 0;

/* hash of sessions (I think a linked list suits better here, but FS does not have the data type) */
static switch_hash_t *g_sessions_hash = NULL;

typedef struct vocallo_codec_s {
	int codec_id; /* vocallo codec ID */
	int iana; /* IANA code to register in FS */
	const char *iana_name; /* IANA name to register in FS */
	const char *fs_name;
	int maxms; /* max supported ms (WARNING: codec impl from 10ms up to this value will be registered if autoinit=1) */
	int bps; /* bits per second */

	/* following values must be set assuming frames of 10ms */
	int mpf; /* microseconds per frame */
	int spf; /* samples per frame */
	int bpfd; /* bytes per frame decompressed */
	int bpfc; /* bytes per frame compressed */

	int sampling_rate; /* declared sampling rate */
	int actual_sampling_rate; /* true sampling rate */
	int autoinit; /* initialize on start loop or manually */
} vocallo_codec_t;

vocallo_codec_t g_codec_map[] =
{
	/* auto-init codecs */
	{ SNGTC_CODEC_PCMU,      IANA_PCMU_A_8000_1,  "PCMU",    "Sangoma PCMU",      40, 64000,  10000, 80,  160, 80,  8000,  8000,  1 },
	{ SNGTC_CODEC_PCMA,      IANA_PCMA_A_8000_1,  "PCMA",    "Sangoma PCMA",      40, 64000,  10000, 80,  160, 80,  8000,  8000,  1 },
	{ SNGTC_CODEC_L16_1,     IANA_L16_A_8000_1,   "L16",     "Sangoma L16",       40, 120000, 10000, 80,  160, 160, 8000,  8000,  0 },
	{ SNGTC_CODEC_L16_2,     IANA_L16_A_16000_1,  "L16",     "Sangoma L16 2",     40, 320000, 10000, 160, 320, 320, 16000, 16000, 0 },
	{ SNGTC_CODEC_G729AB,    IANA_G729_AB_8000_1, "G729",    "Sangoma G729",      50, 8000,   10000, 80,  160, 10,  8000,  8000,  1 },
	{ SNGTC_CODEC_G726_32,   IANA_G726_32_8000_1, "G726-32", "Sangoma G.726 32k", 40, 32000,  10000, 80,  160, 40,  8000,  8000,  1 },
	{ SNGTC_CODEC_G722,      IANA_G722_A_8000_1,  "G722",    "Sangoma G722",      20, 64000,  10000, 80,  160, 80,  8000,  8000, 1  },

	/* manually initialized */
	{ SNGTC_CODEC_GSM_FR,    IANA_GSM_A_8000_1,    "GSM",   "Sangoma GSM",    20, 13200, 20000, 160, 320, 33, 8000,  8000,  0 },
	{ SNGTC_CODEC_G723_1_63, IANA_G723_A_8000_1,   "G723",  "Sangoma G723",   90, 6300,  30000, 240, 480, 24, 8000,  8000,  0 },
	{ SNGTC_CODEC_AMR_1220,  IANA_AMR_A_8000_1,    "AMR",   "Sangoma AMR",    20, 12200, 20000, 160, 320, 0,  8000,  8000,  0 },
	{ SNGTC_CODEC_SIREN7_24, IANA_SIREN7,          "G7221", "Sangoma G722.1", 20, 24000, 20000, 320, 640, 60, 16000, 16000, 0 },
	{ SNGTC_CODEC_SIREN7_32, IANA_SIREN7,          "G7221", "Sangoma G722.1", 20, 32000, 20000, 320, 640, 80, 16000, 16000, 0 },
	{ SNGTC_CODEC_ILBC_133,  IANA_ILBC_133_8000_1, "iLBC",  "Sangoma iLBC",   30, 13330, 30000, 240, 480, 50, 8000,  8000,  0 },
	{ SNGTC_CODEC_ILBC_152,  IANA_ILBC_152_8000_1, "iLBC",  "Sangoma iLBC",   20, 15200, 20000, 160, 320, 38, 8000,  8000,  0 },
	{ -1,                    -1,                   NULL,    NULL,             -1, -1,    -1,    -1,  -1,  -1, -1,    -1,    0 },
};

/* RFC3389 RTP Payload for Comfort Noise */
#define IANACODE_CN 13

/* default codec list to load, users may override, special codec 'all' registers everything available unless listed in noregister */
static char g_codec_register_list[1024] = "G729";

/* default codec list to NOT load, users may override */
static char g_codec_noregister_list[1024] = "";

#define SANGOMA_RTP_QUEUE_SIZE 4
struct sangoma_rtp_payload {
	uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	int32_t datalen;
};

struct codec_data {
	/* sngtc request and reply */
	sngtc_codec_request_t request;
	sngtc_codec_reply_t reply;

	/* rtp streams */
	void *txrtp;
	void *rxrtp;

	/* packet counters */
	unsigned long tx;
	unsigned long rx;
	unsigned long ticks; 

	/* Lost packets */
  	long lastrxseqno;
	unsigned long rxlost;

	/* discarded silence packets */
	unsigned long rxdiscarded;

	/* avg Rx time */
	switch_time_t avgrxus;
	switch_time_t last_rx_time;
	switch_time_t last_func_call_time;

	/* RTP queue. The bigger the queue, the bigger the possible delay */
	struct sangoma_rtp_payload rtp_queue[SANGOMA_RTP_QUEUE_SIZE];
	uint8_t queue_windex;
	uint8_t queue_rindex;
	uint8_t queue_size;
	uint8_t queue_max_ever;
	unsigned debug_timing:1;
};

struct sangoma_transcoding_session {
	/* unique session id */
	unsigned long sessid;
	char hashkey[25];

	/* encoder and decoder */
	struct codec_data encoder;
	struct codec_data decoder;

	/* codec implementation */
	const switch_codec_implementation_t *impl;

	/* memory pool */
	switch_memory_pool_t *pool;
};

static int codec_id_to_iana(int codec_id)
{
	int i;
	for (i = 0; g_codec_map[i].codec_id != -1; i++) {
		if (codec_id == g_codec_map[i].codec_id) {
			return g_codec_map[i].iana;
		}
	}
	return -1;
}

static vocallo_codec_t *get_codec_from_iana(int iana, int bitrate)
{
	int i;
	for (i = 0; g_codec_map[i].codec_id != -1; i++) {
		if (iana == g_codec_map[i].iana && !bitrate) {
			return &g_codec_map[i];
		}
		if (iana == g_codec_map[i].iana && bitrate == g_codec_map[i].bps) {
			return &g_codec_map[i];
		}
	}
	return NULL;
}

static vocallo_codec_t *get_codec_from_id(int id)
{
	int i;
	for (i = 0; g_codec_map[i].codec_id != -1; i++) {
		if (id == g_codec_map[i].codec_id) {
			return &g_codec_map[i];
		}
	}
	return NULL;
}

static int sangoma_create_rtp_port(void *usr_priv, uint32_t host_ip, uint32_t *p_rtp_port, void **rtp_fd)
{
	struct in_addr local_ip_addr = { 0 };
	char local_ip[255];
	switch_port_t rtp_port;

	local_ip_addr.s_addr = htonl(host_ip);
	
	switch_inet_ntop(AF_INET, &local_ip_addr, local_ip, sizeof(local_ip));

	/* request a port */
	if (!(rtp_port = switch_rtp_request_port(local_ip))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate RTP port for IP %s\n", local_ip);
		return -1;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "New allocated port %d for IP %s/%d.%d.%d.%d\n", rtp_port, local_ip,
			SNGTC_NIPV4(host_ip));
	*p_rtp_port = rtp_port;
	*rtp_fd = NULL;
	return 0;
}

static int sangoma_release_rtp_port(void *usr_priv, uint32_t host_ip, uint32_t p_rtp_port, void *rtp_fd)
{
	struct in_addr local_ip_addr = { 0 };
	char local_ip[255];
	switch_port_t rtp_port = p_rtp_port;

	local_ip_addr.s_addr = htonl(host_ip);
	
	switch_inet_ntop(AF_INET, &local_ip_addr, local_ip, sizeof(local_ip));

	/* release the port */
	switch_rtp_release_port(local_ip, rtp_port);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Released port %d for IP %s/%d.%d.%d.%d\n", rtp_port, local_ip,
			SNGTC_NIPV4(host_ip));
	return 0;
}

static int sangoma_create_rtp(void *usr_priv, sngtc_codec_request_leg_t *codec_req_leg, sngtc_codec_reply_leg_t* codec_reply_leg, void **rtp_fd)
{
	switch_status_t status;
	switch_memory_pool_t *sesspool = NULL;
	switch_rtp_t *rtp_session = NULL;
	char codec_ip[255];
	switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
	int iana = 0;
	const char *err = NULL;
	struct in_addr local_ip_addr = { 0 };
	char local_ip[255];
	switch_port_t rtp_port;
	struct sangoma_transcoding_session *sess = usr_priv;

	rtp_port = codec_req_leg->host_udp_port;
	*rtp_fd = NULL;

	/*
	 * We *MUST* use a new pool
	 * Do not use the session pool since the session may go away while the RTP socket should linger around 
	 * until sangoma_transcode decides to kill it (possibly because the same RTP session is used for a different call) 
	 * also do not use the module pool otherwise memory would keep growing because switch_rtp_destroy does not
	 * free the memory used (is assumed it'll be freed when the pool is destroyed)
	 */
	status = switch_core_new_memory_pool(&sesspool);
	if (status != SWITCH_STATUS_SUCCESS) {
		return -1;
	}
	
	local_ip_addr.s_addr = htonl(codec_req_leg->host_ip);
	switch_inet_ntop(AF_INET, &local_ip_addr, local_ip, sizeof(local_ip));
	sngtc_codec_ipv4_hex_to_str(codec_reply_leg->codec_ip, codec_ip);

	iana = codec_id_to_iana(codec_req_leg->codec_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Creating RTP session for host (%s/%d)  vocallo(%s/%d) Iana=%d CodecId=%d ms=%d idx=%lu\n",
					  local_ip, rtp_port, codec_ip, codec_reply_leg->codec_udp_port, iana, codec_req_leg->codec_id, 
					  codec_req_leg->ms*1000, sess->sessid);

	/* create the RTP socket */
	rtp_session = switch_rtp_new(local_ip, rtp_port, 
			codec_ip, codec_reply_leg->codec_udp_port, 
			iana,
			sess->impl->samples_per_packet,
			codec_req_leg->ms * 1000, /* microseconds per packet */
			flags, NULL, &err, sesspool);

	if (!rtp_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create switch rtp session: %s\n", err);
		switch_core_destroy_memory_pool(&sesspool);
		return -1;
	}
	switch_rtp_set_private(rtp_session, sesspool);
	*rtp_fd = rtp_session;

	return 0;
}

static int sangoma_destroy_rtp(void *usr_priv, void *fd)
{
	switch_memory_pool_t *sesspool;
	switch_rtp_t *rtp = fd;
	if (!rtp) {
		return 0;
	}
	sesspool = switch_rtp_get_private(rtp);
	switch_rtp_destroy(&rtp);
	switch_core_destroy_memory_pool(&sesspool);
	return 0;
}

static switch_status_t switch_sangoma_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct sangoma_transcoding_session *sess = NULL;
	vocallo_codec_t *vcodec;
	
	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sangoma init called (encoding = %d, decoding = %d, iana = %d)\n", encoding ? 1 : 0, decoding ? 1 : 0, codec->implementation->ianacode);

	if (!(encoding || decoding)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!(sess = switch_core_alloc(codec->memory_pool, sizeof(*sess)))) {
		return SWITCH_STATUS_FALSE;
	}
	memset(sess, 0, sizeof(*sess));

	sess->encoder.lastrxseqno = -1;
	sess->decoder.lastrxseqno = -1;

	sess->pool = codec->memory_pool;
	sess->impl = codec->implementation;

	vcodec = get_codec_from_iana(codec->implementation->ianacode, codec->implementation->bits_per_second);
	
	switch_mutex_lock(g_sessions_lock);

	if (encoding) {
		sess->encoder.request.usr_priv = sess;
		sess->encoder.request.a.host_ip = g_rtpip;
		sess->encoder.request.a.codec_id = vcodec->actual_sampling_rate == 16000 
			                         ? SNGTC_CODEC_L16_2 : SNGTC_CODEC_L16_1;
		sess->encoder.request.a.ms = codec->implementation->microseconds_per_packet/1000;

		sess->encoder.request.b.host_ip = g_rtpip;
		sess->encoder.request.b.codec_id = vcodec->codec_id;
		sess->encoder.request.b.ms = codec->implementation->microseconds_per_packet/1000;
	}

	if (decoding) {
		sess->decoder.request.usr_priv = sess;
		sess->decoder.request.a.host_ip = g_rtpip;
		sess->decoder.request.a.codec_id = vcodec->codec_id;
		sess->decoder.request.a.ms = codec->implementation->microseconds_per_packet/1000;

		sess->decoder.request.b.host_ip = g_rtpip;
		sess->decoder.request.b.codec_id = vcodec->actual_sampling_rate == 16000 
			                         ? SNGTC_CODEC_L16_2 : SNGTC_CODEC_L16_1;
		sess->decoder.request.b.ms = codec->implementation->microseconds_per_packet/1000;

	}

	sess->sessid = g_next_session_id++;
	switch_snprintf(sess->hashkey, sizeof(sess->hashkey), SANGOMA_SESS_HASH_KEY_FORMAT, sess->sessid);
	switch_core_hash_insert(g_sessions_hash, sess->hashkey, sess);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sangoma init done for codec %s/%s, iana = %d\n", codec->implementation->iananame, vcodec->fs_name, codec->implementation->ianacode);
	switch_mutex_unlock(g_sessions_lock);

	codec->private_info = sess;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_sangoma_init_ilbc(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int mode = codec->implementation->microseconds_per_packet / 1000;
	if (codec->fmtp_in) {
		int x, argc;
		char *argv[10];
		argc = switch_separate_string(codec->fmtp_in, ';', argv, (sizeof(argv) / sizeof(argv[0])));
		for (x = 0; x < argc; x++) {
			char *data = argv[x];
			char *arg;
			switch_assert(data);
			while (*data == ' ') {
				data++;
			}
			if ((arg = strchr(data, '='))) {
				*arg++ = '\0';
				if (!strcasecmp(data, "mode")) {
					mode = atoi(arg);
				}
			}
		}
	}
	codec->fmtp_out = switch_core_sprintf(codec->memory_pool, "mode=%d", mode);
	return switch_sangoma_init(codec, flags, codec_settings);
}

static switch_status_t switch_sangoma_init_siren7(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	int bit_rate = codec->implementation->bits_per_second;
	codec->fmtp_out = switch_core_sprintf(codec->memory_pool, "bitrate=%d", bit_rate);
	return switch_sangoma_init(codec, flags, codec_settings);
}

static void flush_rtp(switch_rtp_t *rtp)
{
	switch_status_t sres;
	switch_frame_t read_frame;
	int flushed = 0;
	int sanity = 1000;
	while (sanity--) {
		sres = switch_rtp_zerocopy_read_frame(rtp, &read_frame, SWITCH_IO_FLAG_NOBLOCK);
		if (sres == SWITCH_STATUS_GENERR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read on Sangoma encoder RTP session while flushing: %d\n", sres);
			return;
		}
		if (!read_frame.datalen) {
			break;
		}
		flushed++;
	}
	if (!sanity) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Insanely big UDP queue!\n");
	}
}

#define SAFE_INDEX_INC(array, index) \
		(index)++; \
		if ((index) == switch_arraylen((array))) { \
			(index) = 0; \
		}

static switch_status_t switch_sangoma_encode(switch_codec_t *codec, switch_codec_t *other_codec,	/* codec that was used by the other side */
										  void *decoded_data,	/* decoded data that we must encode */
										  uint32_t decoded_data_len /* decoded data length */ ,
										  uint32_t decoded_rate /* rate of the decoded data */ ,
										  void *encoded_data,	/* here we will store the encoded data */
										  uint32_t *encoded_data_len,	/* here we will set the length of the encoded data */
										  uint32_t *encoded_rate /* here we will set the rate of the encoded data */ ,
										  unsigned int *flag /* frame flag, see switch_frame_flag_enum_t */ )
{
	/* FS core checks the actual samples per second and microseconds per packet to determine the buffer size in the worst case scenario, no need to check
	 * whether the buffer passed in by the core (encoded_data) will be big enough */
	switch_frame_t linear_frame;
	switch_frame_t encoded_frame;
	switch_status_t sres = SWITCH_STATUS_FALSE;
	uint16_t decoded_byteswapped_data[SWITCH_RECOMMENDED_BUFFER_SIZE];
	uint16_t *decoded_data_linear = decoded_data;
	switch_time_t now_time = 0, difftime = 0;
	switch_time_t func_start_time = 0, func_end_time = 0;
	int i = 0;
	int res = 0;
	int linear_payload = codec->implementation->actual_samples_per_second == 8000 ? IANA_L16_A_8000_1 : IANA_L16_A_16000_1;
	struct sangoma_transcoding_session *sess = codec->private_info;

	if (sess->encoder.debug_timing) {
		func_start_time =  switch_micro_time_now();
	}

	sess->encoder.ticks++;

	/* start assuming we will not encode anything */
	*encoded_data_len = 0;

	/* initialize on first use */
	if (!sess->encoder.txrtp) {
		int err = 0;
		switch_mutex_lock(g_sessions_lock);
		err = sngtc_create_transcoding_session(&sess->encoder.request, &sess->encoder.reply, 0);
		if (err) {
			memset(&sess->encoder, 0, sizeof(sess->encoder));
			switch_mutex_unlock(g_sessions_lock);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create Sangoma encoding session.\n");
			return SWITCH_STATUS_FALSE;
		}
		sess->encoder.txrtp = sess->encoder.reply.tx_fd;
		sess->encoder.rxrtp = sess->encoder.reply.rx_fd;
		switch_mutex_unlock(g_sessions_lock);
		flush_rtp(sess->encoder.rxrtp);
	}

	if (sess->encoder.debug_timing && sess->encoder.last_func_call_time) {
		difftime = func_start_time - sess->encoder.last_func_call_time;
		if (difftime > 25000 || difftime < 15000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%ldus since last read on encoding session %lu\n", (long)difftime, sess->sessid);
		}
	}

	/* do the writing */
	memset(&linear_frame, 0, sizeof(linear_frame));	
	linear_frame.source = __FUNCTION__;
	linear_frame.data = decoded_byteswapped_data;
	linear_frame.datalen = decoded_data_len;
	linear_frame.payload = linear_payload;

	/* copy and byte-swap */
	for (i = 0; i < decoded_data_len/2; i++) {
		decoded_byteswapped_data[i] = (decoded_data_linear[i] << 8) | (decoded_data_linear[i] >> 8);
	}


	res = switch_rtp_write_frame(sess->encoder.txrtp, &linear_frame);
	if (-1 == res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to write to Sangoma encoder RTP session.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (res < i) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"Requested to write %d bytes to Sangoma encoder RTP session, but wrote %d bytes.\n", i, res);
		return SWITCH_STATUS_FALSE;
	}
	sess->encoder.tx++;

	/* do the reading */
	for ( ; ; ) {
#if 0
		prevread_time = switch_micro_time_now();
#endif
		sres = switch_rtp_zerocopy_read_frame(sess->encoder.rxrtp, &encoded_frame, SWITCH_IO_FLAG_NOBLOCK);
		if (sres == SWITCH_STATUS_GENERR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read on Sangoma encoder RTP session: %d\n", sres);
			return SWITCH_STATUS_FALSE;
		}

#if 0
		afterread_time = switch_micro_time_now();
		difftime = afterread_time - prevread_time;
		if (difftime > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%ldus to read on encoding session %lu.\n", (long)difftime, sess->sessid);
		}
#endif
		if (0 == encoded_frame.datalen) {
			break;
		}

		if (encoded_frame.payload == IANACODE_CN) {
			/* confort noise is treated as silence by us */
			continue;
		}

		if (codec->implementation->encoded_bytes_per_packet && encoded_frame.datalen != codec->implementation->encoded_bytes_per_packet) {
			/* seen when silence suppression is enabled */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ignoring encoded frame of %d bytes intead of %d bytes\n", encoded_frame.datalen, codec->implementation->encoded_bytes_per_packet);
			continue;
		}

		if (encoded_frame.payload != codec->implementation->ianacode) {
			if (sess->encoder.request.b.codec_id == SNGTC_CODEC_ILBC_152 || sess->encoder.request.b.codec_id == SNGTC_CODEC_ILBC_133) {
				/* since we moved to SOAP based communications, the mapping between vocallo IANA and our IANA does not work, 
				 * some codecs checks cannot be completely done, like iLBC */
				if (encoded_frame.payload != IANA_ILBC_152_8000_1 && encoded_frame.payload != IANA_ILBC_133_8000_1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Read unexpected payload %d in Sangoma encoder RTP session, expecting either %d or %d\n",
							encoded_frame.payload, IANA_ILBC_152_8000_1, IANA_ILBC_133_8000_1);
					break;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Read unexpected payload %d in Sangoma encoder RTP session, expecting %d\n",
						encoded_frame.payload, codec->implementation->ianacode);
				break;
			}
		}

		if (sess->encoder.queue_windex == sess->encoder.queue_rindex) {
			if (sess->encoder.rtp_queue[sess->encoder.queue_rindex].datalen) {
				/* if there is something where we want to write, we're dropping it */
				sess->encoder.rxdiscarded++;
#if 0
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Discarding encoded frame of %d bytes from RTP session %lu, windex = %d, rindex = %d\n", 
						sess->encoder.rtp_queue[sess->encoder.queue_rindex].datalen, sess->sessid, sess->encoder.queue_windex, sess->encoder.queue_rindex);
#endif
				SAFE_INDEX_INC(sess->encoder.rtp_queue, sess->encoder.queue_rindex);
				sess->encoder.queue_size--;
			}
		}

		memcpy(sess->encoder.rtp_queue[sess->encoder.queue_windex].data, encoded_frame.data, encoded_frame.datalen);
		sess->encoder.rtp_queue[sess->encoder.queue_windex].datalen = encoded_frame.datalen;
		SAFE_INDEX_INC(sess->encoder.rtp_queue, sess->encoder.queue_windex);

		/* monitor the queue size */
		sess->encoder.queue_size++;
		if (sess->encoder.queue_size > sess->encoder.queue_max_ever) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Encoder Rx queue for RTP session %lu is now %d, windex = %d, rindex = %d\n", sess->sessid, sess->encoder.queue_size, 
					sess->encoder.queue_windex, sess->encoder.queue_rindex);
			sess->encoder.queue_max_ever = sess->encoder.queue_size;
		}
	}

	/* update encoding stats if we have a frame to give */
	if (sess->encoder.rtp_queue[sess->encoder.queue_rindex].datalen) {
		sess->encoder.rx++;
		now_time = switch_micro_time_now();
		if (!sess->encoder.last_rx_time) {
			sess->encoder.last_rx_time = now_time;
		} else {
			difftime = now_time - sess->encoder.last_rx_time;
			sess->encoder.avgrxus = sess->encoder.avgrxus ? ((sess->encoder.avgrxus + difftime)/2) : difftime;
			sess->encoder.last_rx_time  = now_time;
		}

		/* check sequence and bump lost rx packets count if needed */
		if (sess->encoder.lastrxseqno >= 0) {
			if (encoded_frame.seq > (sess->encoder.lastrxseqno + 2) ) {
				sess->encoder.rxlost += encoded_frame.seq - sess->encoder.lastrxseqno - 1;
			}
		}
		sess->encoder.lastrxseqno = encoded_frame.seq;

		/* pop the data from the queue */
		*encoded_data_len = sess->encoder.rtp_queue[sess->encoder.queue_rindex].datalen;
		memcpy(encoded_data, sess->encoder.rtp_queue[sess->encoder.queue_rindex].data, *encoded_data_len);
		sess->encoder.rtp_queue[sess->encoder.queue_rindex].datalen = 0;
		SAFE_INDEX_INC(sess->encoder.rtp_queue, sess->encoder.queue_rindex);
		sess->encoder.queue_size--;
		if (codec->implementation->encoded_bytes_per_packet && *encoded_data_len != codec->implementation->encoded_bytes_per_packet) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Returning odd encoded frame of %d bytes intead of %d bytes\n", *encoded_data_len, codec->implementation->encoded_bytes_per_packet);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No output from sangoma encoder\n");
	}

	if (sess->encoder.debug_timing) {
		func_end_time = switch_micro_time_now();
		difftime = func_end_time - func_start_time;
		if (difftime > 5000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%ldus to execute encoding function in session %lu.\n", (long)difftime, sess->sessid);
		}
		sess->encoder.last_func_call_time = func_end_time;
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_sangoma_decode(switch_codec_t *codec,	/* codec session handle */
										  switch_codec_t *other_codec,	/* what is this? */
										  void *encoded_data,	/* data that we must decode into slinear and put it in decoded_data */
										  uint32_t encoded_data_len,	/* length in bytes of the encoded data */
										  uint32_t encoded_rate,	/* at which rate was the data encoded */
										  void *decoded_data,	/* buffer where we must put the decoded data */
										  uint32_t *decoded_data_len,	/* we must set this value to the size of the decoded data */
										  uint32_t *decoded_rate,	/* rate of the decoded data */
										  unsigned int *flag /* frame flag, see switch_frame_flag_enum_t */ )
{
	/* FS core checks the actual samples per second and microseconds per packet to determine the buffer size in the worst case scenario, no need to check
	 * whether the buffer passed in by the core will be enough */
	switch_frame_t encoded_frame;
	switch_frame_t linear_frame;
	switch_status_t sres = SWITCH_STATUS_FALSE;
	switch_time_t now_time = 0, difftime = 0;
	switch_time_t func_start_time = 0, func_end_time = 0;
	uint16_t *dbuf_linear;
	uint16_t *linear_frame_data;
	uint16_t *rtp_data_linear;
	int res = 0;
	int i = 0;
	int linear_payload = codec->implementation->actual_samples_per_second == 8000 ? IANA_L16_A_8000_1 : IANA_L16_A_16000_1;
	struct sangoma_transcoding_session *sess = codec->private_info;

	if (sess->decoder.debug_timing) {
		func_start_time =  switch_micro_time_now();
	}

	dbuf_linear = decoded_data;
	sess->decoder.ticks++;

	/* start assuming we will not decode anything */
	*decoded_data_len = 0;
	if (*flag & SWITCH_CODEC_FLAG_SILENCE) {
		memset(dbuf_linear, 0, codec->implementation->decoded_bytes_per_packet);
		*decoded_data_len = codec->implementation->decoded_bytes_per_packet;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Returned silence on request\n");
		return SWITCH_STATUS_SUCCESS;
	}

	/* initialize on first use */
	if (!sess->decoder.txrtp) {
		int err = 0;
		switch_mutex_lock(g_sessions_lock);
		err = sngtc_create_transcoding_session(&sess->decoder.request, &sess->decoder.reply, 0);
		if (err) {
			memset(&sess->decoder, 0, sizeof(sess->decoder));
			switch_mutex_unlock(g_sessions_lock);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create Sangoma decoding session.\n");
			return SWITCH_STATUS_FALSE;
		}
		sess->decoder.txrtp = sess->decoder.reply.tx_fd;
		sess->decoder.rxrtp = sess->decoder.reply.rx_fd;
		switch_mutex_unlock(g_sessions_lock);
		flush_rtp(sess->decoder.rxrtp);
	}

	if (sess->decoder.debug_timing && sess->decoder.last_func_call_time) {
		difftime = func_start_time - sess->decoder.last_func_call_time;
		if (difftime > 25000 || difftime < 15000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%ldms since last read on decoding session %lu.\n", (long)difftime, sess->sessid);
		}
	}

	/* do the writing */
	memset(&encoded_frame, 0, sizeof(encoded_frame));
	encoded_frame.source = __FUNCTION__;
	encoded_frame.data = encoded_data;
	encoded_frame.datalen = encoded_data_len;
	encoded_frame.payload = codec->implementation->ianacode;

	res = switch_rtp_write_frame(sess->decoder.txrtp, &encoded_frame);
	if (-1 == res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to write to Sangoma decoder RTP session.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (res < encoded_data_len) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"Requested to write %d bytes to Sangoma decoder RTP session, but wrote %d bytes.\n",
			       	encoded_data_len, res);
		return SWITCH_STATUS_FALSE;
	}
	sess->decoder.tx++;

	/* do the reading */
	for ( ; ; ) {
#if 0
		prevread_time = switch_micro_time_now();
#endif
		sres = switch_rtp_zerocopy_read_frame(sess->decoder.rxrtp, &linear_frame, SWITCH_IO_FLAG_NOBLOCK);
		if (sres == SWITCH_STATUS_GENERR) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read on Sangoma decoder RTP session: %d\n", sres);
			return SWITCH_STATUS_FALSE;
		}

#if 0
		afterread_time = switch_micro_time_now();
		difftime = afterread_time - prevread_time;
		if (difftime > 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%ldus to read on decoding session %lu.\n", (long)difftime, sess->sessid);
		}
#endif
		if (0 == linear_frame.datalen) {
			break;
		}

		if (linear_frame.payload == IANACODE_CN) {
			/* confort noise is treated as silence by us */
			continue;
		}

		if (linear_frame.payload != linear_payload) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Read unexpected payload %d in Sangoma decoder RTP session, expecting %d\n",
					linear_frame.payload, linear_payload);
			break;
		}


		if (sess->decoder.queue_windex == sess->decoder.queue_rindex) {
			if (sess->decoder.rtp_queue[sess->decoder.queue_rindex].datalen) {
				/* if there is something where we want to write, we're dropping it */
				sess->decoder.rxdiscarded++;
#if 0
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Discarding decoded frame of %d bytes from RTP session %lu, windex = %d, rindex = %d\n", 
						sess->decoder.rtp_queue[sess->decoder.queue_rindex].datalen, sess->sessid, sess->decoder.queue_windex, sess->decoder.queue_rindex);
#endif
				SAFE_INDEX_INC(sess->decoder.rtp_queue, sess->decoder.queue_rindex);
				sess->decoder.queue_size--;
			}
		}

		/* byteswap the received data */
		rtp_data_linear = (unsigned short *)sess->decoder.rtp_queue[sess->decoder.queue_windex].data;
		linear_frame_data = linear_frame.data;
		for (i = 0; i < linear_frame.datalen/2; i++) {
			rtp_data_linear[i] = (linear_frame_data[i] << 8) | (linear_frame_data[i] >> 8);
		}
		sess->decoder.rtp_queue[sess->decoder.queue_windex].datalen = linear_frame.datalen;

		SAFE_INDEX_INC(sess->decoder.rtp_queue, sess->decoder.queue_windex);

		/* monitor the queue size */
		sess->decoder.queue_size++;
		if (sess->decoder.queue_size > sess->decoder.queue_max_ever) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Encoder Rx queue for RTP session %lu is now %d, windex = %d, rindex = %d\n", sess->sessid, sess->decoder.queue_size, 
					sess->decoder.queue_windex, sess->decoder.queue_rindex);
			sess->decoder.queue_max_ever = sess->decoder.queue_size;
		}
	}

	if (sess->decoder.rtp_queue[sess->decoder.queue_rindex].datalen) {
		/* update decoding stats */
		sess->decoder.rx++;

		now_time = switch_micro_time_now();
		if (!sess->decoder.last_rx_time) {
			sess->decoder.last_rx_time = now_time;
		} else {
			difftime = now_time - sess->decoder.last_rx_time;
			sess->decoder.avgrxus = sess->decoder.avgrxus ? ((sess->decoder.avgrxus + difftime)/2) : difftime;
			sess->decoder.last_rx_time = now_time;
		}

		/* check sequence and bump lost rx packets count if needed */
		if (sess->decoder.lastrxseqno >= 0) {
			if (linear_frame.seq > (sess->decoder.lastrxseqno + 2) ) {
				sess->decoder.rxlost += linear_frame.seq - sess->decoder.lastrxseqno - 1;
			}
		}
		sess->decoder.lastrxseqno = linear_frame.seq;

		/* pop the data from the queue */
		memcpy(dbuf_linear, sess->decoder.rtp_queue[sess->decoder.queue_rindex].data, sess->decoder.rtp_queue[sess->decoder.queue_rindex].datalen);
		*decoded_data_len = sess->decoder.rtp_queue[sess->decoder.queue_rindex].datalen;
		sess->decoder.rtp_queue[sess->decoder.queue_rindex].datalen = 0;
		SAFE_INDEX_INC(sess->decoder.rtp_queue, sess->decoder.queue_rindex);
		sess->decoder.queue_size--;

		if (*decoded_data_len != codec->implementation->decoded_bytes_per_packet) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Returning odd decoded frame of %d bytes intead of %d bytes\n", *decoded_data_len, codec->implementation->decoded_bytes_per_packet);
		}
	} else {
		*decoded_data_len = codec->implementation->decoded_bytes_per_packet;
		memset(dbuf_linear, 0, *decoded_data_len);
	}

	if (sess->decoder.debug_timing) {
		func_end_time = switch_micro_time_now();
		difftime = func_end_time - func_start_time;
		if (difftime > 5000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%ldus to execute decoding function in session %lu.\n", (long)difftime, sess->sessid);
		}
		sess->decoder.last_func_call_time = func_end_time;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_sangoma_destroy(switch_codec_t *codec)
{
	struct sangoma_transcoding_session *sess = codec->private_info;
	/* things that you may do here is closing files, sockets or other resources used during the codec session 
	 * no need to free memory allocated from the pool though, the owner of the pool takes care of that */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sangoma destroy called.\n");
	
	switch_mutex_lock(g_sessions_lock);

	if (sess->encoder.txrtp) {
		sngtc_free_transcoding_session(&sess->encoder.reply);
		memset(&sess->encoder, 0, sizeof(sess->encoder));
	}
	if (sess->decoder.txrtp) {
		sngtc_free_transcoding_session(&sess->decoder.reply);
		memset(&sess->decoder, 0, sizeof(sess->decoder));
	}
	
	switch_core_hash_delete(g_sessions_hash, sess->hashkey);

	switch_mutex_unlock(g_sessions_lock);
	return SWITCH_STATUS_SUCCESS;
}

static struct sangoma_transcoding_session *sangoma_find_session(unsigned long sessid)
{
	char hashkey[50];
	snprintf(hashkey, sizeof(hashkey), SANGOMA_SESS_HASH_KEY_FORMAT, sessid);
	return switch_core_hash_find(g_sessions_hash, hashkey);
}

static void sangoma_print_stats(switch_stream_handle_t *stream, switch_rtp_numbers_t *stats)
{
	stream->write_function(stream, "Raw bytes: %lu\n", stats->raw_bytes);
	stream->write_function(stream, "Media bytes: %lu\n", stats->media_bytes);
	stream->write_function(stream, "Packet count: %lu\n", stats->packet_count);
	stream->write_function(stream, "Media packet count: %lu\n", stats->media_packet_count);
	stream->write_function(stream, "Skip packet count: %lu\n", stats->skip_packet_count);
	stream->write_function(stream, "Jitter buffer packet count: %lu\n", stats->jb_packet_count);
	stream->write_function(stream, "DTMF packet count: %lu\n", stats->dtmf_packet_count);
	stream->write_function(stream, "CNG packet count: %lu\n", stats->cng_packet_count);
	stream->write_function(stream, "Flush packet count: %lu\n\n\n", stats->flush_packet_count);
}

#define SANGOMA_SYNTAX "settings|sessions|stats <session>|debug <session>|nodebug <session>"
SWITCH_STANDARD_API(sangoma_function)
{
	char *argv[10] = { 0 };
	int argc = 0;
	char *mycmd = NULL;
	switch_bool_t locked = SWITCH_FALSE;

	if (zstr(cmd)) {
		stream->write_function(stream, "%s", SANGOMA_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(mycmd = strdup(cmd))) {
		return SWITCH_STATUS_MEMERR;
	}

	if (!(argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) || !argv[0]) {
		stream->write_function(stream, "%s", SANGOMA_SYNTAX);
		switch_safe_free(mycmd);
		return SWITCH_STATUS_SUCCESS;
	}

	/* Most operations in this API require the global session lock anyways since sessions can disappear at any moment ... */
	switch_mutex_lock(g_sessions_lock);
	locked = SWITCH_TRUE;

	if (!strcasecmp(argv[0], "settings")) {
		char addrbuff[50];
		int addr;
		addr = htonl(g_rtpip);
		stream->write_function(stream, "\tRTP IP Address: %s\n",
				switch_inet_ntop(AF_INET, &addr, addrbuff, sizeof(addrbuff)));
	} else if (!strcasecmp(argv[0], "sessions")) {
		/* iterate over sessions hash */
		switch_hash_index_t *hi;
		const void *var;
		void *val;
		unsigned totalsess = 0;
#define STATS_FORMAT "%-10.10s %-10.10s %-10.10s %-10.10s %-10.10s %-10.10s %-10.10s %-10.10s %-10.10s %-10.10s %-15.15s %-15.15s\n"
		stream->write_function(stream, STATS_FORMAT,
				"Session", "Codec", "Enc", "Dec", "Enc Tx", "Enc Rx", "Dec Tx", "Dec Rx", "Enc Lost", "Dec Lost", "Enc AvgRxMs", "Dec AvgRxMs");
		for (hi = switch_hash_first(NULL, g_sessions_hash); hi; hi = switch_hash_next(hi)) {
			struct sangoma_transcoding_session *sess;
			char sessid_str[25];
			char encoder_tx_str[25];
			char encoder_rx_str[25];
			char decoder_tx_str[25];
			char decoder_rx_str[25];
			char encoder_lostrx_str[25];
			char decoder_lostrx_str[25];
			char encoder_avgrxus_str[25];
			char decoder_avgrxus_str[25];

			switch_hash_this(hi, &var, NULL, &val);
			sess = val;

			snprintf(sessid_str, sizeof(sessid_str), "%lu", sess->sessid);
			snprintf(encoder_tx_str, sizeof(encoder_tx_str), "%lu", sess->encoder.tx);
			snprintf(encoder_rx_str, sizeof(encoder_rx_str), "%lu", sess->encoder.rx);
			snprintf(decoder_tx_str, sizeof(decoder_tx_str), "%lu", sess->decoder.tx);
			snprintf(decoder_rx_str, sizeof(decoder_rx_str), "%lu", sess->decoder.rx);
			snprintf(encoder_lostrx_str, sizeof(encoder_lostrx_str), "%lu", sess->encoder.rxlost);
			snprintf(decoder_lostrx_str, sizeof(decoder_lostrx_str), "%lu", sess->decoder.rxlost);
			snprintf(encoder_avgrxus_str, sizeof(encoder_avgrxus_str), "%ld", (long)(sess->encoder.avgrxus/1000));
			snprintf(decoder_avgrxus_str, sizeof(encoder_avgrxus_str), "%ld", (long)(sess->decoder.avgrxus/1000));


			stream->write_function(stream, STATS_FORMAT,
					sessid_str, 
					sess->impl->iananame,
					sess->encoder.txrtp ? "Yes" : "No", 
					sess->decoder.txrtp ? "Yes" : "No",
					encoder_tx_str,
					encoder_rx_str,
					decoder_tx_str,
					decoder_rx_str,
					encoder_lostrx_str,
					decoder_lostrx_str,
					encoder_avgrxus_str,
					decoder_avgrxus_str);
			totalsess++;
		}
		stream->write_function(stream, "Total sessions: %d\n", totalsess);
	} else if (!strcasecmp(argv[0], "stats")) {
		struct sangoma_transcoding_session *sess;
		unsigned long sessid = 0;
		switch_rtp_stats_t *stats = NULL;
		int ret = 0;
		if (argc < 2) {
			stream->write_function(stream, "%s", SANGOMA_SYNTAX);
			goto done;
		} 
		ret = sscanf(argv[1], "%lu", &sessid);
		if (ret != 1) {
			stream->write_function(stream, "%s", SANGOMA_SYNTAX);
			goto done;
		} 

		sess = sangoma_find_session(sessid);
		if (!sess) {
			stream->write_function(stream, "Failed to find session %lu\n", sessid);
			goto done;
		}
		stream->write_function(stream, "Stats for transcoding session: %lu\n", sessid);

		if (sess->encoder.rxrtp) {
			stats = switch_rtp_get_stats(sess->encoder.rxrtp, NULL);
			stream->write_function(stream, "=== %s Encoder ===\n", sess->impl->iananame);

			stream->write_function(stream, "Tx L16 from %d.%d.%d.%d:%d to %d.%d.%d.%d:%d\n\n", SNGTC_NIPV4(sess->encoder.reply.a.host_ip), sess->encoder.reply.a.host_udp_port,
					SNGTC_NIPV4(sess->encoder.reply.a.codec_ip), sess->encoder.reply.a.codec_udp_port);
			stream->write_function(stream, "Rx %s at %d.%d.%d.%d:%d from %d.%d.%d.%d:%d\n\n", sess->impl->iananame, SNGTC_NIPV4(sess->encoder.reply.b.host_ip), sess->encoder.reply.b.host_udp_port,
					SNGTC_NIPV4(sess->encoder.reply.b.codec_ip), sess->encoder.reply.b.codec_udp_port);

			stream->write_function(stream, "Ticks: %lu\n", sess->encoder.ticks);

			stream->write_function(stream, "-- Inbound Stats --\n");
			stream->write_function(stream, "Rx Discarded: %lu\n", sess->encoder.rxdiscarded);
			sangoma_print_stats(stream, &stats->inbound);
			

			stats = switch_rtp_get_stats(sess->encoder.txrtp, NULL);
			stream->write_function(stream, "-- Outbound Stats --\n");
			sangoma_print_stats(stream, &stats->outbound);
		}

		if (sess->decoder.rxrtp) {
			stats = switch_rtp_get_stats(sess->decoder.rxrtp, NULL);

			stream->write_function(stream, "=== %s Decoder ===\n", sess->impl->iananame);
			stream->write_function(stream, "Tx %s from %d.%d.%d.%d:%d to %d.%d.%d.%d:%d\n\n", sess->impl->iananame, SNGTC_NIPV4(sess->decoder.reply.a.host_ip), sess->decoder.reply.a.host_udp_port,
					SNGTC_NIPV4(sess->decoder.reply.a.codec_ip), sess->decoder.reply.a.codec_udp_port);
			stream->write_function(stream, "Rx L16 at %d.%d.%d.%d:%d from %d.%d.%d.%d:%d\n\n", SNGTC_NIPV4(sess->decoder.reply.b.host_ip), sess->decoder.reply.b.host_udp_port,
					SNGTC_NIPV4(sess->decoder.reply.b.codec_ip), sess->decoder.reply.b.codec_udp_port);
			stream->write_function(stream, "Ticks: %lu\n", sess->decoder.ticks);

			stream->write_function(stream, "-- Inbound Stats --\n");
			stream->write_function(stream, "Rx Discarded: %lu\n", sess->decoder.rxdiscarded);
			sangoma_print_stats(stream, &stats->inbound);

			stats = switch_rtp_get_stats(sess->decoder.txrtp, NULL);
			stream->write_function(stream, "-- Outbound Stats --\n");
			sangoma_print_stats(stream, &stats->outbound);
		}
	} else if (!strcasecmp(argv[0], "debug")) {
		struct sangoma_transcoding_session *sess;
		unsigned long sessid = 0;
		int ret = 0;
		if (argc < 2) {
			stream->write_function(stream, "%s", SANGOMA_SYNTAX);
			goto done;
		} 
		ret = sscanf(argv[1], "%lu", &sessid);
		if (ret != 1) {
			stream->write_function(stream, "%s", SANGOMA_SYNTAX);
			goto done;
		} 
		sess = sangoma_find_session(sessid);
		if (!sess) {
			stream->write_function(stream, "Failed to find session %lu\n", sessid);
			goto done;
		}
		sess->encoder.debug_timing = 1;
		sess->decoder.debug_timing = 1;
		stream->write_function(stream, "Debug enabled for transcoding session: %lu\n", sessid);
	} else if (!strcasecmp(argv[0], "nodebug")) {
		struct sangoma_transcoding_session *sess;
		unsigned long sessid = 0;
		int ret = 0;
		if (argc < 2) {
			stream->write_function(stream, "%s", SANGOMA_SYNTAX);
			goto done;
		} 
		ret = sscanf(argv[1], "%lu", &sessid);
		if (ret != 1) {
			stream->write_function(stream, "%s", SANGOMA_SYNTAX);
			goto done;
		} 
		sess = sangoma_find_session(sessid);
		if (!sess) {
			stream->write_function(stream, "Failed to find session %lu\n", sessid);
			goto done;
		}
		sess->encoder.debug_timing = 0;
		sess->decoder.debug_timing = 0;
		stream->write_function(stream, "Debug disabled for transcoding session: %lu\n", sessid);
	} else {
		stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
	}

done:
	if (locked) {
		switch_mutex_unlock(g_sessions_lock);
	}
	switch_safe_free(mycmd);

	return SWITCH_STATUS_SUCCESS;
}

static int sangoma_logger(int level, char *fmt, ...)
{
	char *data;
	int ret = 0;
	va_list ap;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	if (ret == -1) {
		return -1;
	}
	va_end(ap);

	switch (level) {
	case SNGTC_LOGLEVEL_DEBUG:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s\n", data);
		break;
	case SNGTC_LOGLEVEL_WARN:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s\n", data);
		break;
	case SNGTC_LOGLEVEL_INFO:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s\n", data);
		break;
	case SNGTC_LOGLEVEL_STATS:
		break;
	case SNGTC_LOGLEVEL_ERROR:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", data);
		break;
	case SNGTC_LOGLEVEL_CRIT:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s\n", data);
		break;
	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unexpected msg with loglevel %d: %s\n", level, data);
	}

	free(data);
	return 0;
}

static int sangoma_parse_config(void)
{
	switch_xml_t cfg, settings, param, xml;
	struct in_addr rtpaddr;
	char localip[255];
	int mask = 0;
	int rc = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Reading sangoma codec configuration\n");
	if (!(xml = switch_xml_open_cfg(SANGOMA_TRANSCODE_CONFIG, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to open sangoma codec configuration %s\n", SANGOMA_TRANSCODE_CONFIG);
		return -1;
	}

	memset(&g_init_cfg, 0, sizeof(g_init_cfg));

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *)switch_xml_attr_soft(param, "name");
				char *val = (char *)switch_xml_attr_soft(param, "value");

				/* this parameter overrides the default list of codecs to load */
				if (!strcasecmp(var, "register")) {
					strncpy(g_codec_register_list, val, sizeof(g_codec_register_list)-1);
					g_codec_register_list[sizeof(g_codec_register_list)-1] = 0;
				} else if (!strcasecmp(var, "noregister")) {
					strncpy(g_codec_noregister_list, val, sizeof(g_codec_noregister_list)-1);
					g_codec_noregister_list[sizeof(g_codec_noregister_list)-1] = 0;
				} else if (!strcasecmp(var, "soapserver")) {
					strncpy(g_soap_url, val, sizeof(g_soap_url)-1);
					g_soap_url[sizeof(g_soap_url)-1] = 0;
				} else if (!strcasecmp(var, "rtpip")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found Sangoma RTP IP %s\n", val);
					if (switch_inet_pton(AF_INET, val, &rtpaddr) <= 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Sangoma RTP IP %s\n", val);
						break;
					}
					g_rtpip = ntohl(rtpaddr.s_addr);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignored unknown Sangoma codec setting %s\n", var);
				}
		}
	}

	if (!g_rtpip) {
		if (SWITCH_STATUS_SUCCESS != switch_find_local_ip(localip, sizeof(localip), &mask, AF_INET)) {
			rc = -1;
			goto done;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "No RTP IP specified, using %s\n", localip);
		switch_inet_pton(AF_INET, localip, &rtpaddr);
		g_rtpip = ntohl(rtpaddr.s_addr);
	}

done:
	switch_xml_free(xml);

	g_init_cfg.host_nic_vocallo_sz = 0;

	return rc;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sangoma_codec_shutdown)
{
	switch_core_hash_destroy(&g_sessions_hash);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_sangoma_codec_load)
{
	/* the codec interface that will be registered with the core */
	switch_codec_interface_t *codec_interface = NULL;
	switch_api_interface_t *api_interface = NULL;
	int i = 0, c = 0;
	int ilbc_done = 0;
	int siren_done = 0;
	vocallo_codec_t *ilbc_codec = NULL;
	vocallo_codec_t *siren_codec = NULL;
	int detected = 0, activated = 0;

	/* make sure we have valid configuration */
	if (sangoma_parse_config()) {
		return SWITCH_STATUS_FALSE;
	}

	g_init_cfg.log = sangoma_logger;
	g_init_cfg.create_rtp = sangoma_create_rtp;
	g_init_cfg.create_rtp_port = sangoma_create_rtp_port;
	g_init_cfg.destroy_rtp = sangoma_destroy_rtp;
	g_init_cfg.release_rtp_port = sangoma_release_rtp_port;

	if (sngtc_detect_init_modules(&g_init_cfg, &detected)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to detect vocallo modules\n");
		return SWITCH_STATUS_FALSE;
	}
    
	if (sngtc_activate_modules(&g_init_cfg, &activated)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to activate vocallo modules\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Detected %d and activated %d Sangoma codec vocallo modules\n", detected, activated);

	if (strlen(g_soap_url)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Using %s SOAP server\n", g_soap_url);
		sngtc_set_soap_server_url(g_soap_url);
	}

	switch_mutex_init(&g_sessions_lock, SWITCH_MUTEX_UNNESTED, pool);

	switch_core_hash_init(&g_sessions_hash, pool);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading codecs, register='%s', noregister='%s'\n", g_codec_register_list, g_codec_noregister_list);
	for (c = 0; g_codec_map[c].codec_id != -1; c++) {

		if (g_codec_map[c].codec_id == SNGTC_CODEC_L16_1 || g_codec_map[c].codec_id == SNGTC_CODEC_L16_2) {
			/* registering L16 does not make any sense */
			continue;
		}

		/* check if the codec is in the load list, otherwise skip it */
		if (strcasecmp(g_codec_register_list, "all") && !strcasestr(g_codec_register_list, g_codec_map[c].iana_name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not loading codec %s because was not found in the load list\n", 
					g_codec_map[c].iana_name);
			continue;
		}

		/* load it unless is named in the noload list */
		if (strcasestr(g_codec_noregister_list, g_codec_map[c].iana_name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not loading codec %s because was not found in the noload list\n", 
					g_codec_map[c].iana_name);
			continue;
		}

		/* special check for iLBC to add a single codec interface for both ILBC bitrate versions */
		if ((g_codec_map[c].codec_id == SNGTC_CODEC_ILBC_152 || g_codec_map[c].codec_id == SNGTC_CODEC_ILBC_133) && ilbc_done) {
			continue;
		}

		/* special check for siren to add a single codec interface for all siren bitrate versions */
		if ((g_codec_map[c].codec_id == SNGTC_CODEC_SIREN7_24 || g_codec_map[c].codec_id == SNGTC_CODEC_SIREN7_32) && siren_done) {
			continue;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Registering implementations for codec %s\n", g_codec_map[c].iana_name);

		/* SWITCH_ADD_CODEC allocates a codec interface structure from the pool the core gave us and adds it to the internal interface 
		 * list the core keeps, gets a codec id and set the given codec name to it.
		 * At this point there is an empty shell codec interface registered, but not yet implementations */
		SWITCH_ADD_CODEC(codec_interface, g_codec_map[c].fs_name);

		/* Now add as many codec implementations as needed, just up to 200ms for now */
		if (g_codec_map[c].autoinit) {
			int ms = 0;
			for (i = 1; i <= 20; i++) {
				ms = i * 10;
				if (g_codec_map[c].maxms < ms) {
					break;
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding %dms implementation of codec %s\n", ms, g_codec_map[c].fs_name);
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 g_codec_map[c].iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 g_codec_map[c].iana_name, /* the IANA code name */
								 NULL,	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 g_codec_map[c].sampling_rate,	/* samples transferred per second */
								 g_codec_map[c].actual_sampling_rate, /* actual samples transferred per second */
								 g_codec_map[c].bps,	/* bits transferred per second */
								 g_codec_map[c].mpf * i, /* microseconds per frame */
								 g_codec_map[c].spf * i, /* samples per frame */
								 g_codec_map[c].bpfd * i, /* number of bytes per frame decompressed */
								 g_codec_map[c].bpfc * i, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 g_codec_map[c].spf * i, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init,	/* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */

			}
		} else {

			/* custom implementation for some codecs */
			switch (g_codec_map[c].codec_id) {
			case SNGTC_CODEC_GSM_FR:
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 g_codec_map[c].iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 g_codec_map[c].iana_name, /* the IANA code name */
								 NULL,	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 8000,	/* samples transferred per second */
								 8000,	/* actual samples transferred per second */
								 g_codec_map[c].bps,	/* bits transferred per second */
								 g_codec_map[c].mpf, /* microseconds per frame */
								 g_codec_map[c].spf, /* samples per frame */
								 g_codec_map[c].bpfd, /* number of bytes per frame decompressed */
								 g_codec_map[c].bpfc, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 g_codec_map[c].spf, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init,	/* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */
				
				break;
			case SNGTC_CODEC_ILBC_133:
			case SNGTC_CODEC_ILBC_152:
				ilbc_codec = get_codec_from_id(SNGTC_CODEC_ILBC_152);
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 ilbc_codec->iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 ilbc_codec->iana_name, /* the IANA code name */
								 "mode=20",	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 8000,	/* samples transferred per second */
								 8000,	/* actual samples transferred per second */
								 15200,	/* bits transferred per second */
								 20000, /* microseconds per frame */
								 160, /* samples per frame */
								 320, /* number of bytes per frame decompressed */
								 38, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 1, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init_ilbc,	/* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */

				ilbc_codec = get_codec_from_id(SNGTC_CODEC_ILBC_133);
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 ilbc_codec->iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 ilbc_codec->iana_name, /* the IANA code name */
								 "mode=30",	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 8000,	/* samples transferred per second */
								 8000,	/* actual samples transferred per second */
								 13330,	/* bits transferred per second */
								 30000, /* microseconds per frame */
								 240, /* samples per frame */
								 480, /* number of bytes per frame decompressed */
								 50, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 1, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init_ilbc, /* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */
				ilbc_done = 1;
				break;

			case SNGTC_CODEC_G723_1_63:

				for (i = 1; i <= 3; i++) {
					switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
									 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
									 g_codec_map[c].iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
									 g_codec_map[c].iana_name, /* the IANA code name */
									 NULL,	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
									 g_codec_map[c].sampling_rate, /* samples transferred per second */
									 g_codec_map[c].actual_sampling_rate, /* actual samples transferred per second */
									 g_codec_map[c].bps, /* bits transferred per second */
									 g_codec_map[c].mpf * i, /* microseconds per frame */
									 g_codec_map[c].spf * i, /* samples per frame */
									 g_codec_map[c].bpfd * i, /* number of bytes per frame decompressed */
									 g_codec_map[c].bpfc * i, /* number of bytes per frame compressed */
									 1,	/* number of channels represented */
									 g_codec_map[c].spf * i, /* number of frames per network packet (I dont think this is used at all) */
									 switch_sangoma_init,	/* function to initialize a codec session using this implementation */
									 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
									 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
									 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */
				}

				break;

			case SNGTC_CODEC_AMR_1220:
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 g_codec_map[c].iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 g_codec_map[c].iana_name, /* the IANA code name */
								 NULL,	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 g_codec_map[c].sampling_rate,	/* samples transferred per second */
								 g_codec_map[c].actual_sampling_rate, /* actual samples transferred per second */
								 g_codec_map[c].bps,	/* bits transferred per second */
								 g_codec_map[c].mpf, /* microseconds per frame */
								 g_codec_map[c].spf, /* samples per frame */
								 g_codec_map[c].bpfd, /* number of bytes per frame decompressed */
								 g_codec_map[c].bpfc, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 g_codec_map[c].spf, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init,	/* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */
				break;

			case SNGTC_CODEC_SIREN7_24:
			case SNGTC_CODEC_SIREN7_32:

				siren_codec = get_codec_from_id(SNGTC_CODEC_SIREN7_24);
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 siren_codec->iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 siren_codec->iana_name, /* the IANA code name */
								 "bitrate=24000",	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 siren_codec->sampling_rate,	/* samples transferred per second */
								 siren_codec->actual_sampling_rate, /* actual samples transferred per second */
								 siren_codec->bps,	/* bits transferred per second */
								 siren_codec->mpf, /* microseconds per frame */
								 siren_codec->spf, /* samples per frame */
								 siren_codec->bpfd, /* number of bytes per frame decompressed */
								 siren_codec->bpfc, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 siren_codec->spf, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init_siren7,	/* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */

				siren_codec = get_codec_from_id(SNGTC_CODEC_SIREN7_32);
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 siren_codec->iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 siren_codec->iana_name, /* the IANA code name */
								 "bitrate=32000",	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 siren_codec->sampling_rate,	/* samples transferred per second */
								 siren_codec->actual_sampling_rate, /* actual samples transferred per second */
								 siren_codec->bps,	/* bits transferred per second */
								 siren_codec->mpf, /* microseconds per frame */
								 siren_codec->spf, /* samples per frame */
								 siren_codec->bpfd, /* number of bytes per frame decompressed */
								 siren_codec->bpfc, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 siren_codec->spf, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init_siren7,	/* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */
				siren_done = 1;
				break;

			default:
				break;
			}
		}
	}


	SWITCH_ADD_API(api_interface, "sangoma_codec", "Sangoma Codec Commands", sangoma_function, SANGOMA_SYNTAX);
	switch_console_set_complete("add sangoma_codec");
	switch_console_set_complete("add sangoma_codec settings");
	switch_console_set_complete("add sangoma_codec sessions");
	switch_console_set_complete("add sangoma_codec stats");
	switch_console_set_complete("add sangoma_codec debug");
	switch_console_set_complete("add sangoma_codec nodebug");

	return SWITCH_STATUS_SUCCESS;
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
