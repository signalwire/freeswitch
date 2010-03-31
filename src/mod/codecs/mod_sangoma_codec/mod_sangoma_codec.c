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
#include "g711.h"

#include <sng_tc/sng_tc.h>

#ifdef __linux__
/* for ethernet device query */
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_sangoma_codec_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sangoma_codec_shutdown);
SWITCH_MODULE_DEFINITION(mod_sangoma_codec, mod_sangoma_codec_load, mod_sangoma_codec_shutdown, NULL);

#define IANA_ULAW 0
#define SANGOMA_SESS_HASH_KEY_FORMAT "sngtc%lu"

/* it seemed we need higher PTIME than the calling parties, so we assume nobody will use higher ptime than 40 */
#define SANGOMA_DEFAULT_SAMPLING_RATE 80
#define SANGOMA_TRANSCODE_CONFIG "sangoma_codec.conf" 

#define SANGOMA_DEFAULT_UDP_PORT 15000
#define SANGOMA_MIN_UDP_PORT 0
#define SANGOMA_MAX_UDP_PORT 65535

/* \brief vocallos configuration */
static sngtc_init_cfg_t g_init_cfg;
static char g_vocallo_names[SNGTC_MAX_HOST_VOCALLO_NIC][255];

/* \brief protect vocallo session creation and destroy */
static switch_mutex_t *g_sessions_lock = NULL;

/* \brief next unique session id (protected by g_sessions_lock) */
unsigned long long g_next_session_id = 0;

/* hash of sessions (I think a linked list suits better here, but FS does not have the data type) */
switch_hash_t *g_sessions_hash = NULL;

/* global memory pool provided by FS */
switch_memory_pool_t *g_pool = NULL;

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

	int autoinit; /* initialize on start loop or manually */
} vocallo_codec_t;

vocallo_codec_t g_codec_map[] =
{
	{ SNGTC_CODEC_PCMU,    0,   "PCMU",    "Sangoma PCMU",      40, 64000,  10000, 80,  160, 80,  1 },
	{ SNGTC_CODEC_PCMA,    8,   "PCMA",    "Sangoma PCMA",      40, 64000,  10000, 80,  160, 80,  1 },
	{ SNGTC_CODEC_L16_1,   10,  "L16",     "Sangoma L16",       40, 120000, 10000, 80,  160, 160, 1 },
	{ SNGTC_CODEC_G729AB,  18,  "G729",    "Sangoma G729",      40, 8000,   10000, 80,  160, 10,  1 },
	{ SNGTC_CODEC_G726_32, 122, "G726-32", "Sangoma G.726 32k", 40, 32000,  10000, 80,  160, 40,  1 },
	{ SNGTC_CODEC_GSM_FR,  3,   "GSM",     "Sangoma GSM",       20, 13200,  20000, 160, 320, 33,  0 },
#if 0
	/* FIXME: grandstream crashes with iLBC implementation */
	{ SNGTC_CODEC_ILBC,    97,  "iLBC",    "Sangoma ILBC",      -1, -1,     -1,    -1,  -1,  -1,  0 },
	/* FIXME: sampling rate seems wrong with this, audioooo soooundssssss sloooooow ... */
	{ SNGTC_CODEC_G722,    9,   "G722",    "Sangoma G722",      20, 64000,  20000, 160, 640, 160, 0 },
#endif
	{ -1,                  -1,  NULL,      NULL,                -1, -1,     -1,    -1,  -1,      -1 },
};

/* default codec list to load, users may override, special codec 'all' loads everything available unless listed in noload */
static char g_codec_load_list[1024] = "all";

/* default codec list to NOT load, users may override */
static char g_codec_noload_list[1024] = "";

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

	/* Lost packets */
  	long lastrxseqno;
	unsigned long rxlost;

	/* avg Rx time */
	switch_time_t avgrxus;
	switch_time_t last_rx_time;

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

