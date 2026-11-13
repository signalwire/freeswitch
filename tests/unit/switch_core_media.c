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
 * switch_core_media.c -- tests for the core media layer.
 */

#include <switch.h>
#include <test/switch_test.h>

/* SDP media sections for the Opus offer tests. RFC 7587 fixes the rtpmap at opus/48000/2; the
   audio rate a peer wants is carried by maxplaybackrate/sprop-maxcapturerate in the fmtp. */
#define OPUS_16K_ATTRS "a=rtpmap:96 opus/48000/2\r\n" \
	"a=fmtp:96 useinbandfec=1;maxplaybackrate=16000;sprop-maxcapturerate=16000\r\n"
#define OPUS_8K_ATTRS "a=rtpmap:96 opus/48000/2\r\n" \
	"a=fmtp:96 useinbandfec=1;maxplaybackrate=8000;sprop-maxcapturerate=8000\r\n"
#define OPUS_24K_ATTRS "a=rtpmap:96 opus/48000/2\r\n" \
	"a=fmtp:96 useinbandfec=1;maxplaybackrate=24000;sprop-maxcapturerate=24000\r\n"
#define OPUS_PLAIN_ATTRS "a=rtpmap:96 opus/48000/2\r\n"
#define OPUS_WEBRTC_ATTRS "a=rtpmap:96 opus/48000/2\r\na=fmtp:96 minptime=10;useinbandfec=1\r\n"
#define PCMU_ATTRS "a=rtpmap:0 PCMU/8000\r\n"

#define OFFER_OPUS_16K "m=audio 11114 RTP/AVP 96\r\n" OPUS_16K_ATTRS
#define OFFER_OPUS_16K_PTIME40 "m=audio 11114 RTP/AVP 96\r\n" OPUS_16K_ATTRS "a=ptime:40\r\n"
#define OFFER_OPUS_16K_PCMU "m=audio 11114 RTP/AVP 96 0\r\n" OPUS_16K_ATTRS PCMU_ATTRS
#define OFFER_PCMU_OPUS_16K "m=audio 11114 RTP/AVP 0 96\r\n" PCMU_ATTRS OPUS_16K_ATTRS
#define OFFER_OPUS_8K "m=audio 11114 RTP/AVP 96\r\n" OPUS_8K_ATTRS
#define OFFER_OPUS_24K "m=audio 11114 RTP/AVP 96\r\n" OPUS_24K_ATTRS
#define OFFER_OPUS_24K_PCMU "m=audio 11114 RTP/AVP 96 0\r\n" OPUS_24K_ATTRS PCMU_ATTRS
#define OFFER_OPUS_PLAIN "m=audio 11114 RTP/AVP 96\r\n" OPUS_PLAIN_ATTRS
#define OFFER_OPUS_PLAIN_PCMU "m=audio 11114 RTP/AVP 96 0\r\n" OPUS_PLAIN_ATTRS PCMU_ATTRS
#define OFFER_OPUS_WEBRTC_PCMU "m=audio 11114 RTP/AVP 96 0\r\n" OPUS_WEBRTC_ATTRS PCMU_ATTRS
#define OFFER_ILBC "m=audio 11114 RTP/AVP 97\r\na=rtpmap:97 iLBC/8000\r\n"

/* Negotiate one remote offer against a codec preference string on a fresh session and report the
   audio codec it selected. Returns the switch_core_media_negotiate_sdp() result. */
