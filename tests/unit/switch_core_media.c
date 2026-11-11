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
	}
	FST_SUITE_END()
}
FST_CORE_END()