static vocallo_codec_t *get_codec_from_iana(int iana)
{
	int i;
	for (i = 0; g_codec_map[i].codec_id != -1; i++) {
		if (iana == g_codec_map[i].iana) {
			return &g_codec_map[i];
		}
	}
	return NULL;
}

static int sangoma_create_rtp(void *usr_priv, sngtc_codec_request_leg_t *codec_reg_leg, sngtc_codec_reply_leg_t* codec_reply_leg, void **rtp_fd)
{
	switch_rtp_t *rtp_session = NULL;
	switch_port_t rtp_port;
	char codec_ip[255];
	char local_ip[255];
	switch_rtp_flag_t flags = 0;
	int iana = 0;
	const char *err = NULL;
	struct sangoma_transcoding_session *sess = usr_priv;
	struct in_addr local_ip_addr = { 0 };

	local_ip_addr.s_addr = htonl(codec_reply_leg->host_ip);
	
	switch_inet_ntop(AF_INET, &local_ip_addr, local_ip, sizeof(local_ip));

	/* request a port */
	if (!(rtp_port = switch_rtp_request_port(local_ip))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate RTP port for IP %s\n", local_ip);
		return -1;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocated port %d for IP %s\n", rtp_port, local_ip);

	codec_reg_leg->host_udp_port = rtp_port;

	sngtc_codec_ipv4_hex_to_str(codec_reply_leg->codec_ip, codec_ip);

	iana = codec_id_to_iana(codec_reg_leg->codec_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Creating RTP session for host (%s/%d)  vocallo(%s/%d) Iana=%d ms=%d idx=%lu\n",
					  local_ip, rtp_port, codec_ip, codec_reply_leg->codec_udp_port, iana, 
					  codec_reg_leg->ms*1000, sess->sessid);

	/* create the RTP socket, dont use the session pool since the session may go away while the RTP socket should linger around 
	 * until sangoma_transcode decides to kill it (possibly because the same RTP session is used for a different call) */
	rtp_session = switch_rtp_new(local_ip, rtp_port, 
			codec_ip, codec_reply_leg->codec_udp_port, 
			iana,
			sess->impl->samples_per_packet,
			codec_reg_leg->ms*1000, /* microseconds per packet */
			flags, NULL, &err, g_pool);

	if (!rtp_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create switch rtp session: %s\n", err);
		return -1;
	}

	*rtp_fd = rtp_session;

	return 0;
}

static int sangoma_destroy_rtp(void *usr_priv, void *fd)
{
	switch_rtp_t *rtp = fd;
	switch_rtp_destroy(&rtp);
	return 0;
}

static switch_status_t switch_sangoma_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct sangoma_transcoding_session *sess = NULL;
	vocallo_codec_t *vcodec;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sangoma init called.\n");

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

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

	vcodec = get_codec_from_iana(codec->implementation->ianacode);
	
	switch_mutex_lock(g_sessions_lock);

	if (encoding) {
		sess->encoder.request.usr_priv = sess;
		sess->encoder.request.a.codec_id = SNGTC_CODEC_PCMU;
		sess->encoder.request.a.ms = codec->implementation->microseconds_per_packet/1000;

		sess->encoder.request.b.codec_id = vcodec->codec_id;
		sess->encoder.request.b.ms = codec->implementation->microseconds_per_packet/1000;
	}

	if (decoding) {
		sess->decoder.request.usr_priv = sess;
		sess->decoder.request.a.codec_id = vcodec->codec_id;
		sess->decoder.request.a.ms = codec->implementation->microseconds_per_packet/1000;

		sess->decoder.request.b.codec_id = SNGTC_CODEC_PCMU;
		sess->decoder.request.b.ms = codec->implementation->microseconds_per_packet/1000;

	}

	sess->sessid = g_next_session_id++;
	switch_snprintf(sess->hashkey, sizeof(sess->hashkey), SANGOMA_SESS_HASH_KEY_FORMAT, sess->sessid);
	switch_core_hash_insert(g_sessions_hash, sess->hashkey, sess);

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
	switch_frame_t ulaw_frame;
	switch_frame_t encoded_frame;
	switch_status_t sres;
	switch_time_t now_time, difftime;
	unsigned char ebuf_ulaw[decoded_data_len / 2];
	short *dbuf_linear;
	int i = 0;
	int res = 0;
	struct sangoma_transcoding_session *sess = codec->private_info;

	/* start assuming we will not encode anything */
	*encoded_data_len = 0;

	/* initialize on first use */
	if (!sess->encoder.txrtp) {
		int err = 0;
		switch_mutex_lock(g_sessions_lock);
		err = sngtc_create_transcoding_session(&sess->encoder.request, &sess->encoder.reply, 0);
		if (err) {
			switch_mutex_unlock(g_sessions_lock);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create Sangoma encoding session.\n");
			return SWITCH_STATUS_FALSE;
		}
		sess->encoder.txrtp = sess->encoder.reply.tx_fd;
		sess->encoder.rxrtp = sess->encoder.reply.rx_fd;
		switch_mutex_unlock(g_sessions_lock);
	}

	/* transcode to ulaw first */
	dbuf_linear = decoded_data;

	for (i = 0; i < decoded_data_len / sizeof(short); i++) {
		ebuf_ulaw[i] = linear_to_ulaw(dbuf_linear[i]);
	}
	
	/* do the writing */
	memset(&ulaw_frame, 0, sizeof(ulaw_frame));	
	ulaw_frame.source = __FUNCTION__;
	ulaw_frame.data = ebuf_ulaw;
	ulaw_frame.datalen = i;
	ulaw_frame.payload = IANA_ULAW;

	res = switch_rtp_write_frame(sess->encoder.txrtp, &ulaw_frame);
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
	memset(&encoded_frame, 0, sizeof(encoded_frame));
	sres = switch_rtp_zerocopy_read_frame(sess->encoder.rxrtp, &encoded_frame, SWITCH_IO_FLAG_NOBLOCK);
	if (sres == SWITCH_STATUS_GENERR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read on Sangoma encoder RTP session: %d\n", sres);
		return SWITCH_STATUS_FALSE;
	}

	if (0 == encoded_frame.datalen) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No output on Sangoma encoder RTP session.\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (encoded_frame.payload != codec->implementation->ianacode) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Read unexpected payload %d in Sangoma encoder RTP session, expecting %d\n", 
				encoded_frame.payload, codec->implementation->ianacode);
		return SWITCH_STATUS_FALSE;
	}
	memcpy(encoded_data, encoded_frame.data, encoded_frame.datalen);
	*encoded_data_len = encoded_frame.datalen;

	/* update encoding stats */
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
	switch_frame_t ulaw_frame;
	switch_status_t sres;
	switch_time_t now_time, difftime;
	short *dbuf_linear;
	int i = 0;
	int res = 0;
	struct sangoma_transcoding_session *sess = codec->private_info;

	dbuf_linear = decoded_data;

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
			switch_mutex_unlock(g_sessions_lock);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create Sangoma decoding session.\n");
			return SWITCH_STATUS_FALSE;
		}
		sess->decoder.txrtp = sess->decoder.reply.tx_fd;
		sess->decoder.rxrtp = sess->decoder.reply.rx_fd;
		switch_mutex_unlock(g_sessions_lock);
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
	memset(&ulaw_frame, 0, sizeof(ulaw_frame));
	sres = switch_rtp_zerocopy_read_frame(sess->decoder.rxrtp, &ulaw_frame, SWITCH_IO_FLAG_NOBLOCK);
	if (sres == SWITCH_STATUS_GENERR) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read on Sangoma decoder RTP session: %d\n", sres);
		return SWITCH_STATUS_FALSE;
	}

	if (0 == ulaw_frame.datalen) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No output on Sangoma decoder RTP session.\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (ulaw_frame.payload != IANA_ULAW) { 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Read unexpected payload %d in Sangoma decoder RTP session, expecting %d\n", 
				ulaw_frame.payload, IANA_ULAW);
		return SWITCH_STATUS_FALSE;
	}

	/* transcode to linear */
	for (i = 0; i < ulaw_frame.datalen; i++) {
		dbuf_linear[i] = ulaw_to_linear(((char *)ulaw_frame.data)[i]);
	}
	*decoded_data_len = i * 2;

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
		if (ulaw_frame.seq > (sess->decoder.lastrxseqno + 2) ) {
			sess->decoder.rxlost += ulaw_frame.seq - sess->decoder.lastrxseqno - 1;
		}
	}
	sess->decoder.lastrxseqno = ulaw_frame.seq;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_sangoma_destroy(switch_codec_t *codec)
{
	struct sangoma_transcoding_session *sess = codec->private_info;
	/* things that you may do here is closing files, sockets or other resources used during the codec session 
	 * no need to free memory allocated from the pool though, the owner of the pool takes care of that */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sangoma destroy called.\n");
	
	switch_mutex_lock(g_sessions_lock);

	if (sess->encoder.reply.codec_rtp_session) {
		sngtc_destroy_transcoding_session(sess->encoder.reply.codec_rtp_session);
	}
	if (sess->decoder.reply.codec_rtp_session) {
		sngtc_destroy_transcoding_session(sess->decoder.reply.codec_rtp_session);
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

#define SANGOMA_SYNTAX "settings|sessions|stats <session>"
SWITCH_STANDARD_API(sangoma_function)
{
	char *argv[10] = { 0 };
	int argc = 0;
	char *mycmd = NULL;

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

	if (!strcasecmp(argv[0], "settings")) {
		char addrbuff[50];
		int addr;
		int i;
		for (i = 0; i < g_init_cfg.host_nic_vocallo_sz; i++) {
			stream->write_function(stream, "Vocallo %s:\n", g_vocallo_names[i]);

			addr = htonl(g_init_cfg.host_nic_vocallo_cfg[i].host_ip);
			stream->write_function(stream, "\tIP Address: %s\n", 
					switch_inet_ntop(AF_INET, &addr, addrbuff, sizeof(addrbuff)));

			addr = htonl(g_init_cfg.host_nic_vocallo_cfg[i].host_ip_netmask);
			stream->write_function(stream, "\tNetmask: %s\n", 
					switch_inet_ntop(AF_INET, &addr, addrbuff, sizeof(addrbuff)));

			addr = htonl(g_init_cfg.host_nic_vocallo_cfg[i].vocallo_ip);
			stream->write_function(stream, "\tVocallo Base IP: %s\n", 
					switch_inet_ntop(AF_INET, &addr, addrbuff, sizeof(addrbuff)));

			stream->write_function(stream, "\tVocallo Base UDP: %d\n\n", g_init_cfg.host_nic_vocallo_cfg[i].vocallo_base_udp_port);
		}
	} else if (!strcasecmp(argv[0], "sessions")) {
		/* iterate over sessions hash */
		switch_hash_index_t *hi;
		const void *var;
		void *val;
		unsigned totalsess = 0;
		switch_mutex_lock(g_sessions_lock);
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
		switch_mutex_unlock(g_sessions_lock);
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
		stream->write_function(stream, "Session: %lu\n", sessid);

		if (sess->encoder.rxrtp) {
			stats = switch_rtp_get_stats(sess->encoder.rxrtp, NULL);
			stream->write_function(stream, "-- Encoder Inbound Stats --\n");
			sangoma_print_stats(stream, &stats->inbound);
			

			stats = switch_rtp_get_stats(sess->encoder.txrtp, NULL);
			stream->write_function(stream, "-- Encoder Outbound Stats --\n");
			sangoma_print_stats(stream, &stats->outbound);
		}

		if (sess->decoder.rxrtp) {
			stats = switch_rtp_get_stats(sess->decoder.rxrtp, NULL);
			stream->write_function(stream, "-- Decoder Inbound Stats --\n");
			sangoma_print_stats(stream, &stats->inbound);

			stats = switch_rtp_get_stats(sess->decoder.txrtp, NULL);
			stream->write_function(stream, "-- Decoder Outbound Stats --\n");
			sangoma_print_stats(stream, &stats->outbound);
		}
	} else {
		stream->write_function(stream, "Unknown Command [%s]\n", argv[0]);
	}

done:
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

static int load_nic_network_information(const char *nic, sngtc_host_nic_vocallo_cfg_t *cfg)
{
#ifdef __linux__
	struct ifreq ifr;
	int sock;
	char *mac;
	int j = 0, k = 0, ret = 0;
	
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (-1 == sock) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
			"Failed to create socket to query network configuration for NIC %s: %s\n", nic, strerror(errno));
		return -1;
	}

	strncpy(ifr.ifr_name, nic, sizeof(ifr.ifr_name)-1);
	ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';

	if (-1 == ioctl(sock, SIOCGIFADDR, &ifr)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to query IP address for NIC %s: %s\n", nic, strerror(errno));
		ret = -1;
		goto done;
	}
	cfg->host_ip = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);

	if (-1 == ioctl(sock, SIOCGIFNETMASK, &ifr)) {
		switch_log_printf(SWITCH_CHANNEL_LOG,  SWITCH_LOG_ERROR, "Failed to query network address for NIC %s: %s\n", nic, strerror(errno));
		ret = -1;
		goto done;
	}
	cfg->host_ip_netmask = ntohl(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr);

	if (-1 == ioctl(sock, SIOCGIFHWADDR, &ifr)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to query HW address for NIC %s: %s\n", nic, strerror(errno));
		ret = -1;
		goto done;
	}

	mac = cfg->host_mac.mac_str;
	for (j = 0, k = 0; j < 6; j++) {
		k += snprintf(mac + k, sizeof(cfg->host_mac.mac_str) - k, j ? "-%02X" : "%02X",
				(int)(unsigned int)(unsigned char)ifr.ifr_hwaddr.sa_data[j]);
	}
	mac[sizeof(cfg->host_mac.mac_str)-1] = '\0';

done:
	close(sock);
	return ret;
#else
	return 0;
#endif
}