static uint8_t negotiate_audio_offer(const char *prefs, const char *mode, const char *media,
									 char *codec, size_t codec_len, int *rate)
{
	switch_core_session_t *session = NULL;
	switch_channel_t *channel;
	switch_media_handle_t *smh = NULL;
	switch_core_media_params_t *mparams;
	switch_call_cause_t cause;
	const char *val;
	char *sdp;
	uint8_t match = 0, proceed = 0;

	*codec = '\0';
	*rate = 0;

	switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE,
						 NULL, NULL);
	if (!session) {
		return 0;
	}

	channel = switch_core_session_get_channel(session);
	mparams = switch_core_session_alloc(session, sizeof(*mparams));
	mparams->rtpip = switch_core_session_strdup(session, "127.0.0.1");

	if (switch_media_handle_create(&smh, session, mparams) == SWITCH_STATUS_SUCCESS) {
		switch_channel_set_variable(channel, "absolute_codec_string", prefs);
		switch_channel_set_variable(channel, "rtp_codec_negotiation", mode);
		switch_core_media_prepare_codecs(session, SWITCH_TRUE);

		sdp = switch_core_session_sprintf(session, "v=0\r\no=- 1 1 IN IP4 127.0.0.1\r\ns=-\r\n"
										  "c=IN IP4 127.0.0.1\r\nt=0 0\r\n%s", media);
		match = switch_core_media_negotiate_sdp(session, sdp, &proceed, SDP_OFFER);

		if (match) {
			if ((val = switch_channel_get_variable(channel, "rtp_use_codec_name"))) {
				switch_copy_string(codec, val, codec_len);
			}
			if ((val = switch_channel_get_variable(channel, "rtp_use_codec_rate"))) {
				*rate = atoi(val);
			}
		}
	}

	switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	switch_core_session_rwunlock(session);

	return match;
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_core_media)
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

		FST_TEST_BEGIN(test_add_crypto_keysalt_bounds)
		{
			switch_core_session_t *session = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			switch_secure_settings_t ssec;
			int i;

			/* Writable buffers: switch_core_media_add_crypto() strips spaces in place. Each key||salt is a
			   40-char base64 that decodes to the 30-byte key+salt of AES_CM_128_HMAC_SHA1_80. RFC 4568 crypto
			   line: "<tag> <suite> inline:<key||salt> ["|" lifetime] ["|" MKI ":" length]". rfc_full and
			   rfc_mki_only are the RFC's two verbatim examples; a multi-key attribute requires every key to
			   carry an equal-length MKI (RFC 4568 section 6.1). '/' (base64 63) decodes to 0xFF bytes. */
			char valid_crypto[]     = "1 AES_CM_128_HMAC_SHA1_80 inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
			char nonzero_crypto[]   = "1 AES_CM_128_HMAC_SHA1_80 inline:////////////////////////////////////////";
			char rfc_full[]         = "1 AES_CM_128_HMAC_SHA1_80 inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:4";
			char rfc_mki_only[]     = "1 AES_CM_128_HMAC_SHA1_80 inline:YUJDZGVmZ2hpSktMbW9QUXJzVHVWd3l6MTIzNDU2|1066:4";
			char rfc_multikey[]     = "1 AES_CM_128_HMAC_SHA1_80 inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:4 inline:YUJDZGVmZ2hpSktMbW9QUXJzVHVWd3l6MTIzNDU2|2^20|2:4";
			char zero_len_space[]   = "1 AES_CM_128_HMAC_SHA1_80 inline: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
			char zero_len_pipe[]    = "1 AES_CM_128_HMAC_SHA1_80 inline:|2^20|1:4";
			char short_keysalt[]    = "1 AES_CM_128_HMAC_SHA1_80 inline:QUJD";
			char overlong_keysalt[] = "1 AES_CM_128_HMAC_SHA1_80 inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
			char zero_len_tail[]    = "1 AES_CM_128_HMAC_SHA1_80 inline: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
			/* Other suites carry longer key+salts (RFC 6188): AES_192 is 38 bytes (52 base64 chars),
			   AES_256 is 46 bytes (64 chars - the largest keysalt that fits the copy buffer). Both
			   decode to all-0xFF bytes. aes256_short is a 128-length keysalt (30 bytes) offered under
			   the 256 suite. */
			char aes192_crypto[]    = "1 AES_192_CM_HMAC_SHA1_80 inline://////////////////////////////////////////////////8=";
			char aes256_crypto[]    = "1 AES_256_CM_HMAC_SHA1_80 inline://///////////////////////////////////////////////////////////w==";
			char aes256_short[]     = "1 AES_256_CM_HMAC_SHA1_80 inline:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

			/* Independent base64 reference decode (not the parser's) of RFC 4568's first example key||salt. */
			unsigned char rfc_full_raw[30] = {
				0x77, 0x44, 0x66, 0x76, 0x67, 0x26, 0x54, 0x2b, 0x29, 0x78,
				0x47, 0x37, 0x40, 0x66, 0x62, 0x35, 0x6a, 0x55, 0x2c, 0x52,
				0x61, 0x41, 0x7d, 0x5c, 0x7c, 0x70, 0x30, 0x25, 0x2a, 0x23 };
			unsigned char ff_raw[46];	/* all-'/' keysalts decode to 0xFF bytes; raw_len of them compared per suite */

			/* One row per crypto line: expected add_crypto() result and, for accepted lines, the expected
			   decoded key+salt over raw_len bytes (raw == NULL skips the content check). Covers: valid AES-128;
			   byte-exact decode; RFC lifetime+MKI, MKI-only (lifetime/MKI disambiguation) and multi-key
			   (loop + buffer reuse); the AES-192 (38-byte) and AES-256 (46-byte, max-length) suite key+salts;
			   a keysalt too short for its suite; both zero-length rejects (space and '|' opts branch); short
			   decode; over-long. */
			struct {
				const char *desc;
				char *crypto;
				switch_status_t expect;
				const unsigned char *raw;
				size_t raw_len;
			} cases[] = {
				{ "well-formed keysalt (AES-128)", valid_crypto,     SWITCH_STATUS_SUCCESS, NULL,         0  },
				{ "byte-exact decode (AES-128)",   nonzero_crypto,   SWITCH_STATUS_SUCCESS, ff_raw,       30 },
				{ "RFC lifetime+MKI key",          rfc_full,         SWITCH_STATUS_SUCCESS, rfc_full_raw, 30 },
				{ "RFC MKI-only key",              rfc_mki_only,     SWITCH_STATUS_SUCCESS, NULL,         0  },
				{ "RFC multi-key attribute",       rfc_multikey,     SWITCH_STATUS_SUCCESS, rfc_full_raw, 30 },
				{ "AES-192 keysalt (38-byte)",     aes192_crypto,    SWITCH_STATUS_SUCCESS, ff_raw,       38 },
				{ "AES-256 keysalt (46-byte)",     aes256_crypto,    SWITCH_STATUS_SUCCESS, ff_raw,       46 },
				{ "keysalt too short for AES-256", aes256_short,     SWITCH_STATUS_FALSE,   NULL,         0  },
				{ "zero-length keysalt (space)",   zero_len_space,   SWITCH_STATUS_FALSE,   NULL,         0  },
				{ "zero-length keysalt (pipe)",    zero_len_pipe,    SWITCH_STATUS_FALSE,   NULL,         0  },
				{ "short keysalt",                 short_keysalt,    SWITCH_STATUS_FALSE,   NULL,         0  },
				{ "over-long keysalt",             overlong_keysalt, SWITCH_STATUS_FALSE,   NULL,         0  },
				{ "zero-length, long tail",        zero_len_tail,    SWITCH_STATUS_FALSE,   NULL,         0  },
			};

			memset(ff_raw, 0xFF, sizeof(ff_raw));

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			if (!session) {
				fst_fail("failed to originate session");
				goto add_crypto_bounds_done;
			}
			fst_xcheck(status == SWITCH_STATUS_SUCCESS, "originate must succeed");

			for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
				memset(&ssec, 0, sizeof(ssec));
				ssec.remote_crypto_key = cases[i].crypto;

				status = switch_core_media_add_crypto(session, &ssec, SWITCH_RTP_CRYPTO_RECV);
				fst_xcheck(status == cases[i].expect,
					switch_core_sprintf(fst_pool, "add_crypto(%s): unexpected status", cases[i].desc));

				if (cases[i].raw) {
					fst_xcheck(memcmp(ssec.remote_raw_key, cases[i].raw, cases[i].raw_len) == 0,
						switch_core_sprintf(fst_pool, "add_crypto(%s): decoded key+salt mismatch", cases[i].desc));
				}
			}

		add_crypto_bounds_done:
			if (session) {
				switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
				switch_core_session_rwunlock(session);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_opus_offer_negotiation)
		{
			/* Opus offers against stock, multi-rate, 16k-only and 8k-only preferences in each negotiation mode.
			   Opus must win over PCMU whenever any Opus implementation is configured (GH #2226): the one
			   configured at the offered rate, otherwise the first configured, moved down to the offered rate
			   when the offer caps it lower and an implementation exists there. Scrooge keeps the configured
			   implementation. */
			const char *stock = "OPUS,PCMU,PCMA";
			const char *multi = "OPUS@16000h,OPUS@8000h,OPUS,PCMU,PCMA";
			const char *only16k = "OPUS@16000h,PCMU,PCMA";
			const char *only8k = "OPUS@8000h,PCMU";
			struct {
				const char *desc;
				const char *prefs;
				const char *mode;
				const char *offer;
				uint8_t match;
				const char *codec;
				int rate;
			} cases[] = {
				{ "stock prefs, 16k Opus",              stock,   "generous", OFFER_OPUS_16K,         1, "opus", 16000 },
				{ "stock prefs, 16k Opus",              stock,   "greedy",   OFFER_OPUS_16K,         1, "opus", 16000 },
				{ "stock prefs, 16k Opus",              stock,   "scrooge",  OFFER_OPUS_16K,         1, "opus", 48000 },
				{ "stock prefs, 16k Opus + PCMU",       stock,   "generous", OFFER_OPUS_16K_PCMU,    1, "opus", 16000 },
				{ "stock prefs, 16k Opus + PCMU",       stock,   "greedy",   OFFER_OPUS_16K_PCMU,    1, "opus", 16000 },
				{ "stock prefs, 16k Opus + PCMU",       stock,   "scrooge",  OFFER_OPUS_16K_PCMU,    1, "opus", 48000 },
				{ "stock prefs, 8k Opus",               stock,   "generous", OFFER_OPUS_8K,          1, "opus", 8000 },
				{ "stock prefs, 24k Opus",              stock,   "generous", OFFER_OPUS_24K,         1, "opus", 48000 },
				{ "stock prefs, plain Opus",            stock,   "generous", OFFER_OPUS_PLAIN,       1, "opus", 48000 },
				{ "multi prefs, 16k Opus",              multi,   "generous", OFFER_OPUS_16K,         1, "opus", 16000 },
				{ "multi prefs, 16k Opus, ptime 40",    multi,   "generous", OFFER_OPUS_16K_PTIME40, 1, "opus", 16000 },
				{ "multi prefs, 16k Opus + PCMU",       multi,   "generous", OFFER_OPUS_16K_PCMU,    1, "opus", 16000 },
				{ "multi prefs, 16k Opus + PCMU",       multi,   "greedy",   OFFER_OPUS_16K_PCMU,    1, "opus", 16000 },
				{ "multi prefs, 16k Opus + PCMU",       multi,   "scrooge",  OFFER_OPUS_16K_PCMU,    1, "opus", 16000 },
				{ "multi prefs, PCMU + 16k Opus",       multi,   "generous", OFFER_PCMU_OPUS_16K,    1, "PCMU", 8000 },
				{ "multi prefs, PCMU + 16k Opus",       multi,   "greedy",   OFFER_PCMU_OPUS_16K,    1, "opus", 16000 },
				{ "multi prefs, 8k Opus",               multi,   "generous", OFFER_OPUS_8K,          1, "opus", 8000 },
				{ "multi prefs, 24k Opus",              multi,   "generous", OFFER_OPUS_24K,         1, "opus", 16000 },
				{ "multi prefs, 24k Opus + PCMU",       multi,   "generous", OFFER_OPUS_24K_PCMU,    1, "opus", 16000 },
				{ "multi prefs, plain Opus + PCMU",     multi,   "generous", OFFER_OPUS_PLAIN_PCMU,  1, "opus", 48000 },
				{ "16k-only prefs, 16k Opus + PCMU",    only16k, "generous", OFFER_OPUS_16K_PCMU,    1, "opus", 16000 },
				{ "16k-only prefs, 8k Opus",            only16k, "generous", OFFER_OPUS_8K,          1, "opus", 8000 },
				{ "16k-only prefs, plain Opus",         only16k, "generous", OFFER_OPUS_PLAIN,       1, "opus", 16000 },
				{ "16k-only prefs, plain Opus + PCMU",  only16k, "generous", OFFER_OPUS_PLAIN_PCMU,  1, "opus", 16000 },
				{ "16k-only prefs, plain Opus + PCMU",  only16k, "greedy",   OFFER_OPUS_PLAIN_PCMU,  1, "opus", 16000 },
				{ "16k-only prefs, WebRTC Opus + PCMU", only16k, "generous", OFFER_OPUS_WEBRTC_PCMU, 1, "opus", 16000 },
				{ "8k-only prefs, 16k Opus",            only8k,  "generous", OFFER_OPUS_16K,         1, "opus", 8000 },
				{ "8k-only prefs, 16k Opus + PCMU",     only8k,  "generous", OFFER_OPUS_16K_PCMU,    1, "opus", 8000 },
				{ "8k-only prefs, 16k Opus + PCMU",     only8k,  "greedy",   OFFER_OPUS_16K_PCMU,    1, "opus", 8000 },
				{ "8k-only prefs, plain Opus",          only8k,  "generous", OFFER_OPUS_PLAIN,       1, "opus", 8000 },
				{ "8k-only prefs, plain Opus + PCMU",   only8k,  "generous", OFFER_OPUS_PLAIN_PCMU,  1, "opus", 8000 },
				{ "8k-only prefs, 8k Opus",             only8k,  "generous", OFFER_OPUS_8K,          1, "opus", 8000 },
				{ "PCMU-only prefs, 16k Opus + PCMU",   "PCMU",  "generous", OFFER_OPUS_16K_PCMU,    1, "PCMU", 8000 },
				{ "stock prefs, no common codec",       stock,   "generous", OFFER_ILBC,             0, NULL,   0 },
			};
			int i, sps = 1000;

			fst_requires_module("mod_opus");

			/* Each case originates a session and FreeSWITCH starts at most 30 a second by default, so raise
			   the limit; it takes effect at the next one-second refresh. */
			switch_core_session_ctl(SCSC_SPS, &sps);
			switch_yield(1100000);

			for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
				char codec[64];
				int rate;
				uint8_t match = negotiate_audio_offer(cases[i].prefs, cases[i].mode, cases[i].offer,
													  codec, sizeof(codec), &rate);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opus offer [%s, %s]: match=%d %s/%d\n",
								  cases[i].desc, cases[i].mode, match, codec, rate);

				fst_xcheck(match == cases[i].match,
					switch_core_sprintf(fst_pool, "%s (%s): match=%d, expected %d",
										cases[i].desc, cases[i].mode, match, cases[i].match));

				if (match && cases[i].codec) {
					fst_xcheck(!strcasecmp(codec, cases[i].codec) && rate == cases[i].rate,
						switch_core_sprintf(fst_pool, "%s (%s): got %s/%d, expected %s/%d",
											cases[i].desc, cases[i].mode, codec, rate, cases[i].codec, cases[i].rate));
				}
			}
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