static int sangoma_parse_config(void)
{
	switch_xml_t cfg, settings, param, vocallos, xml, vocallo;
	struct in_addr vocallo_base_ip;
	char ipbuff[50];
	char netbuff[50];
	int host_ipaddr = 0;
	int host_netmaskaddr = 0;
	int vidx = 0;
	int baseudp = 0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Reading sangoma codec configuration\n");
	if (!(xml = switch_xml_open_cfg(SANGOMA_TRANSCODE_CONFIG, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to open sangoma codec configuration %s\n", SANGOMA_TRANSCODE_CONFIG);
		return -1;
	}

	memset(&g_init_cfg, 0, sizeof(g_init_cfg));

	if ((settings = switch_xml_child(cfg, "settings"))) {
		/* nothing here yet */
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *)switch_xml_attr_soft(param, "name");
				char *val = (char *)switch_xml_attr_soft(param, "value");

				/* this parameter overrides the default list of codecs to load */
				if (!strcasecmp(var, "load")) {
					strncpy(g_codec_load_list, val, sizeof(g_codec_load_list)-1);
					g_codec_load_list[sizeof(g_codec_load_list)-1] = 0;
				} else if (!strcasecmp(var, "noload")) {
					strncpy(g_codec_noload_list, val, sizeof(g_codec_noload_list)-1);
					g_codec_noload_list[sizeof(g_codec_noload_list)-1] = 0;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignored unknown Sangoma codec setting %s\n", var);
				}
		}
	}

	if ((vocallos = switch_xml_child(cfg, "vocallos"))) {
		for (vocallo = switch_xml_child(vocallos, "vocallo"); vocallo; vocallo = vocallo->next) {
			const char *name = switch_xml_attr(vocallo, "name");
			if (!name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Sangoma vocallo found with no name= attribute, ignoring!\n");
				continue;
			}

			if (load_nic_network_information(name, &g_init_cfg.host_nic_vocallo_cfg[vidx])) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 
						"Ignoring vocallo %s, failed to retrieve its network configuration\n", name);
				continue;
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Configuring vocallo %s\n", name);

			g_init_cfg.host_nic_vocallo_cfg[vidx].vocallo_base_udp_port = SANGOMA_DEFAULT_UDP_PORT;
			for (param = switch_xml_child(vocallo, "param"); param; param = param->next) {
				char *var = (char *)switch_xml_attr_soft(param, "name");
				char *val = (char *)switch_xml_attr_soft(param, "value");

				/* starting UDP port to be used by the vocallo modules */
				if (!strcasecmp(var, "baseudp")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found Sangoma codec base udp port %s\n", val);
					baseudp = atoi(val);
					if (baseudp < SANGOMA_MIN_UDP_PORT || baseudp > SANGOMA_MAX_UDP_PORT) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 
								"Invalid Sangoma codec base udp port %s, using default %d\n", 
								val, SANGOMA_DEFAULT_UDP_PORT);
						break;
					}
					g_init_cfg.host_nic_vocallo_cfg[vidx].vocallo_base_udp_port = baseudp;
				}
				else if (!strcasecmp(var, "vocalloaddr")) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found Sangoma codec vocallo addr %s\n", val);
					if (switch_inet_pton(AF_INET, val, &vocallo_base_ip) <= 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Sangoma codec vocallo addr %s\n", val);
						break;
					}
					g_init_cfg.host_nic_vocallo_cfg[vidx].vocallo_ip = ntohl(vocallo_base_ip.s_addr);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignored unknown Sangoma vocallo setting %s\n", var);
				}
			}

			if (!g_init_cfg.host_nic_vocallo_cfg[vidx].vocallo_ip) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring vocallo %s, no valid address was configured\n", name);
				continue;
			}
			host_ipaddr = htonl(g_init_cfg.host_nic_vocallo_cfg[vidx].host_ip);
			host_netmaskaddr = htonl(g_init_cfg.host_nic_vocallo_cfg[vidx].host_ip_netmask);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, 
					"Configured Sangoma transcoding interface %s, IP address %s, netmask %s\n", 
					name, 
					switch_inet_ntop(AF_INET, &host_ipaddr, ipbuff, sizeof(ipbuff)),
					switch_inet_ntop(AF_INET, &host_netmaskaddr, netbuff, sizeof(netbuff)));
			strncpy(g_vocallo_names[vidx], name, sizeof(g_vocallo_names[vidx])-1);
			g_vocallo_names[vidx][sizeof(g_vocallo_names[vidx])-1] = 0;
			vidx++;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No <vocallos> section found in configuration file %s\n", SANGOMA_TRANSCODE_CONFIG);
	}

	switch_xml_free(xml);

	if (!vidx) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
		"No vocallos were configured, make sure there is at least one <vocallo> in %s.\n", SANGOMA_TRANSCODE_CONFIG);
	}

	g_init_cfg.host_nic_vocallo_sz = vidx;

	return 0;
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
	int detected = 0, activated = 0;

	/* make sure we have valid configuration */
	if (sangoma_parse_config()) {
		return SWITCH_STATUS_FALSE;
	}

	g_pool = pool;

	g_init_cfg.log = sangoma_logger;
	g_init_cfg.create_rtp = sangoma_create_rtp;
	g_init_cfg.destroy_rtp = sangoma_destroy_rtp;

	if (sngtc_detect_init_modules(&g_init_cfg, &detected)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to detect vocallo modules\n");
		return SWITCH_STATUS_FALSE;
	}
    
	if (sngtc_activate_modules(&g_init_cfg, &activated)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to activate vocallo modules\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Detected %d and activated %d Sangoma codec vocallo modules\n", detected, activated);

	switch_mutex_init(&g_sessions_lock, SWITCH_MUTEX_UNNESTED, pool);

	switch_core_hash_init(&g_sessions_hash, pool);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading codecs, load='%s', noload='%s'\n", g_codec_load_list, g_codec_noload_list);
	for (c = 0; g_codec_map[c].codec_id != -1; c++) {

		/* check if the codec is in the load list, otherwise skip it */
		if (strcasecmp(g_codec_load_list, "all") && !strcasestr(g_codec_load_list, g_codec_map[c].iana_name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not loading codec %s because was not found in the load list\n", 
					g_codec_map[c].iana_name);
			continue;
		}

		/* load it unless is named in the noload list */
		if (strcasestr(g_codec_noload_list, g_codec_map[c].iana_name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not loading codec %s because was not found in the noload list\n", 
					g_codec_map[c].iana_name);
			continue;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Registering implementations for codec %s\n", g_codec_map[c].iana_name);

		/* let know the library which iana to use */
		sngtc_set_iana_code_based_on_codec_id(g_codec_map[c].codec_id, g_codec_map[c].iana);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Mapped codec %d to IANA %d\n", g_codec_map[c].codec_id, g_codec_map[c].iana);

		/* SWITCH_ADD_CODEC allocates a codec interface structure from the pool the core gave us and adds it to the internal interface 
		 * list the core keeps, gets a codec id and set the given codec name to it.
		 * At this point there is an empty shell codec interface registered, but not yet implementations */
		SWITCH_ADD_CODEC(codec_interface, g_codec_map[c].fs_name);

		/* Now add as many codec implementations as needed, just up to 40ms for now */
		if (g_codec_map[c].autoinit) {
			for (i = 1; i <= 4; i++) {

				if ((g_codec_map[c].maxms/10) < i) {
					continue;
				}

				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 g_codec_map[c].iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 g_codec_map[c].iana_name, /* the IANA code name */
								 NULL,	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 8000,	/* samples transferred per second */
								 8000,	/* actual samples transferred per second */
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
			case SNGTC_CODEC_ILBC:
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 g_codec_map[c].iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 g_codec_map[c].iana_name, /* the IANA code name */
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

#if 0
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 g_codec_map[c].iana,	/* 97, the IANA code number */
								 g_codec_map[c].iana_name, /* the IANA code name */
								 "mode=30",	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 8000,	/* samples transferred per second */
								 8000,	/* actual samples transferred per second */
								 13300,	/* bits transferred per second */
								 30000, /* microseconds per frame */
								 240, /* samples per frame */
								 480, /* number of bytes per frame decompressed */
								 50, /* number of bytes per frame compressed */
								 1,	/* number of channels represented */
								 1, /* number of frames per network packet (I dont think this is used at all) */
								 switch_sangoma_init,	/* function to initialize a codec session using this implementation */
								 switch_sangoma_encode,	/* function to encode slinear data into encoded data */
								 switch_sangoma_decode,	/* function to decode encoded data into slinear data */
								 switch_sangoma_destroy); /* deinitalize a codec handle using this implementation */
#endif
				break;

			case SNGTC_CODEC_G722:
				switch_core_codec_add_implementation(pool, codec_interface,	/* the codec interface we allocated and we want to register with the core */
								 SWITCH_CODEC_TYPE_AUDIO, /* enumeration defining the type of the codec */
								 g_codec_map[c].iana,	/* the IANA code number, ie http://www.iana.org/assignments/rtp-parameters */
								 g_codec_map[c].iana_name, /* the IANA code name */
								 NULL,	/* default fmtp to send (can be overridden by the init function), fmtp is used in SDP for format specific parameters */
								 8000,	/* samples transferred per second */
								 16000,	/* actual samples transferred per second */
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
